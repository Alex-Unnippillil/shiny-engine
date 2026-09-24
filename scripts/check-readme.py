"""Offline README contract: local links, release images and documented source paths."""
from pathlib import Path
from urllib.parse import urlsplit, unquote
import json
import re

ROOT = Path(__file__).resolve().parents[1]
text = (ROOT / 'README.md').read_text(encoding='utf-8')
version = json.loads((ROOT / 'package.json').read_text())['version']
required = {'player-workspace.png', 'player-welcome.png', 'player-quick-actions.png',
            'player-video-studio.png', 'player-library-manager.png', 'player-workspace-compact.png'}
links = re.findall(r'\]\(([^\s)]+)\)', text) + re.findall(r'(?:src|href)="([^"]+)"', text)
images = set()
for link in links:
    url = urlsplit(link)
    if not url.scheme and url.path:
        target = (ROOT / unquote(url.path)).resolve()
        if not target.is_relative_to(ROOT) or not target.exists():
            raise RuntimeError(f'Broken README local link: {link}')
    if '/releases/download/' in link:
        if f'/releases/download/v{version}/' not in link:
            raise RuntimeError(f'Stale README release reference: {link}')
        if url.path.endswith('.png'):
            images.add(url.path.rsplit('/', 1)[-1])
if images != required:
    raise RuntimeError('README release gallery is incomplete or unexpected')
for heading in ('## Quick start', '## Architecture', '## Tech stack', '## Capabilities and limits', '## Build and test'):
    if heading not in text:
        raise RuntimeError(f'Missing README section: {heading}')
if text.count('```mermaid\n') != 2:
    raise RuntimeError('README requires two source-maintained Mermaid diagrams')
for path in ('native/vlc-player', 'native/ui', 'native/library-manager', 'native/nr-worker',
             'apps/extension', 'native/windows', 'apps/viewer', 'apps/nr', 'apps/looklock'):
    if not (ROOT / path).is_dir():
        raise RuntimeError(f'Documented component missing: {path}')
print(f'README {version}: {len(links)} links checked, {len(images)} versioned images, two Mermaid source blocks and actual component paths')
