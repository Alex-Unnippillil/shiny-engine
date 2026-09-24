# SPDX-License-Identifier: MIT
"""Copy only production library-manager payloads; never test executables."""
from pathlib import Path
import hashlib
import json
import shutil
import sys

binaries, destination = [Path(p).resolve() for p in sys.argv[1:3]]
root = Path(__file__).resolve().parent.parent
destination.mkdir(parents=True, exist_ok=True)
for name in ('ShinyLibraryManager.exe', 'ShinyLibraryManagerCli.exe', 'ShinyEnhancementWorker.exe'):
    shutil.copyfile(binaries / name, destination / name)
for version in ('1.0.0', '1.1.0', '1.2.0'):
    folder = binaries / 'library-bundles' / version
    target = destination / 'library-bundles' / version
    target.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((folder / 'manifest.json').read_text())
    expected = manifest['files'][0]
    data = (folder / 'shiny_spatial.dll').read_bytes()
    if len(data) != expected['size'] or hashlib.sha256(data).hexdigest() != expected['sha256']:
        raise RuntimeError('Build bundle no longer matches its generated manifest')
    for name in ('manifest.json', 'shiny_spatial.dll'):
        shutil.copyfile(folder / name, target / name)
for name in ('library-manager-guide.md', 'library-manager-status.md', 'workspace-guide.md'):
    shutil.copyfile(root / 'docs' / name, destination / name)
shutil.copyfile(root / 'LICENSE', destination / 'ShinyLibraryManager-LICENSE.txt')
shutil.copyfile(root / 'native/library-manager/THIRD_PARTY_NOTICES.md', destination / 'ShinyLibraryManager-NOTICES.md')
