"""Validate native-player setup UX in the actual extension; not Windows playback."""
import json
import os
from pathlib import Path
import shutil
import tempfile
from playwright.sync_api import sync_playwright, expect
ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'artifacts'
OUT.mkdir(exist_ok=True)
errors, passed = [], []
with sync_playwright() as p:
    executable = os.environ.get('CHROMIUM_PATH') or (None if os.environ.get('CI') else shutil.which('chromium'))
    with tempfile.TemporaryDirectory(prefix='shiny-player-extension-') as profile:
        extension = str(ROOT / 'dist/extension')
        context = p.chromium.launch_persistent_context(profile, executable_path=executable,
            channel=None if executable else 'chromium', headless=True,
            args=['--no-sandbox', f'--disable-extensions-except={extension}', f'--load-extension={extension}'])
        service = context.service_workers[0] if context.service_workers else context.wait_for_event('serviceworker')
        extension_id = service.url.split('/')[2]
        popup = context.new_page()
        popup.on('pageerror', lambda error: errors.append(str(error)))
        popup.goto(f'chrome-extension://{extension_id}/apps/extension/popup.html')
        expect(popup.locator('#player')).to_be_enabled()
        with context.expect_page() as opened:
            popup.locator('#player').click()
        page = opened.value
        page.on('pageerror', lambda error: errors.append(str(error)))
        page.wait_for_load_state()
        assert '/apps/player/index.html' in page.url
        expect(page.get_by_role('heading', name='Your media. VLC at the core.')).to_be_visible()
        passed.append('actual MV3 popup opens native-player setup, not simulated playback')
        expect(page.locator('.warning')).to_have_text('Experimental / unverified')
        expect(page.get_by_text('Unsigned Windows installer and portable package.', exact=False)).to_be_visible()
        assert page.locator('video, canvas, script, iframe').count() == 0
        assert "script-src 'none'" in page.locator('meta[http-equiv="Content-Security-Policy"]').get_attribute('content')
        passed.append('research boundaries visible; no fake player or automatic inference')
        for width in [320,390,768,1280]:
            page.set_viewport_size({'width':width,'height':900})
            assert page.evaluate('document.documentElement.scrollWidth <= innerWidth'), width
        page.screenshot(path=str(OUT / 'player-setup-desktop.png'), full_page=True)
        page.set_viewport_size({'width':390,'height':844})
        page.screenshot(path=str(OUT / 'player-setup-mobile.png'), full_page=True)
        page.set_viewport_size({'width':1280,'height':900})
        passed.append('320/390/768/1280 layout without horizontal overflow')
        page.get_by_role('link', name='Explore research mode').click()
        expect(page).to_have_url(f'chrome-extension://{extension_id}/apps/player/index.html#research')
        page.get_by_text('Build from source and validation boundaries', exact=True).click()
        expect(page.get_by_text('Requires Windows x64, the Visual Studio C++ toolchain', exact=False)).to_be_visible()
        passed.append('research anchor and build disclosure operate')
        assert page.get_by_role('link',name='Get Windows 0.7.0').get_attribute('href') == 'https://github.com/Alex-Unnippillil/shiny-engine/releases/tag/v0.7.0'
        assert all(link.get_attribute('rel') == 'noopener noreferrer' for link in page.locator('a[target="_blank"]').all())
        passed.append('release link pinned to version with isolated external navigation')
        page.get_by_role('link',name='LookLock', exact=True).click()
        expect(page).to_have_url(f'chrome-extension://{extension_id}/apps/looklock/index.html')
        passed.append('navigation back to working local review workspace')
        assert not errors, errors
        context.close()
(OUT / 'player-setup-report.json').write_text(json.dumps({'passed':passed,'nativePlaybackTestedHere':False},indent=2))
print(json.dumps({'passed':passed},indent=2))
