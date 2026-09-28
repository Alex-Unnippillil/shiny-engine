#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Native-host build/package/installation smoke checks; never cross-compile and assume success."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
VERSION = '0.1.0'
QT_VERSION = '6.11.2'

from desktop_notices import collect, inventory
from desktop_packaging import create_dmg, ffmpeg_version


def run(*args: object, cwd: Path = ROOT, env: dict | None = None, timeout: int = 900) -> None:
    subprocess.run([str(a) for a in args], cwd=cwd, env=env, check=True, timeout=timeout)


def sha(path: Path) -> str:
    with path.open('rb') as handle:
        return hashlib.file_digest(handle, 'sha256').hexdigest()


def smoke(executable: Path, fixture: Path, output: Path, env: dict) -> dict:
    output.mkdir(parents=True, exist_ok=True)
    command = [executable, '--smoke', fixture, output]
    if sys.platform.startswith('linux'):
        command = ['xvfb-run', '-a', '-s', '-screen 0 1280x900x24', *command]
    completed = subprocess.run([str(a) for a in command], cwd=ROOT, env=env, capture_output=True,
                               text=True, encoding='utf-8', errors='replace', timeout=60)
    if completed.returncode:
        print(completed.stdout, completed.stderr)
        completed.check_returncode()
    backend_version = ffmpeg_version(completed.stderr)
    if not sys.platform.startswith('linux') and backend_version is None:
        raise RuntimeError('The packaged FFmpeg runtime version was not reported')
    data = json.loads((output / 'playback.json').read_text())
    if not (data['passed'] is True and data['deliveredVideoFrames'] >= 8 and data['pauseVerified']
            and data['seekVerified'] and data['stopVerified'] and data['dlss'] is False):
        raise RuntimeError('Native playback evidence incomplete')
    for name in ['desktop.png', 'decoded-frame.png']:
        if not (output / name).read_bytes().startswith(b'\x89PNG\r\n\x1a\n'):
            raise RuntimeError('Missing native screenshot/frame evidence')
    data['ffmpegRuntimeVersion'] = backend_version
    (output / 'playback.json').write_text(json.dumps(data, indent=2) + '\n')
    return data


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--platform', required=True, choices=['Windows-x64', 'macOS-arm64', 'macOS-x64', 'Linux-x64'])
    args = parser.parse_args()
    target = args.platform
    machine = platform.machine().lower()
    if ('arm64' in target) != (machine in {'arm64', 'aarch64'}):
        raise RuntimeError('Runner architecture does not match the package label')
    build = ROOT / 'build/desktop'
    stage = ROOT / 'build/desktop-stage'
    artifacts = ROOT / 'artifacts/desktop'
    artifacts.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    prefix = None
    options = ['-DCMAKE_BUILD_TYPE=Release']
    if sys.platform == 'win32' or sys.platform == 'darwin':
        matches = list((ROOT / '.qt').rglob('Qt6Config.cmake'))
        if len(matches) != 1:
            raise RuntimeError('Expected exactly one downloaded Qt development kit')
        prefix = matches[0].parents[3]
        options.append(f'-DCMAKE_PREFIX_PATH={prefix}')
        env['PATH'] = str(prefix / 'bin') + os.pathsep + env['PATH']
        if sys.platform == 'win32':
            options += ['-A', 'x64']
        else:
            options += [f'-DCMAKE_OSX_ARCHITECTURES={machine}', '-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0']
    run('cmake', '-S', 'native/desktop', '-B', build, *options, env=env)
    run('cmake', '--build', build, '--config', 'Release', '--parallel', '3', env=env)
    run('ctest', '--test-dir', build, '-C', 'Release', '--output-on-failure', '--output-junit', artifacts / 'ctest.xml', env=env)
    fixture_dir = ROOT / 'build/desktop-fixture'
    run(sys.executable, 'tests/vlc-player/make_fixture.py', fixture_dir)
    fixture = fixture_dir / 'moving-original.avi'
    executable = build / 'ShinyDesktop'
    if sys.platform == 'win32': executable = build / 'Release/ShinyDesktop.exe'
    if sys.platform == 'darwin': executable = build / 'ShinyDesktop.app/Contents/MacOS/ShinyDesktop'
    build_evidence = smoke(executable, fixture, artifacts / 'build-smoke', env)
    dependency_info = collect(ROOT, build / 'notices', build_evidence['ffmpegRuntimeVersion'])
    run('cmake', '--install', build, '--config', 'Release', '--prefix', stage, env=env)
    base = f'ShinyDesktop-{VERSION}-{target}'
    runtime_env = env.copy()
    # Run deployed binaries without finding Qt through the developer PATH.
    if prefix:
        runtime_env['PATH'] = os.pathsep.join(p for p in os.environ['PATH'].split(os.pathsep) if str(prefix).lower() not in p.lower())
    for key in ['QT_PLUGIN_PATH', 'QML2_IMPORT_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH']:
        runtime_env.pop(key, None)
    if sys.platform == 'win32':
        inventory(stage, artifacts / 'installed-payload.json')
        run('cmake', '-E', 'tar', 'cf', artifacts / (base + '-Portable.zip'), '--format=zip', '.', cwd=stage)
        iscc = Path(r'C:\Program Files (x86)\Inno Setup 6\ISCC.exe')
        run(iscc, '/Qp', f'/DPackageDir={stage}', f'/DOutputDir={artifacts}', 'installers/desktop/windows.iss')
        installer = artifacts / (base + '-Setup.exe')
        with tempfile.TemporaryDirectory(prefix='shiny-installed-') as tmp:
            installed = Path(tmp) / 'app'
            run(installer, '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', f'/DIR={installed}')
            smoke(installed / 'bin/ShinyDesktop.exe', fixture, artifacts / 'installed-smoke', runtime_env)
            run(installed / 'unins000.exe', '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART')
            for _ in range(30):
                if not (installed / 'bin/ShinyDesktop.exe').exists(): break
                time.sleep(0.2)
            if (installed / 'bin/ShinyDesktop.exe').exists(): raise RuntimeError('Uninstall left the application binary')
    elif sys.platform == 'darwin':
        bundle = stage / 'ShinyDesktop.app'
        # Ad-hoc integrity signature only; not a Developer ID or notarization claim.
        run('codesign', '--force', '--deep', '--sign', '-', bundle)
        run('codesign', '--verify', '--deep', '--strict', bundle)
        inventory(stage, artifacts / 'installed-payload.json')
        os.symlink('/Applications', stage / 'Applications')
        dmg = artifacts / (base + '.dmg')
        create_dmg(stage, dmg)
        with tempfile.TemporaryDirectory(prefix='shiny-volume-') as tmp:
            mount = Path(tmp) / 'mounted'
            run('hdiutil', 'attach', dmg, '-mountpoint', mount, '-nobrowse', '-readonly')
            try:
                installed = Path(tmp) / 'installed/ShinyDesktop.app'
                installed.parent.mkdir()
                run('ditto', mount / 'ShinyDesktop.app', installed)
                smoke(installed / 'Contents/MacOS/ShinyDesktop', fixture, artifacts / 'installed-smoke', runtime_env)
                shutil.rmtree(installed)
                if installed.exists(): raise RuntimeError('App removal failed')
            finally:
                run('hdiutil', 'detach', mount)
    else:
        inventory(stage, artifacts / 'installed-payload.json')
        packaging = ROOT / 'build/desktop-packages'
        run('cpack', '--config', build / 'CPackConfig.cmake', '-B', packaging, env=env)
        for name in [base + '.deb', base + '.tar.gz']:
            shutil.copyfile(packaging / name, artifacts / name)
        package = artifacts / (base + '.deb')
        run('sudo', 'apt-get', 'install', '-y', package)
        try:
            smoke(Path('/usr/bin/ShinyDesktop'), fixture, artifacts / 'installed-smoke', runtime_env)
        finally:
            run('sudo', 'apt-get', 'remove', '-y', 'shiny-desktop')
        if Path('/usr/bin/ShinyDesktop').exists(): raise RuntimeError('DEB removal failed')
    # Actual build identity, no machine/user paths in the distributable report.
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    tree = subprocess.check_output(['git', 'rev-parse', 'HEAD^{tree}'], cwd=ROOT, text=True).strip()
    report = {'schema': 1, 'version': VERSION, 'platform': target, 'sourceSha': commit, 'sourceTree': tree,
              'runId': int(os.environ.get('GITHUB_RUN_ID', 0)), 'runAttempt': int(os.environ.get('GITHUB_RUN_ATTEMPT', 0)),
              'signedByPublisher': False, 'notarized': False, 'installedPlaybackVerified': True, 'uninstallVerified': True,
              'qt': json.loads((artifacts / 'installed-smoke/playback.json').read_text())['qt'],
              'linuxDependencies': 'OS-managed Qt/FFmpeg/GStreamer (not a universal Linux binary)' if sys.platform.startswith('linux') else None}
    (artifacts / 'build-info.json').write_text(json.dumps(report, indent=2) + '\n')
    (artifacts / 'dependencies.json').write_text(json.dumps(dependency_info, indent=2) + '\n')
    run('git', 'archive', '--format=zip', '-o', artifacts / (base + '-Source.zip'), 'HEAD', 'native/desktop', 'installers/desktop', 'scripts/desktop_build.py', 'scripts/desktop_notices.py', 'scripts/desktop_packaging.py', 'scripts/install_desktop_qt.py', 'tests/desktop', 'tests/vlc-player/make_fixture.py', 'LICENSE')
    shutil.copyfile(ROOT / 'native/desktop/README.md', artifacts / 'README.md')
    shutil.copyfile(ROOT / 'native/desktop/THIRD_PARTY_NOTICES.md', artifacts / 'THIRD_PARTY_NOTICES.md')
    payloads = sorted(p for p in artifacts.rglob('*') if p.is_file() and p.name != 'SHA256SUMS.txt')
    (artifacts / 'SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  {p.relative_to(artifacts).as_posix()}\n' for p in payloads))


if __name__ == '__main__':
    main()
