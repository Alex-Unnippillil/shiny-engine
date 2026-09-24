# SPDX-License-Identifier: MIT
"""Actual Windows processes: first-party DLLs, durable catalog, crashes and UI."""
import ctypes
from ctypes import wintypes as wt
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import unittest

CLI, FAULT, GUI = [Path(sys.argv.pop(1)).resolve() for _ in range(3)]
WORKER = CLI.parent / 'ShinyEnhancementWorker.exe'
BUNDLES = CLI.parent / 'library-bundles'
MAGIC = 0x534C504B
HEADER = struct.Struct('<6I2Qq')


class Manager(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='shiny manager-')
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name).resolve()
        self.root = self.base / 'store é'

    def cli(self, *args, code=0):
        p = subprocess.run([str(CLI), '--store-dir', str(self.root), *map(str, args)],
                           capture_output=True, text=True, encoding='utf-8', timeout=25)
        self.assertEqual(p.returncode, code, p.stderr)
        text = p.stdout if code == 0 else p.stderr
        self.assertNotIn(str(self.base), text)
        self.assertNotIn(str(self.base).replace('\\', '\\\\'), text)
        return json.loads(text)

    def bundled(self):
        r = self.cli('--import-bundled')
        return {p['version']: p['digest'] for p in r['packages']}

    def select(self, identity):
        self.cli('--stage', identity)
        return self.cli('--activate', identity)

    def fault(self, mode, arg, point):
        p = subprocess.run([str(FAULT), str(self.root), mode, str(arg), point],
                           capture_output=True, timeout=25)
        self.assertEqual(p.returncode, 86, (mode, point, p.stderr))

    def test_two_real_dll_versions_probe_switch_restore(self):
        ids = self.bundled()
        # Catalog rows are digest-sorted; build hashes have no version ordering.
        for version in ('1.0.0', '1.1.0'):
            identity = ids[version]
            self.assertTrue(self.cli('--probe', identity)['syntheticFrameProbePassed'])
            self.select(identity)
        self.assertEqual(self.cli('--list')['selected'], ids['1.1.0'])
        r = self.cli('--rollback')
        self.assertEqual(r['selected'], ids['1.0.0'])
        self.assertEqual(self.cli('--rollback')['selected'], ids['1.0.0'])
        self.assertEqual(self.cli('--original')['selected'], '')
        self.assertFalse(r['active'])
        self.assertFalse(r['dlssActivationSupported'])

    def test_stage_is_required(self):
        ids = self.bundled()
        self.assertEqual(self.cli('--activate', ids['1.0.0'], code=2)['error'], 'stage-package-first')

    def test_unapproved_candidate_never_executes(self):
        dll = self.base / 'nvngx_dlss.dll'
        shutil.copyfile(BUNDLES / '1.0.0/shiny_spatial.dll', dll)
        identity = self.cli('--quarantine-file', dll)['digest']
        self.assertEqual(self.cli('--verify', identity)['blockReason'], 'not-in-build-pinned-catalog')
        self.cli('--stage', identity, code=2)
        self.cli('--activate', identity, code=2)
        p = subprocess.run([str(WORKER), '--store-dir', str(self.root), '--package', identity],
                           input=b'', capture_output=True, timeout=15)
        self.assertNotEqual(p.returncode, 0)
        self.assertEqual(p.stdout, b'')
        self.assertEqual(self.cli('--list')['selected'], '')

    def test_integrity_rechecked_after_selection(self):
        ids = self.bundled()
        identity = ids['1.0.0']
        self.select(identity)
        dll = self.root / 'packages' / identity / 'shiny_spatial.dll'
        original = dll.read_bytes()
        dll.write_bytes(original + b'tampered')
        self.cli('--probe', identity, code=2)
        p = subprocess.run([str(WORKER), '--store-dir', str(self.root), '--package', identity],
                           input=b'', capture_output=True, timeout=15)
        self.assertEqual(p.stdout, b'')
        # Original remains available even after a candidate was damaged.
        self.assertEqual(self.cli('--original')['selected'], '')

    def test_selected_and_previous_protected(self):
        ids = self.bundled()
        for identity in ids.values():
            self.select(identity)
        for identity in ids.values():
            self.cli('--remove', identity, code=2)

    def test_completed_import_idempotent_and_report_redacted(self):
        a = self.bundled()
        self.assertEqual(a, self.bundled())
        self.assertEqual(len(self.cli('--list')['packages']), 2)
        self.assertTrue(self.cli('--history')['history'])

    def test_interrupted_imports_recover_after_process_death(self):
        for point in ('import-journal', 'import-directory', 'import-manifest',
                      'import-component', 'import-promoted', 'import-committed'):
            with self.subTest(point=point):
                self.root = self.base / point
                self.fault('import', BUNDLES / '1.0.0', point)
                self.cli('--recover')
                self.cli('--import-folder', BUNDLES / '1.0.0')
                r = self.cli('--list')
                self.assertEqual(len(r['packages']), 1)
                self.assertEqual(r['selected'], '')
                self.assertEqual(list((self.root / 'staging').iterdir()), [])

    def test_interrupted_selection_preserves_last_committed_selection(self):
        for point in ('selection-pending', 'selection-probed', 'selection-committed'):
            with self.subTest(point=point):
                self.root = self.base / point
                ids = self.bundled()
                self.select(ids['1.0.0'])
                self.cli('--stage', ids['1.1.0'])
                self.fault('activate', ids['1.1.0'], point)
                expected = ids['1.1.0'] if point == 'selection-committed' else ids['1.0.0']
                self.assertEqual(self.cli('--recover')['selected'], expected)

    def test_interrupted_delete_recovers(self):
        for point in ('delete-pending', 'delete-files'):
            with self.subTest(point=point):
                self.root = self.base / point
                ids = self.bundled()
                self.fault('remove', ids['1.0.0'], point)
                self.assertEqual(len(self.cli('--recover')['packages']), 1)

    def test_active_lease_blocks_removal(self):
        identity = self.bundled()['1.0.0']
        proc = subprocess.Popen([str(FAULT), str(self.root), 'lease', identity, '-'],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            self.assertEqual(proc.stdout.readline(), b'ready\r\n')
            self.cli('--remove', identity, code=2)
        finally:
            proc.kill()
            proc.communicate(timeout=10)
        self.cli('--remove', identity)

    def test_two_manager_processes_cannot_write_same_store(self):
        self.cli('--list')
        proc = subprocess.Popen([str(FAULT), str(self.root), 'lock', '-', '-'],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            self.assertEqual(proc.stdout.readline(), b'ready\r\n')
            self.assertEqual(self.cli('--import-bundled', code=2)['error'], 'store-busy')
        finally:
            proc.kill()
            proc.communicate(timeout=10)
        self.bundled()

    def test_worker_rejects_malformed_protocol_without_a_frame(self):
        identity = self.bundled()['1.0.0']
        for header in (HEADER.pack(0, 1, 1, 7, 5, 140, 1, 0, 0),
                       HEADER.pack(MAGIC, 1, 1, 0xffffffff, 5, 140, 1, 0, 0),
                       HEADER.pack(MAGIC, 1, 1, 7, 5, 0xffffffff, 1, 0, 0)):
            p = subprocess.run([str(WORKER), '--store-dir', str(self.root), '--package', identity],
                               input=header, capture_output=True, timeout=15)
            self.assertEqual(len(p.stdout), 80)
            self.assertNotEqual(p.returncode, 0)

    def test_worker_returns_same_frame_identity_and_preserves_alpha(self):
        identity = self.bundled()['1.0.0']
        data = bytes((x * 37 + y * 11 + c * 23) % 256 if c < 3 else (x+y)*7
                     for y in range(5) for x in range(7) for c in range(4))
        frame = HEADER.pack(MAGIC, 1, 1, 7, 5, 140, 10, 14, 9001)
        p = subprocess.run([str(WORKER), '--store-dir', str(self.root), '--package', identity],
                           input=frame+data, capture_output=True, timeout=15)
        self.assertEqual(HEADER.unpack(p.stdout[80:128]), (MAGIC, 1, 2, 7, 5, 140, 10, 14, 9001))
        self.assertEqual(p.stdout[128:][3::4], data[3::4])
        self.assertEqual(hashlib.sha256(p.stdout[128:]).hexdigest(),
                         'b086a1d7394bb19d3470c7ff4a8e479019e9a97e3ae1302a7b805e71fc7ec8d8')

    def test_symlink_hardlink_and_extra_file_are_rejected(self):
        folder = self.base / 'candidate'
        shutil.copytree(BUNDLES / '1.0.0', folder)
        (folder / 'extra.txt').write_text('unexpected')
        self.cli('--import-folder', folder, code=2)
        (folder / 'extra.txt').unlink()
        import os
        os.link(folder / 'shiny_spatial.dll', self.base / 'alias.dll')
        self.cli('--import-folder', folder, code=2)
        (self.base / 'alias.dll').unlink()
        junction = self.base / 'junction'
        p = subprocess.run(['cmd.exe','/d','/c','mklink','/J',str(junction),str(folder)], capture_output=True)
        self.assertEqual(p.returncode, 0)
        try:
            self.cli('--import-folder', junction, code=2)
        finally:
            os.rmdir(junction)

    def test_user_editable_manifest_cannot_grant_approval(self):
        folder = self.base / 'candidate'
        shutil.copytree(BUNDLES / '1.0.0', folder)
        p = folder / 'manifest.json'
        manifest = json.loads(p.read_text())
        manifest['version'] = '99.0.0'
        p.write_text(json.dumps(manifest))
        identity = self.cli('--import-folder', folder)['digest']
        self.cli('--stage', identity, code=2)
        manifest['approved'] = True
        p.write_text(json.dumps(manifest))
        self.cli('--import-folder', folder, code=2)

    def test_unknown_commands_and_paths(self):
        self.cli('--load-dll', 'anything', code=2)
        self.cli('--import-folder', 'relative', code=2)
        self.cli('--remove', '../escape', code=2)
        self.cli('--stage', 'g'*64, code=2)

    def test_ui_import_verify_stage_selection_and_original(self):
        user = ctypes.WinDLL('user32', use_last_error=True)
        user.FindWindowW.argtypes=[wt.LPCWSTR, wt.LPCWSTR];user.FindWindowW.restype=wt.HWND
        user.GetDlgItem.argtypes=[wt.HWND,ctypes.c_int];user.GetDlgItem.restype=wt.HWND
        user.SendMessageW.argtypes=[wt.HWND,wt.UINT,wt.WPARAM,wt.LPARAM];user.SendMessageW.restype=ctypes.c_ssize_t
        user.PostMessageW.argtypes=[wt.HWND,wt.UINT,wt.WPARAM,wt.LPARAM]
        user.IsWindowEnabled.argtypes=[wt.HWND];user.IsWindowEnabled.restype=wt.BOOL
        proc=subprocess.Popen([str(GUI),'--store-dir',str(self.root)])
        hwnd=None
        def wait(test):
            deadline=time.monotonic()+20
            while time.monotonic()<deadline:
                if test():return
                if proc.poll() is not None: self.fail('manager window exited')
                time.sleep(.05)
            self.fail('UI action timeout')
        try:
            wait(lambda:user.FindWindowW('ShinyLibraryManager',None))
            hwnd=user.FindWindowW('ShinyLibraryManager',None)
            wait(lambda:user.IsWindowEnabled(user.GetDlgItem(hwnd,201)))
            def action(identifier):
                user.SendMessageW(hwnd,0x111,identifier,0)
                wait(lambda:user.IsWindowEnabled(user.GetDlgItem(hwnd,211)))
            action(201)
            self.assertEqual(user.SendMessageW(user.GetDlgItem(hwnd,100),0x18B,0,0),2)
            action(204);action(205);action(206)
            self.assertNotEqual(self.cli('--list')['selected'],'')
            action(208)
            self.assertEqual(self.cli('--list')['selected'],'')
        finally:
            if hwnd:user.PostMessageW(hwnd,0x10,0,0)
            try:proc.wait(timeout=20)
            except subprocess.TimeoutExpired:proc.kill();proc.wait()
        self.assertEqual(proc.returncode,0)


if __name__ == '__main__':
    unittest.main(verbosity=2)
