"""Download the pinned public-domain SQLite amalgamation for native Windows builds."""
from pathlib import Path
import hashlib
import sys
import urllib.request
import zipfile

VERSION = '3530400'
SHA3 = '628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e'
root = Path(sys.argv[1] if len(sys.argv) > 1 else '.deps').resolve()
root.mkdir(parents=True, exist_ok=True)
archive = root / f'sqlite-amalgamation-{VERSION}.zip'
if not archive.exists():
    with urllib.request.urlopen(f'https://sqlite.org/2026/{archive.name}', timeout=60) as response:
        data = response.read(8 * 1024 * 1024 + 1)
    if len(data) > 8 * 1024 * 1024:
        raise RuntimeError('SQLite archive exceeds size limit')
    if hashlib.sha3_256(data).hexdigest() != SHA3:
        raise RuntimeError('SQLite source SHA3-256 mismatch')
    archive.write_bytes(data)
if hashlib.sha3_256(archive.read_bytes()).hexdigest() != SHA3:
    raise RuntimeError('Cached SQLite source SHA3-256 mismatch')
# Extract only authenticated fixed names; do not execute dependency scripts.
with zipfile.ZipFile(archive) as z:
    for name in ('sqlite3.c', 'sqlite3.h'):
        data = z.read(f'sqlite-amalgamation-{VERSION}/{name}')
        if len(data) > 16 * 1024 * 1024:
            raise RuntimeError('SQLite source size exceeds limit')
        destination = root / 'sqlite'
        destination.mkdir(exist_ok=True)
        (destination / name).write_bytes(data)
print('Verified SQLite 3.53.4 in', root / 'sqlite')
