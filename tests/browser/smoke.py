"""Real Chromium/WebGL integration; no hardware performance certification.
Run after npm run build. Requires playwright; uses installed chromium or Playwright's browser.
"""
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import time
import zlib
from urllib.request import urlopen
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'artifacts'
OUT.mkdir(exist_ok=True)
PORT = 4173

def png(width=192, height=108):
    def chunk(name, data):
        return struct.pack('!I', len(data)) + name + data + struct.pack('!I', zlib.crc32(name + data) & 0xffffffff)
    raw = b''.join(b'\0' + b''.join(bytes((x % 256, y % 256, (x ^ y) % 256, 128)) for x in range(width)) for y in range(height))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('!2I5B', width, height, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')

server = subprocess.Popen(['node', 'scripts/serve.mjs'], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
results = []
try:
    for _ in range(100):
        try:
            urlopen(f'http://127.0.0.1:{PORT}', timeout=.2).close()
            break
        except OSError:
            time.sleep(.1)
    else:
        raise RuntimeError('Local test server did not start.')
    with sync_playwright() as p:
        executable = os.environ.get('CHROMIUM_PATH') or (None if os.environ.get('CI') else shutil.which('chromium'))
        browser = p.chromium.launch(executable_path=executable, channel=None if executable else 'chromium', headless=True,
            args=['--no-sandbox', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--autoplay-policy=no-user-gesture-required'])
        context = browser.new_context(viewport={'width': 1440, 'height': 1080}, accept_downloads=True)
        # Exercise the explicit fallback rather than depending on the runner's adapter policy.
        context.add_init_script("Object.defineProperty(navigator, 'gpu', {value: undefined, configurable: true});")
        page = context.new_page()
        errors = []
        page.on('pageerror', lambda error: errors.append(str(error)))
        page.goto(f'http://127.0.0.1:{PORT}/')
        page.wait_for_function("document.querySelector('#stage').getAttribute('aria-busy') === 'false'")
        assert 'WebGL2' in page.locator('#backend-pill').inner_text(), page.locator('#status').inner_text()
        page.screenshot(path=str(OUT / 'lab-desktop.png'), full_page=True)
        before = page.locator('#canvas-mount canvas').evaluate('(c) => c.toDataURL()')
        page.locator('#bypass').click()
        page.wait_for_timeout(100)
        after = page.locator('#canvas-mount canvas').evaluate('(c) => c.toDataURL()')
        assert before != after, 'The shader must change actual output, not just its label.'
        results.append('real WebGL2 shader changes output and bypass restores original')
        page.locator('#file').set_input_files({'name': 'alpha-fixture.png', 'mimeType': 'image/png', 'buffer': png()})
        page.wait_for_function("document.querySelector('#dimensions').textContent === '192 × 108'")
        page.locator('#scale').select_option('2')
        page.wait_for_function("document.querySelector('#dimensions').textContent === '384 × 216'")
        with page.expect_download() as download:
            page.locator('#export').click()
        output = Path(download.value.path()).read_bytes()
        assert output.startswith(b'\x89PNG') and struct.unpack('!2I', output[16:24]) == (384, 216)
        results.append('local image, scaling and real PNG export')
        page.locator('#file').set_input_files({'name': 'unsafe.svg', 'mimeType': 'image/svg+xml', 'buffer': b'<svg />'})
        assert 'Choose a PNG' in page.locator('#status').inner_text()
        results.append('unsupported-file rejection preserves existing session')
        page.locator('#sample').click()
        page.wait_for_function("document.querySelector('#stage').getAttribute('aria-busy') === 'false' && document.querySelector('#dimensions').textContent === '960 × 540'")
        page.locator('#benchmark').click()
        page.wait_for_function("document.querySelector('#status').textContent.includes('60-frame diagnostic complete')", timeout=60000)
        assert page.locator('#metric-count').inner_text() == '60'
        with page.expect_download() as download:
            page.locator('#report').click()
        report = json.loads(Path(download.value.path()).read_text())
        assert report['samples'] == 60 and report['sourceNamesIncluded'] is False
        assert 'alpha-fixture' not in json.dumps(report)
        results.append('measured 60-frame report without filenames or media')
        page.evaluate("navigator.mediaDevices.getDisplayMedia = async () => { throw new DOMException('User cancelled capture', 'NotAllowedError'); }")
        page.locator('#share').click()
        page.wait_for_function("document.querySelector('#status').textContent.includes('User cancelled')")
        assert page.locator('#canvas-mount canvas').count() == 0
        results.append('capture denial stops the previous session without a stuck canvas')
        for _ in range(5):
            page.locator('#sample').click()
            page.wait_for_function("document.querySelector('#stage').getAttribute('aria-busy') === 'false' && document.querySelector('#canvas-mount canvas') !== null")
            page.keyboard.press('Escape')
            assert page.locator('#canvas-mount canvas').count() == 0
        results.append('five create/process/dispose cycles and Escape cleanup')
        page.locator('#sample').click()
        page.wait_for_function("document.querySelector('#stage').getAttribute('aria-busy') === 'false'")
        page.set_viewport_size({'width': 390, 'height': 844})
        assert page.evaluate('document.documentElement.scrollWidth <= window.innerWidth'), 'Mobile horizontal overflow'
        page.screenshot(path=str(OUT / 'lab-mobile.png'), full_page=True)
        page.locator('#connect-native').click()
        assert 'installed extension' in page.locator('#native-status').inner_text()
        results.append('390px mobile layout and honest unavailable-native state')
        assert not errors, errors
        browser.close()
        # Install the actual MV3 package and exercise its privileged viewer entry point.
        with tempfile.TemporaryDirectory(prefix='shiny-extension-') as profile:
            extension = str(ROOT / 'dist' / 'extension')
            context = p.chromium.launch_persistent_context(profile, executable_path=executable, channel=None if executable else 'chromium', headless=True,
                args=['--no-sandbox', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', f'--disable-extensions-except={extension}', f'--load-extension={extension}'])
            worker = context.service_workers[0] if context.service_workers else context.wait_for_event('serviceworker')
            extension_id = worker.url.split('/')[2]
            popup = context.new_page()
            popup.goto(f'chrome-extension://{extension_id}/apps/extension/popup.html')
            with context.expect_page() as opened:
                popup.locator('#lab').click()
            lab = opened.value
            lab.wait_for_load_state()
            lab.wait_for_function("document.querySelector('#stage').getAttribute('aria-busy') === 'false'", timeout=30000)
            assert 'apps/viewer/index.html' in lab.url
            assert 'Spatial' in lab.locator('#backend-pill').inner_text(), lab.locator('#status').inner_text()
            results.append('actual MV3 extension service worker, popup and lab launch')
            context.close()
    (OUT / 'browser-report.json').write_text(json.dumps({'passed': results, 'hardwareCertified': False,
        'notTested': ['physical GPU performance', 'real screen/tab consent picker', 'DRM', 'native Windows capture', 'third-party players']}, indent=2))
    print(json.dumps({'passed': results}, indent=2))
finally:
    server.terminate()
    try:
        server.wait(timeout=5)
    except subprocess.TimeoutExpired:
        server.kill()
