# SPDX-License-Identifier: MIT
"""Collect dependency notices, source-access information and a payload inventory."""
from __future__ import annotations
import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess
import sys


def collect(root: Path, output: Path, ffmpeg_runtime: str | None = None) -> dict:
    output.mkdir(parents=True, exist_ok=True)
    records = []
    if sys.platform.startswith('linux'):
        names = ['qt6-base-dev', 'qt6-multimedia-dev', 'libqt6core6t64', 'libqt6gui6',
                 'libqt6widgets6', 'libqt6multimedia6', 'libqt6multimediawidgets6',
                 'gstreamer1.0-plugins-good', 'gstreamer1.0-libav']
        for name in names:
            result = subprocess.run(['dpkg-query', '-W', '-f=${Package} ${Version}', name],
                                    capture_output=True, text=True, check=False)
            if result.returncode == 0:
                records.append({'package': name, 'installedVersion': result.stdout.strip(), 'distribution': 'Ubuntu 24.04'})
                copyright_file = Path('/usr/share/doc') / name / 'copyright'
                if copyright_file.is_file():
                    shutil.copyfile(copyright_file, output / (name + '-copyright.txt'))
        for name in ['LGPL-3', 'LGPL-2.1', 'GPL-3']:
            shutil.copyfile(Path('/usr/share/common-licenses') / name, output / (name + '.txt'))
        sources = ['https://packages.ubuntu.com/noble/qt6-base-dev', 'https://packages.ubuntu.com/noble/qt6-multimedia-dev']
    else:
        if not ffmpeg_runtime or not re.fullmatch(r'7\.1\.[0-9]+', ffmpeg_runtime):
            raise RuntimeError('Review corresponding source for this FFmpeg runtime before packaging')
        source_root = root / '.qt-source'
        for module in ['qtbase', 'qtmultimedia', 'qtsvg']:
            candidates = [p.parent for p in source_root.rglob('CMakeLists.txt') if p.parent.name == module]
            if len(candidates) != 1:
                raise RuntimeError('Missing unambiguous corresponding Qt source: ' + module)
            folder = candidates[0]
            count = 0
            for file in folder.rglob('*'):
                name = file.name.lower()
                relative = file.relative_to(folder)
                if relative.parts[0] in {'tests', 'examples'} or file.suffix.lower() in {'.cpp', '.h', '.in', '.pro', '.qrc', '.qdoc', '.ini'}:
                    continue
                if not file.is_file() or file.is_symlink() or file.stat().st_size > 1024 * 1024:
                    continue
                if not (name.startswith(('license', 'copying', 'copyright')) or
                        'LICENSES' in file.relative_to(folder).parts or name == 'qt_attributions.json'):
                    continue
                # Notices only: no fonts, source fixtures or executable payloads.
                data = file.read_bytes()
                try: data.decode('utf-8')
                except UnicodeDecodeError: continue
                if b'\x00' in data: continue
                target = output / module / file.relative_to(folder)
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data); count += 1
            if count < 5: raise RuntimeError('Qt dependency notices are incomplete')
            records.append({'package': module, 'version': '6.11.2', 'noticeFiles': count, 'linkage': 'shared'})
        if not list(output.rglob('*LGPL-3*')):
            raise RuntimeError('Full LGPLv3 license text is required')
        sources = [f'https://download.qt.io/official_releases/qt/6.11/6.11.2/submodules/{m}-everywhere-src-6.11.2.tar.xz'
                   for m in ['qtbase', 'qtmultimedia', 'qtsvg']]
        sources += ['https://code.qt.io/cgit/qt/qtmultimedia.git/tree/src/3rdparty/ffmpeg?h=v6.11.2',
                    f'https://ffmpeg.org/releases/ffmpeg-{ffmpeg_runtime}.tar.xz']
        records.append({'package': 'FFmpeg', 'version': ffmpeg_runtime, 'versionEvidence': 'Qt playback backend diagnostic from this actual native run', 'configuration': 'Qt official shared binaries; see qtmultimedia FFmpeg build scripts and attributions'})
    info = {'schema': 1, 'dependencies': records, 'correspondingSourceAccess': sources,
            'relinking': 'Qt is dynamically linked. Application source and build instructions accompany the release; compatible modified libraries are permitted. No library hash lock is imposed by this edition.',
            'scope': 'Build dependency record and source-access guidance, not a complete vulnerability or patent audit.'}
    (output / 'source-access.json').write_text(json.dumps(info, indent=2) + '\n')
    return info


def inventory(stage: Path, output: Path) -> None:
    records = []
    if sys.platform == 'win32':
        for name in ('msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll'):
            if not (stage / 'bin' / name).is_file():
                raise RuntimeError('Required app-local compiler runtime missing: ' + name)
        if (stage / 'bin/opengl32sw.dll').exists():
            raise RuntimeError('Unselected software OpenGL runtime must not be packaged')
    for p in sorted(stage.rglob('*')):
        if p.is_file() and not p.is_symlink():
            if p.suffix.lower() in {'.ttf', '.otf', '.woff', '.woff2'}:
                raise RuntimeError('Do not bundle font files')
            with p.open('rb') as stream:
                digest = hashlib.file_digest(stream, 'sha256').hexdigest()
            records.append({'path': p.relative_to(stage).as_posix(), 'bytes': p.stat().st_size, 'sha256': digest})
    output.write_text(json.dumps({'schema': 1, 'files': records}, indent=2) + '\n')
