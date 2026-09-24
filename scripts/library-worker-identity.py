# SPDX-License-Identifier: MIT
import hashlib
from pathlib import Path
import sys
worker, header = map(Path, sys.argv[1:])
value = hashlib.sha256(worker.read_bytes()).hexdigest()
header.parent.mkdir(parents=True, exist_ok=True)
header.write_text('// Generated from the exact worker binary.\n#pragma once\n'
                  'namespace shiny::packages {inline constexpr const char* '
                  f'compiledWorkerSha256="{value}";}}\n', encoding='utf-8')
