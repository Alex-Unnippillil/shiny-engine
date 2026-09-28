# SPDX-License-Identifier: MIT
"""Bounded native packaging helpers; do not retry playback/test failures."""
from pathlib import Path
import re
import subprocess
import time


def ffmpeg_version(diagnostics: str) -> str | None:
    versions = set(re.findall(r'Using Qt multimedia with FFmpeg version ([0-9]+\.[0-9]+\.[0-9]+)', diagnostics))
    if len(versions) > 1:
        raise RuntimeError('Ambiguous FFmpeg runtime versions')
    return next(iter(versions)) if versions else None


def create_dmg(stage: Path, destination: Path) -> None:
    """Retry only hdiutil's transient Resource busy error; keep all later checks."""
    for attempt in range(3):
        subprocess.run(['sync'], check=True, timeout=30)
        result = subprocess.run(['hdiutil', 'create', '-volname', 'Shiny Desktop', '-srcfolder', str(stage),
                                 '-ov', '-format', 'UDZO', str(destination)],
                                capture_output=True, text=True, timeout=180)
        if result.returncode == 0:
            return
        if 'resource busy' not in result.stderr.lower() or attempt == 2:
            raise RuntimeError('DMG creation failed: ' + result.stderr.strip())
        print(f'hdiutil Resource busy; retrying package creation ({attempt + 1}/2).')
        time.sleep(2 * (attempt + 1))
