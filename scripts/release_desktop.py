#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Publish a distinct desktop-v* prerelease only from all verified native targets.

This does not retag or replace the existing Windows libVLC player release.
Pure validation functions are covered by tests/release/test_desktop_release.py.
"""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import tempfile
from zipfile import ZipFile, ZIP_DEFLATED
import release_player as existing

VERSION = '0.1.0'
TARGETS = ('Windows-x64', 'macOS-arm64', 'macOS-x64', 'Linux-x64')
MAX_BYTES = 2 * 1024 ** 3
REQUIRED = {**existing.REQUIRED, **{f'desktop ({p})': '.github/workflows/desktop.yml' for p in TARGETS}}


def require(ok: bool, reason: str) -> None:
    if not ok: raise existing.ReleaseError(reason)


def unpack(archive: Path, destination: Path) -> None:
    with ZipFile(archive) as z:
        entries = z.infolist()
        require(len(entries) <= 12000 and sum(i.file_size for i in entries) <= MAX_BYTES, 'Archive bounds exceeded')
        seen = set()
        for i in entries:
            p = PurePosixPath(i.filename)
            require(p.parts and not p.is_absolute() and '..' not in p.parts and
                    not any(c in i.filename for c in ('\\', ':', '\x00')) and
                    not any(ord(c) < 32 for c in i.filename), 'Unsafe archive name')
            require(i.filename.casefold() not in seen and not stat.S_ISLNK(i.external_attr >> 16), 'Duplicate or linked entry')
            seen.add(i.filename.casefold())
        z.extractall(destination)


def validate(folder: Path, target: str, sha: str, tree: str, run: dict) -> dict[str, str]:
    require(target in TARGETS, 'Unknown platform')
    sums = existing.checksums(folder)
    actual = {p.relative_to(folder).as_posix() for p in folder.rglob('*') if p.is_file()}
    require(actual == sums.keys() | {'SHA256SUMS.txt'}, 'Incomplete artifact hash coverage')
    base = f'ShinyDesktop-{VERSION}-{target}'
    required = {'build-info.json', 'dependencies.json', 'installed-payload.json', 'ctest.xml', 'README.md',
                'THIRD_PARTY_NOTICES.md', base + '-Source.zip'}
    required |= {f'{mode}/{name}' for mode in ('build-smoke', 'installed-smoke')
                 for name in ('playback.json', 'desktop.png', 'decoded-frame.png')}
    binaries = ([base + '-Setup.exe', base + '-Portable.zip'] if target == 'Windows-x64' else
                [base + '.deb', base + '.tar.gz'] if target == 'Linux-x64' else [base + '.dmg'])
    require(required | set(binaries) <= sums.keys(), 'Required binary or native evidence missing')
    info = json.loads((folder / 'build-info.json').read_text())
    require(info.get('schema') == 1 and info.get('version') == VERSION and info.get('platform') == target
            and info.get('sourceSha') == sha and info.get('sourceTree') == tree
            and info.get('runId') == run['id'] and info.get('runAttempt') == run['run_attempt'], 'Wrong build identity')
    require(info.get('installedPlaybackVerified') is True and info.get('uninstallVerified') is True
            and info.get('notarized') is False and info.get('signedByPublisher') is False, 'Invalid installation/signing evidence')
    for mode in ('build-smoke', 'installed-smoke'):
        playback = json.loads((folder / mode / 'playback.json').read_text())
        require(playback.get('passed') is True and playback.get('version') == VERSION
                and playback.get('deliveredVideoFrames', 0) >= 8 and playback.get('pauseVerified') is True
                and playback.get('seekVerified') is True and playback.get('stopVerified') is True
                and playback.get('dlss') is False, 'Native decoded playback not demonstrated')
        for name in ('desktop.png', 'decoded-frame.png'):
            require((folder / mode / name).read_bytes().startswith(b'\x89PNG\r\n\x1a\n'), 'Invalid captured image')
    import xml.etree.ElementTree as ET
    junit = ET.parse(folder / 'ctest.xml').getroot()
    require(int(junit.get('tests', '0')) >= 1 and int(junit.get('failures', '-1')) == 0
            and int(junit.get('errors', '0')) == 0, 'Native policy/UI tests failed or absent')
    dependency = json.loads((folder / 'dependencies.json').read_text())
    require(bool(dependency.get('dependencies')) and bool(dependency.get('correspondingSourceAccess')), 'Dependency notices/source access missing')
    payload = json.loads((folder / 'installed-payload.json').read_text())
    require(len(payload.get('files', [])) >= 4, 'Installed payload inventory missing')
    if target == 'Windows-x64':
        require((folder / binaries[0]).read_bytes()[:2] == b'MZ', 'Invalid Windows installer')
        with tempfile.TemporaryDirectory() as temp:
            portable = Path(temp)
            unpack(folder / binaries[1], portable)
            files = {p.relative_to(portable).as_posix(): p for p in portable.rglob('*') if p.is_file()}
            require(files.keys() == {p['path'] for p in payload['files']}, 'Portable payload differs from installation inventory')
            for row in payload['files']:
                require(existing.digest(files[row['path']]) == row['sha256'], 'Modified portable payload')
    elif target == 'Linux-x64':
        require((folder / binaries[0]).read_bytes().startswith(b'!<arch>\n'), 'Invalid DEB package')
    return {name: sums[name] for name in [*binaries, base + '-Source.zip']}


def json_api(path: str, payload: dict) -> dict:
    result = subprocess.run(['gh', 'api', path, '--method', 'POST', '--input', '-'],
                            input=json.dumps(payload), text=True, capture_output=True, check=True, timeout=60)
    return json.loads(result.stdout)


def main() -> None:
    repo, sha = os.environ['GH_REPO'], os.environ['RELEASE_SHA']
    require(bool(re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repo)) and
            bool(re.fullmatch(r'[0-9a-f]{40}', sha)), 'Invalid repository/revision')
    require(subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip() == sha, 'Wrong checkout')
    tag = 'desktop-v' + VERSION
    release = existing.find_release(repo, tag)
    if release and not release['draft'] and release['target_commitish'] != sha:
        print('Desktop version is already published; bump its version for another release. No assets changed.')
        return
    existing.REQUIRED = REQUIRED
    repository_id = existing.api(f'repos/{repo}')['id']
    tree = subprocess.check_output(['git', 'rev-parse', 'HEAD^{tree}'], text=True).strip()

    def runs() -> dict:
        require(existing.api(f'repos/{repo}/git/ref/heads/main')['object']['sha'] == sha, 'Main moved')
        return existing.select_runs(existing.paged(f'repos/{repo}/actions/runs?head_sha={sha}&event=push', 'workflow_runs'),
            existing.paged(f'repos/{repo}/commits/{sha}/check-runs', 'check_runs'), sha, repository_id)

    try: selected = runs()
    except existing.ReleaseError as e:
        print('Not ready; no release changes made:', e)
        return
    native = selected['desktop (Windows-x64)']
    artifacts = existing.paged(f'repos/{repo}/actions/runs/{native["id"]}/artifacts', 'artifacts')
    with tempfile.TemporaryDirectory(prefix='shiny-desktop-release-') as tmp:
        root = Path(tmp); upload = root / 'upload'; upload.mkdir()
        for target in TARGETS:
            found = [a for a in artifacts if a['name'] == 'desktop-' + target]
            require(len(found) == 1, 'Missing or ambiguous platform artifact')
            artifact = found[0]; metadata = artifact.get('workflow_run', {})
            require(not artifact['expired'] and metadata.get('head_sha') == sha
                    and metadata.get('head_branch') == 'main' and metadata.get('id') == native['id']
                    and 0 < artifact['size_in_bytes'] <= MAX_BYTES, 'Wrong/oversized native artifact')
            archive = root / (target + '.zip')
            with archive.open('wb') as out:
                subprocess.run(['gh', 'api', f'repos/{repo}/actions/artifacts/{artifact["id"]}/zip'], stdout=out, check=True, timeout=300)
            require(artifact.get('digest') == 'sha256:' + existing.digest(archive), 'Artifact download digest mismatch')
            folder = root / target; unpack(archive, folder)
            assets = validate(folder, target, sha, tree, native)
            for name in assets: (upload / name).write_bytes((folder / name).read_bytes())
            base = f'ShinyDesktop-{VERSION}-{target}'
            with ZipFile(upload / (base + '-Evidence.zip'), 'w', ZIP_DEFLATED) as evidence:
                for p in sorted(folder.rglob('*')):
                    if p.is_file() and (p.suffix in {'.json', '.png', '.xml', '.md'} or p.name == 'SHA256SUMS.txt'):
                        evidence.write(p, p.relative_to(folder).as_posix())
            (upload / (base + '-Screenshot.png')).write_bytes((folder / 'installed-smoke/desktop.png').read_bytes())
        (upload / 'README.md').write_bytes(Path('native/desktop/README.md').read_bytes())
        (upload / 'THIRD_PARTY_NOTICES.md').write_bytes(Path('native/desktop/THIRD_PARTY_NOTICES.md').read_bytes())
        sums = {p.name: existing.digest(p) for p in upload.iterdir()}
        (upload / 'SHA256SUMS.txt').write_text(''.join(f'{value}  {name}\n' for name, value in sorted(sums.items())))
        sums['SHA256SUMS.txt'] = existing.digest(upload / 'SHA256SUMS.txt')
        runs()  # Still the current fully verified main before creating a draft.
        if not release:
            ref = existing.api(f'repos/{repo}/git/ref/tags/{tag}', optional=True)
            require(not ref or (ref['object']['type'] == 'commit' and ref['object']['sha'] == sha), 'Conflicting tag')
            release = json_api(f'repos/{repo}/releases', {'tag_name': tag, 'target_commitish': sha,
                'name': 'Shiny Desktop ' + VERSION + ' — Windows, macOS and Linux', 'draft': True,
                'prerelease': True, 'make_latest': 'false',
                'body': Path('docs/release-desktop-0.1.md').read_text() + f'\nVerified source: `{sha}`\n'})
        require(release['target_commitish'] == sha and release['prerelease'] is True, 'Release identity conflict')
        present = existing.remote_assets(release, sums, complete=not release['draft'])
        if not release['draft']:
            print('Identical desktop release already published; no overwrite.'); return
        for name in sorted(sums.keys() - present):
            existing.gh('release', 'upload', tag, str(upload / name), '--repo', repo)
        release = existing.api(f'repos/{repo}/releases/{release["id"]}')
        existing.remote_assets(release, sums, complete=True)
        final_runs = runs()
        require(all((r['id'], r['run_attempt']) == (final_runs[k]['id'], final_runs[k]['run_attempt'])
                    for k, r in selected.items()), 'A validated workflow changed before publication')
        existing.api(f'repos/{repo}/releases/{release["id"]}', '--method', 'PATCH', '-F', 'draft=false', '-f', 'make_latest=false')
        print('Published verified desktop prerelease:', tag)


if __name__ == '__main__':
    main()
