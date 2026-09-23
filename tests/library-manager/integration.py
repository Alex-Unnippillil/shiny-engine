# SPDX-License-Identifier: MIT
"""Windows tests. PE fixtures are synthetic, not vendor libraries."""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

EXE = Path(sys.argv.pop(1)).resolve()
CANARY = Path(sys.argv.pop(1)).resolve()


def pe(machine=0x8664, dll=True):
    data = bytearray(1024)
    struct.pack_into('<H', data, 0, 0x5a4d)
    struct.pack_into('<I', data, 60, 128)
    struct.pack_into('<IHH', data, 128, 0x4550, machine, 1)
    struct.pack_into('<HHH', data, 148, 240, 2 | (0x2000 if dll else 0), 0x20b)
    struct.pack_into('<I', data, 212, 512)
    struct.pack_into('<II', data, 408, 512, 512)
    return data


class AuditTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='shiny audit-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve() / 'VLC local é'
        self.root.mkdir()
        self.file = self.root / 'nvngx_dlss.dll'
        self.file.write_bytes(pe())

    def run_cli(self, *args, expected=0):
        result = subprocess.run([str(EXE), *map(str, args)], capture_output=True,
                                text=True, encoding='utf-8', timeout=20, check=False)
        self.assertEqual(result.returncode, expected, result.stderr)
        self.assertNotIn(str(self.root), result.stdout + result.stderr)
        self.assertNotIn(str(self.root).replace('\\', '\\\\'), result.stdout + result.stderr)
        return json.loads(result.stdout if expected == 0 else result.stderr)

    def candidate(self):
        return self.run_cli('--candidate', self.file)['files'][0]

    def test_fingerprint(self):
        row = self.candidate()
        self.assertEqual(row['sha256'], hashlib.sha256(self.file.read_bytes()).hexdigest())
        self.assertEqual(row['architecture'], 'x64')
        self.assertFalse(row['activationAllowed'])
        self.assertFalse(row['swapAllowed'])
        self.assertEqual(row['status'], 'inspected-not-approved')

    def test_never_executes_candidate(self):
        shutil.copyfile(CANARY, self.file)
        self.assertEqual(self.candidate()['status'], 'inspected-not-approved')

    def test_no_installation_writes(self):
        before = self.file.read_bytes()
        names = sorted(p.name for p in self.root.iterdir())
        self.candidate()
        self.assertEqual(self.file.read_bytes(), before)
        self.assertEqual(sorted(p.name for p in self.root.iterdir()), names)

    def test_missing(self):
        self.file.unlink()
        self.assertEqual(self.candidate()['status'], 'missing')

    def test_malformed(self):
        self.file.write_bytes(b'not a library')
        self.assertFalse(self.candidate()['peMetadataParsed'])

    def test_empty(self):
        self.file.write_bytes(b'')
        self.assertEqual(self.candidate()['status'], 'file-size-limit')

    def test_oversized(self):
        with self.file.open('wb') as handle:
            handle.truncate(128 * 1024 * 1024 + 1)
        self.assertEqual(self.candidate()['status'], 'file-size-limit')

    def test_architecture_mismatch(self):
        self.file.write_bytes(pe(machine=0xaa64))
        self.assertEqual(self.candidate()['status'], 'architecture-mismatch')

    def test_wrong_image_kind(self):
        self.file.write_bytes(pe(dll=False))
        self.assertEqual(self.candidate()['status'], 'wrong-image-kind')

    def test_no_recursive_discovery(self):
        sub = self.root / 'other'
        sub.mkdir()
        self.file.rename(sub / self.file.name)
        report = self.run_cli('--vlc-dir', self.root)
        self.assertEqual(len(report['files']), 12)
        row = next(r for r in report['files'] if r['slot'] == 'nvngx_dlss.dll')
        self.assertEqual(row['status'], 'missing')
        self.assertFalse(report['dlssBackendAvailable'])

    def test_layout_does_not_establish_compatibility(self):
        for name in ['vlc.exe', 'libvlc.dll', 'libvlccore.dll']:
            (self.root / name).write_bytes(pe(dll=name != 'vlc.exe'))
        report = self.run_cli('--vlc-dir', self.root)
        self.assertTrue(report['vlcLayoutDetected'])
        self.assertEqual(report['runtimeCompatibility'], 'not-established')
        self.assertFalse(report['swapSupported'])
        self.assertFalse(report['activationSupported'])

    def test_candidate_does_not_establish_vlc_layout(self):
        self.assertFalse(self.run_cli('--candidate', self.file)['vlcLayoutDetected'])

    def test_unknown_filename(self):
        self.assertEqual(self.run_cli('--candidate', self.root / 'dxgi.dll', expected=2)['error'],
                         'unrecognized-library-name')

    def test_unknown_command(self):
        self.assertEqual(self.run_cli('--swap', self.file, expected=2)['error'],
                         'unsupported-command-see-help')

    def test_missing_argument(self):
        self.run_cli('--candidate', expected=2)

    def test_extra_argument(self):
        self.run_cli('--candidate', self.file, 'extra', expected=2)

    def test_relative(self):
        row = self.run_cli('--candidate', 'nvngx_dlss.dll')['files'][0]
        self.assertEqual(row['status'], 'absolute-local-drive-path-required')

    def test_device_path(self):
        row = self.run_cli('--candidate', '\\\\?\\' + str(self.file))['files'][0]
        self.assertEqual(row['status'], 'absolute-local-drive-path-required')

    def test_unc_path(self):
        row = self.run_cli('--candidate', r'\\server\share\nvngx_dlss.dll')['files'][0]
        self.assertEqual(row['status'], 'absolute-local-drive-path-required')

    def test_parent_traversal(self):
        row = self.run_cli('--candidate', self.root / '..' / self.root.name / self.file.name)['files'][0]
        self.assertEqual(row['status'], 'ambiguous-path-not-supported')

    def test_directory_not_file(self):
        self.file.unlink()
        self.file.mkdir()
        self.assertEqual(self.candidate()['status'], 'regular-file-required')

    def test_hardlink(self):
        os.link(self.file, self.root / 'alias.dll')
        self.assertEqual(self.candidate()['status'], 'hardlink-rejected')

    def test_file_is_not_vlc_directory(self):
        self.run_cli('--vlc-dir', self.file, expected=2)

    def test_signature_not_approval(self):
        row = self.candidate()
        self.assertTrue(row['authenticode'] == 'unsigned' or row['authenticode'].startswith('unverified-'))
        self.assertEqual(row['fileVersionUntrusted'], 'unavailable')
        self.assertFalse(row['activationAllowed'])

    def test_junction_rejected(self):
        junction = self.root.parent / 'redirected'
        result = subprocess.run(['cmd.exe', '/d', '/c', 'mklink', '/J', str(junction), str(self.root)],
                                capture_output=True, timeout=10, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.addCleanup(lambda: os.rmdir(junction))
        report = self.run_cli('--vlc-dir', junction, expected=2)
        self.assertEqual(report['error'], 'reparse-point-rejected')

    def test_write_locked(self):
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_ulong, ctypes.c_ulong,
                                       ctypes.c_void_p, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_void_p]
        kernel.CreateFileW.restype = ctypes.c_void_p
        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
        handle = kernel.CreateFileW(str(self.file), 0x40000000, 0, None, 3, 0x80, None)
        self.assertNotEqual(handle, ctypes.c_void_p(-1).value)
        try:
            self.assertEqual(self.candidate()['status'], 'unreadable-or-write-locked')
        finally:
            kernel.CloseHandle(handle)

    def test_ads_directory_rejected(self):
        report = self.run_cli('--vlc-dir', str(self.root) + ':stream', expected=2)
        self.assertEqual(report['error'], 'absolute-local-drive-path-required')

    def test_ambiguous_directory_rejected(self):
        report = self.run_cli('--vlc-dir', str(self.root) + ' ', expected=2)
        self.assertEqual(report['error'], 'ambiguous-path-not-supported')

    def test_uppercase_filename(self):
        upper = self.root / 'NVNGX_DLSS.DLL'
        self.assertEqual(self.run_cli('--candidate', upper)['files'][0]['family'], 'dlss-super-resolution')


if __name__ == '__main__':
    unittest.main()
