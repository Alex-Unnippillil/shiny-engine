"""Create installable archives and SHA-256 hashes. Does not sign or publish builds."""
from pathlib import Path
import hashlib
import zipfile
root = Path(__file__).resolve().parents[1]
out = root / 'artifacts'
out.mkdir(exist_ok=True)
for kind in ('extension', 'site'):
    target = out / f'shiny-engine-{kind}-0.2.0.zip'
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for file in sorted((root / 'dist' / kind).rglob('*')):
            if file.is_file():
                archive.write(file, file.relative_to(root / 'dist' / kind))
        for name in ('LICENSE', 'THIRD_PARTY_NOTICES.md'):
            archive.write(root / name, name)
    print(target.name, hashlib.sha256(target.read_bytes()).hexdigest())
with (out / 'SHA256SUMS.txt').open('w') as output:
    for file in sorted(out.glob('*.zip')):
        output.write(f'{hashlib.sha256(file.read_bytes()).hexdigest()}  {file.name}\n')
