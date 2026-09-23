"""Actual script-free player guide UI; does not exercise a Windows executable."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
from urllib.request import urlopen
from playwright.sync_api import sync_playwright, expect
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'artifacts';OUT.mkdir(exist_ok=True)
PORT=4179
server=subprocess.Popen(['node','scripts/serve.mjs'],cwd=ROOT,env={**os.environ,'PORT':str(PORT)},stdout=subprocess.DEVNULL,stderr=subprocess.STDOUT)
passed=[];errors=[];requests=[]
try:
    for _ in range(100):
        try:
            if urlopen(f'http://127.0.0.1:{PORT}/apps/player/index.html').status==200:break
        except OSError:time.sleep(.05)
    else:raise RuntimeError('Local guide server did not start')
    with sync_playwright() as p:
        exe=os.environ.get('CHROMIUM_PATH') or (None if os.environ.get('CI') else shutil.which('chromium'))
        browser=p.chromium.launch(executable_path=exe,headless=True,args=['--no-sandbox'])
        page=browser.new_page(viewport={'width':1360,'height':980})
        page.on('pageerror',lambda e: errors.append(str(e)))
        page.on('request',lambda r: requests.append(r.url))
        page.goto(f'http://127.0.0.1:{PORT}/apps/player/index.html')
        expect(page.get_by_role('heading',name='Your media. VLC at the core.')).to_be_visible()
        expect(page.get_by_text('Unsigned Windows installer and portable package.',exact=False)).to_be_visible()
        passed.append('versioned package identity and experimental boundaries visible')
        assert page.locator('script,iframe,video,canvas').count()==0
        assert "script-src 'none'" in page.locator('meta[http-equiv="Content-Security-Policy"]').get_attribute('content')
        assert all(u.startswith(f'http://127.0.0.1:{PORT}/') for u in requests)
        passed.append('no scripts, simulated player or external resource requests')
        for width in [320,390,640,768,1000,1280,1360]:
            page.set_viewport_size({'width':width,'height':980})
            assert page.evaluate('document.documentElement.scrollWidth <= innerWidth'),width
            expect(page.get_by_role('link',name='Get Windows 0.7.0')).to_be_visible()
        passed.append('seven responsive widths without horizontal overflow')
        page.screenshot(path=str(OUT/'player-guide-0.7-desktop.png'),full_page=True)
        page.screenshot(path=str(OUT/'player-guide-0.7-hero.png'))
        page.set_viewport_size({'width':390,'height':844})
        page.screenshot(path=str(OUT/'player-guide-0.7-mobile.png'),full_page=True)
        page.set_viewport_size({'width':1360,'height':980})
        page.get_by_role('link',name='Explore research mode').click()
        expect(page).to_have_url(f'http://127.0.0.1:{PORT}/apps/player/index.html#research')
        page.locator('summary').focus();page.keyboard.press('Enter')
        expect(page.locator('#build')).to_have_attribute('open','')
        expect(page.get_by_text('Requires Windows x64, the Visual Studio C++ toolchain',exact=False)).to_be_visible()
        passed.append('research navigation and keyboard-operated disclosure')
        for anchor in page.locator('a[href^="#"]').all():
            target=anchor.get_attribute('href')[1:]
            assert page.locator(f'[id="{target}"]').count()==1,target
        assert page.get_by_role('link',name='Get Windows 0.7.0').get_attribute('href').endswith('/releases/tag/v0.7.0')
        assert all('noopener' in link.get_attribute('rel') for link in page.locator('a[target="_blank"]').all())
        passed.append('internal anchors resolve; external links are fixed and isolated')
        assert not errors,errors
        passed.append('no page errors')
        browser.close()
finally:
    server.terminate();server.wait(timeout=10)
report={'passed':passed,'nativeWindowsTested':False,'trainedInferenceTested':False,'scope':'Actual browser rendering of a script-free guide, not native playback or model output.'}
(OUT/'player-guide-report.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
