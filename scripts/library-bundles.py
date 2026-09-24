# SPDX-License-Identifier: MIT
"""Generate exact-build first-party approvals, never a user-editable trust registry."""
import hashlib
import json
from pathlib import Path
import shutil
import sys

out, package, first, second = map(Path, sys.argv[1:])
out.parent.mkdir(parents=True, exist_ok=True)
identities = []
for version, dll in [('1.0.0', first), ('1.1.0', second)]:
    data = dll.read_bytes()
    manifest = {
        'schemaVersion': 1, 'id': 'shiny-spatial', 'version': version,
        'family': 'shiny-spatial-reference', 'architecture': 'x64', 'abi': 1,
        'license': 'MIT', 'source': 'https://github.com/Alex-Unnippillil/shiny-engine',
        'dependencySet': 'shiny-spatial-abi-1',
        'files': [{'name': 'shiny_spatial.dll', 'size': len(data),
                   'sha256': hashlib.sha256(data).hexdigest()}],
    }
    raw = (json.dumps(manifest, sort_keys=True, separators=(',', ':')) + '\n').encode('ascii')
    identity = hashlib.sha256(raw).hexdigest()
    destination = package / version
    destination.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(dll, destination / 'shiny_spatial.dll')
    (destination / 'manifest.json').write_bytes(raw)
    identities.append(identity)
out.write_text('// Generated from the two first-party binaries in this build.\n#pragma once\n'
               '#include "package.hpp"\nnamespace shiny::packages {\n'
               'inline Policy buildPolicy(){return Policy{{' +
               ','.join(json.dumps(value) for value in identities) + '}};}\n}\n', encoding='utf-8')
