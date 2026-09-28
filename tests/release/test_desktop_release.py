# SPDX-License-Identifier: MIT
"""Synthetic metadata fixtures: not native playback evidence."""
import importlib.util
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import release_desktop as release

SHA, TREE = 'a' * 40, 'b' * 40
RUN = {'id': 42, 'run_attempt': 3}


class DesktopReleaseTests(unittest.TestCase):
    def fixture(self, folder):
        base = 'ShinyDesktop-0.1.0-Linux-x64'
        (folder / (base + '.deb')).write_bytes(b'!<arch>\nsynthetic')
        (folder / (base + '.tar.gz')).write_bytes(b'synthetic test archive')
        (folder / (base + '-Source.zip')).write_bytes(b'synthetic test source')
        (folder / 'README.md').write_text('Synthetic test fixture')
        (folder / 'THIRD_PARTY_NOTICES.md').write_text('Synthetic test notices')
        (folder / 'ctest.xml').write_text('<testsuite tests="1" failures="0" errors="0"/>')
        info = {'schema': 1, 'version': '0.1.0', 'platform': 'Linux-x64', 'sourceSha': SHA, 'sourceTree': TREE,
                'runId': 42, 'runAttempt': 3, 'notarized': False, 'signedByPublisher': False,
                'installedPlaybackVerified': True, 'uninstallVerified': True}
        (folder / 'build-info.json').write_text(json.dumps(info))
        (folder / 'dependencies.json').write_text(json.dumps({'dependencies': ['synthetic'], 'correspondingSourceAccess': ['synthetic']}))
        (folder / 'installed-payload.json').write_text(json.dumps({'files': [{}, {}, {}, {}]}))
        for mode in ('build-smoke', 'installed-smoke'):
            d = folder / mode; d.mkdir()
            playback = {'version': '0.1.0', 'passed': True, 'deliveredVideoFrames': 9,
                        'pauseVerified': True, 'seekVerified': True, 'stopVerified': True, 'dlss': False}
            (d / 'playback.json').write_text(json.dumps(playback))
            for n in ('desktop.png', 'decoded-frame.png'): (d / n).write_bytes(b'\x89PNG\r\n\x1a\nsynthetic')
        self.hashes(folder)

    def hashes(self, folder):
        (folder / 'SHA256SUMS.txt').write_text(''.join(
            hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.relative_to(folder).as_posix() + '\n'
            for p in sorted(folder.rglob('*')) if p.is_file() and p.name != 'SHA256SUMS.txt'))

    def validate(self, folder):
        return release.validate(folder, 'Linux-x64', SHA, TREE, RUN)

    def test_complete_metadata(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp); self.fixture(p); self.assertEqual(len(self.validate(p)), 3)

    def test_modified_asset(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp); self.fixture(p); (p / 'README.md').write_text('tampered')
            with self.assertRaises(release.existing.ReleaseError): self.validate(p)

    def test_wrong_revision_and_attempt(self):
        for key, value in [('sourceSha', 'c' * 40), ('sourceTree', 'd' * 40), ('runAttempt', 2), ('notarized', True)]:
            with self.subTest(key=key), tempfile.TemporaryDirectory() as tmp:
                p = Path(tmp); self.fixture(p); info = json.loads((p / 'build-info.json').read_text()); info[key] = value
                (p / 'build-info.json').write_text(json.dumps(info)); self.hashes(p)
                with self.assertRaises(release.existing.ReleaseError): self.validate(p)

    def test_no_native_playback(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp); self.fixture(p)
            report = p / 'installed-smoke/playback.json'; data = json.loads(report.read_text()); data['deliveredVideoFrames'] = 0
            report.write_text(json.dumps(data)); self.hashes(p)
            with self.assertRaises(release.existing.ReleaseError): self.validate(p)

    def test_incomplete_hash_coverage(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp); self.fixture(p); (p / 'unexpected.dll').write_bytes(b'test')
            with self.assertRaises(release.existing.ReleaseError): self.validate(p)

    def test_failed_native_test(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp); self.fixture(p); (p / 'ctest.xml').write_text('<testsuite tests="1" failures="1"/>'); self.hashes(p)
            with self.assertRaises(release.existing.ReleaseError): self.validate(p)

    def test_archive_path_escape(self):
        for name in ('../escape', '/absolute', 'C:/absolute', 'a\\b'):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp); archive = root / 'test.zip'
                with ZipFile(archive, 'w') as z: z.writestr(name, 'test')
                with self.assertRaises(release.existing.ReleaseError): release.unpack(archive, root / 'out')

    def test_archive_case_collision(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); archive = root / 'test.zip'
            with ZipFile(archive, 'w') as z: z.writestr('File.txt', 'a'); z.writestr('file.txt', 'b')
            with self.assertRaises(release.existing.ReleaseError): release.unpack(archive, root / 'out')

    def test_all_old_and_new_checks_required(self):
        self.assertEqual(len(release.REQUIRED), 9)
        for target in release.TARGETS: self.assertIn(f'desktop ({target})', release.REQUIRED)
        self.assertIn('vlc-player', release.REQUIRED)


if __name__ == '__main__': unittest.main()
