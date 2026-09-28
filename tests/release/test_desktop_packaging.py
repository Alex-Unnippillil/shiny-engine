# SPDX-License-Identifier: MIT
import sys
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts'))
import desktop_packaging as p


class PackagingTests(unittest.TestCase):
    def test_observed_runtime(self):
        self.assertEqual(p.ffmpeg_version('qt.multimedia.ffmpeg: Using Qt multimedia with FFmpeg version 7.1.5 LGPL'), '7.1.5')
        self.assertIsNone(p.ffmpeg_version('GStreamer backend'))
        with self.assertRaises(RuntimeError):
            p.ffmpeg_version('Using Qt multimedia with FFmpeg version 7.1.3\nUsing Qt multimedia with FFmpeg version 7.1.5')

    @patch.object(p.time, 'sleep')
    @patch.object(p.subprocess, 'run')
    def test_only_resource_busy_retries(self, run, sleep):
        run.side_effect = [None, SimpleNamespace(returncode=1, stderr='hdiutil: create failed - Resource busy'),
                           None, SimpleNamespace(returncode=0, stderr='')]
        p.create_dmg(Path('stage'), Path('output.dmg'))
        self.assertEqual(run.call_count, 4)
        sleep.assert_called_once_with(2)

    @patch.object(p.time, 'sleep')
    @patch.object(p.subprocess, 'run')
    def test_other_failure_is_not_retried(self, run, sleep):
        run.side_effect = [None, SimpleNamespace(returncode=1, stderr='Permission denied')]
        with self.assertRaises(RuntimeError): p.create_dmg(Path('stage'), Path('output.dmg'))
        self.assertEqual(run.call_count, 2)
        sleep.assert_not_called()

    @patch.object(p.time, 'sleep')
    @patch.object(p.subprocess, 'run')
    def test_retries_are_bounded(self, run, sleep):
        run.side_effect = [None, SimpleNamespace(returncode=1, stderr='Resource busy')] * 3
        with self.assertRaises(RuntimeError): p.create_dmg(Path('stage'), Path('output.dmg'))
        self.assertEqual(run.call_count, 6)
        self.assertEqual(sleep.call_count, 2)

    @patch.object(p.subprocess, 'run')
    def test_success_no_retry(self, run):
        run.side_effect = [None, SimpleNamespace(returncode=0, stderr='')]
        p.create_dmg(Path('stage'), Path('output.dmg'))
        self.assertEqual(run.call_count, 2)


if __name__ == '__main__': unittest.main()
