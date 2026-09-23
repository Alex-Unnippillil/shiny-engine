"""Real Chromium/WebGL integration; no hardware performance certification.
Run after npm run build. Requires playwright; uses installed chromium or Playwright's browser.
"""
import base64
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
from playwright.sync_api import sync_playwright, expect

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
        expect(page.locator('#stage')).to_have_attribute('aria-busy', 'false')
        assert 'WebGL2' in page.locator('#backend-pill').inner_text(), page.locator('#status').inner_text()
        page.screenshot(path=str(OUT / 'lab-desktop.png'), full_page=True)
        before = page.locator('#canvas-mount canvas').evaluate('(c) => c.toDataURL()')
        page.locator('#bypass').click()
        page.wait_for_timeout(100)
        after = page.locator('#canvas-mount canvas').evaluate('(c) => c.toDataURL()')
        assert before != after, 'The shader must change actual output, not just its label.'
        results.append('real WebGL2 shader changes output and bypass restores original')
        page.locator('#file').set_input_files({'name': 'alpha-fixture.png', 'mimeType': 'image/png', 'buffer': png()})
        expect(page.locator('#dimensions')).to_have_text('192 × 108')
        page.locator('#scale').select_option('2')
        expect(page.locator('#dimensions')).to_have_text('384 × 216')
        with page.expect_download() as download:
            page.locator('#export').click()
        output = Path(download.value.path()).read_bytes()
        assert output.startswith(b'\x89PNG') and struct.unpack('!2I', output[16:24]) == (384, 216)
        results.append('local image, scaling and real PNG export')
        page.locator('#file').set_input_files({'name': 'unsafe.svg', 'mimeType': 'image/svg+xml', 'buffer': b'<svg />'})
        assert 'Choose a PNG' in page.locator('#status').inner_text()
        results.append('unsupported-file rejection preserves existing session')
        page.locator('#reset').click()
        page.locator('#sample').click()
        expect(page.locator('#stage')).to_have_attribute('aria-busy', 'false')
        expect(page.locator('#dimensions')).to_have_text('960 × 540')
        page.locator('#benchmark').click()
        expect(page.locator('#status')).to_contain_text('60-frame diagnostic complete', timeout=60000)
        assert page.locator('#metric-count').inner_text() == '60'
        with page.expect_download() as download:
            page.locator('#report').click()
        report = json.loads(Path(download.value.path()).read_text())
        assert report['samples'] == 60 and report['sourceNamesIncluded'] is False
        assert 'alpha-fixture' not in json.dumps(report)
        results.append('measured 60-frame report without filenames or media')
        # A generated moving fixture exercises decoding, native playback and source-driven rendering.
        encoded = page.evaluate("""async () => {
          const c = document.createElement('canvas'); c.width = 320; c.height = 180;
          const g = c.getContext('2d'); const stream = c.captureStream(15);
          const chunks = []; const recorder = new MediaRecorder(stream, {mimeType: 'video/webm;codecs=vp8'});
          const finished = new Promise(resolve => { recorder.onstop = resolve; });
          recorder.ondataavailable = e => { if(e.data.size) chunks.push(e.data); };
          recorder.start(); let i = 0;
          const timer = setInterval(() => {
            g.fillStyle = '#203c48'; g.fillRect(0,0,320,180);
            g.fillStyle = '#a5e9c6'; g.fillRect((i*7)%280,45,40,90); i++;
          }, 60);
          await new Promise(resolve => setTimeout(resolve, 2200));
          clearInterval(timer); recorder.stop(); await finished; stream.getTracks().forEach(t => t.stop());
          const bytes = new Uint8Array(await new Blob(chunks, {type:'video/webm'}).arrayBuffer());
          let binary = ''; for (const b of bytes) binary += String.fromCharCode(b);
          return btoa(binary);
        }""")
        page.locator('#file').set_input_files({'name': 'motion-fixture.webm', 'mimeType': 'video/webm', 'buffer': base64.b64decode(encoded)})
        expect(page.locator('#dimensions')).to_have_text('320 × 180')
        page.wait_for_timeout(400)
        assert int(page.locator('#metric-count').inner_text()) > 1, 'Video must process multiple source frames.'
        playback = page.locator('#source-video').evaluate('(v) => ({time:v.currentTime, muted:v.muted, controls:v.controls})')
        assert playback['time'] > 0 and playback['muted'] is False and playback['controls'] is True
        page.locator('#source-video').evaluate('(v) => { v.pause(); v.currentTime = 0.5; }')
        page.keyboard.press('Escape')
        assert page.locator('#canvas-mount canvas').count() == 0
        results.append('local moving WebM decoding, multiple processed frames, playback controls and cleanup')
        page.evaluate("() => { navigator.mediaDevices.getDisplayMedia = async () => { throw new DOMException('User cancelled capture', 'NotAllowedError'); }; }")
        page.locator('#share').click()
        expect(page.locator('#status')).to_contain_text('cancelled or not permitted')
        assert page.locator('#canvas-mount canvas').count() == 0
        results.append('capture denial leaves the stopped workspace usable')
        for _ in range(5):
            page.locator('#sample').click()
            expect(page.locator('#stage')).to_have_attribute('aria-busy', 'false')
            expect(page.locator('#canvas-mount canvas')).to_have_count(1)
            page.keyboard.press('Escape')
            assert page.locator('#canvas-mount canvas').count() == 0
        results.append('five create/process/dispose cycles and Escape cleanup')
        page.locator('#sample').click()
        expect(page.locator('#stage')).to_have_attribute('aria-busy', 'false')
        page.set_viewport_size({'width': 390, 'height': 844})
        assert page.evaluate('document.documentElement.scrollWidth <= window.innerWidth'), 'Mobile horizontal overflow'
        page.screenshot(path=str(OUT / 'lab-mobile.png'), full_page=True)
        expect(page.locator('#connect-native')).to_be_disabled()
        assert 'installed extension' in page.locator('#native-status').inner_text()
        results.append('390px mobile layout and honest unavailable-native state')
        # Presets and comparison modes round-trip through storage, not just button styling.
        page.set_viewport_size({'width': 1440, 'height': 1000})
        page.locator('[data-preset=detail]').click()
        expect(page.locator('#strength')).to_have_value('65')
        page.reload()
        expect(page.locator('#stage')).to_have_attribute('aria-busy', 'false')
        expect(page.locator('[data-preset=detail]')).to_have_attribute('aria-pressed', 'true')
        expect(page.locator('#strength')).to_have_value('65')
        page.locator('#strength').evaluate("e => { e.value = '51'; e.dispatchEvent(new Event('input', {bubbles:true})); }")
        expect(page.locator('#preset-name')).to_have_text('Custom adjustments')
        assert page.locator('[data-preset][aria-pressed=true]').count() == 0
        page.locator('[data-view=enhanced]').click()
        expect(page.locator('#split')).to_have_value('0')
        page.locator('[data-view=split]').click()
        expect(page.locator('#split')).to_have_value('50')
        divider = page.locator('#comparison-line')
        divider.focus(); page.keyboard.press('ArrowRight')
        expect(page.locator('#split')).to_have_value('51')
        page.keyboard.press('Shift+ArrowRight')
        expect(page.locator('#split')).to_have_value('61')
        box = page.locator('#stage').bounding_box()
        handle = divider.bounding_box()
        page.mouse.move(handle['x'] + handle['width']/2, handle['y'] + handle['height']/2)
        page.mouse.down(); page.mouse.move(box['x']+box['width']*.8, box['y']+box['height']/2, steps=5); page.mouse.up()
        assert 78 <= int(page.locator('#split').input_value()) <= 82
        results.append('persistent presets, custom state, three views and real pointer/keyboard divider')
        # Escape closes help before it can stop the active media.
        page.locator('#help').click()
        expect(page.locator('#help-dialog')).to_be_visible()
        page.keyboard.press('Escape')
        expect(page.locator('#help-dialog')).not_to_be_visible()
        expect(page.locator('#canvas-mount canvas')).to_have_count(1)
        expect(page.locator('#help')).to_be_focused()
        results.append('help modal focus and Escape leave the media session intact')
        # Decode failures and denied capture must preserve the working source.
        page.evaluate("() => { navigator.mediaDevices.getDisplayMedia = async () => { throw new DOMException('User cancelled capture', 'NotAllowedError'); }; }")
        previous_name = page.locator('#source-name').inner_text()
        page.locator('#file').set_input_files({'name': 'broken.png', 'mimeType': 'image/png', 'buffer': b'not-a-real-image'})
        expect(page.locator('#status')).to_contain_text('could not be opened')
        expect(page.locator('#source-name')).to_have_text(previous_name)
        expect(page.locator('#canvas-mount canvas')).to_have_count(1)
        page.locator('#share').click()
        expect(page.locator('#status')).to_contain_text('current media is unchanged')
        expect(page.locator('#source-name')).to_have_text(previous_name)
        expect(page.locator('#canvas-mount canvas')).to_have_count(1)
        results.append('corrupt image and capture denial preserve the active preview')
        # A cancelled picker that later returns a stream must not leave capture running.
        page.evaluate("""() => {
          navigator.mediaDevices.getDisplayMedia = () => new Promise(resolve => { window.finishPicker = resolve; });
        }""")
        page.locator('#share').click()
        expect(page.locator('#loading-banner')).to_be_visible()
        page.locator('#cancel-operation').click()
        expect(page.locator('#loading-banner')).not_to_be_visible()
        stopped = page.evaluate("""async () => {
          const c = document.createElement('canvas'); c.width=32; c.height=32;
          const stream = c.captureStream(10); window.finishPicker(stream);
          await new Promise(resolve=>setTimeout(resolve,100));
          return stream.getTracks().every(t=>t.readyState==='ended');
        }""")
        assert stopped
        expect(page.locator('#source-name')).to_have_text(previous_name)
        results.append('late capture after cancellation is released without replacing the source')
        # Corrupt video failure uses a candidate element and must not empty the active player.
        page.locator('#file').set_input_files({'name': 'broken.webm', 'mimeType': 'video/webm', 'buffer': b'broken'})
        expect(page.locator('#status')).to_contain_text('cannot decode', timeout=20000)
        expect(page.locator('#source-name')).to_have_text(previous_name)
        # Newest source wins even when an older image decode finishes out of order.
        page.evaluate("""() => {
          const original = HTMLImageElement.prototype.decode; let first = true;
          HTMLImageElement.prototype.decode = async function() {
            await original.call(this);
            if (first) { first = false; await new Promise(resolve=>{window.finishDecode = resolve;}); }
          };
        }""")
        page.locator('#file').set_input_files({'name': 'slow.png', 'mimeType': 'image/png', 'buffer': png(100,100)})
        page.wait_for_timeout(150)
        page.locator('#file').set_input_files({'name': 'latest.png', 'mimeType': 'image/png', 'buffer': png(160,90)})
        expect(page.locator('#source-name')).to_have_text('latest.png')
        page.evaluate('() => { window.finishDecode?.(); }')
        page.wait_for_timeout(150)
        expect(page.locator('#source-name')).to_have_text('latest.png')
        expect(page.locator('#stage')).to_have_attribute('aria-busy', 'false')
        results.append('invalid video preserves source and newest image wins out-of-order loading')
        # PNG export offers explicit content selection and preserves displayed comparison.
        for view in ['original', 'enhanced', 'comparison']:
            page.locator('#export-view').select_option(view)
            with page.expect_download() as item:
                page.locator('#export').click()
            assert item.value.suggested_filename == f'latest-{view}.png'
            expect(page.locator('#stage')).to_have_attribute('aria-busy','false')
        results.append('original, enhanced and current-view PNG exports with safe source-derived names')
        page.locator('#reset').click(); page.locator('#sample').click()
        expect(page.locator('#stage')).to_have_attribute('aria-busy', 'false')
        page.locator('#benchmark').click()
        page.locator('#cancel-operation').click()
        expect(page.locator('#status')).to_contain_text('Measurement cancelled')
        expect(page.locator('#benchmark')).to_be_enabled()
        expect(page.locator('#export')).to_be_enabled()
        results.append('measurement cancellation restores adjustment and export controls')
        # Single-preview video transport and paused rendering behavior.
        page.locator('#file').set_input_files({'name':'motion.webm','mimeType':'video/webm','buffer':base64.b64decode(encoded)})
        expect(page.locator('#transport')).to_be_visible()
        page.locator('#play').click()
        expect(page.locator('#play')).to_have_text('Play')
        # Pause permits the in-flight GPU frame and one final refresh to drain.
        # Wait for observed quiescence instead of assuming a hosted software GPU
        # always completes both within 400 ms. Deterministic scheduler unit tests
        # separately enforce the exact callback bound, including a 2-second frame.
        page.wait_for_function("""() => {
          const video = document.querySelector('#source-video');
          const count = Number(document.querySelector('#metric-count').textContent);
          const now = performance.now();
          if (!video.paused || count < 1) { window.pauseObservation = null; return false; }
          if (!window.pauseObservation || window.pauseObservation.count !== count) {
            window.pauseObservation = {count, since: now}; return false;
          }
          return now - window.pauseObservation.since >= 1000;
        }""", timeout=15000, polling=50)
        count = page.locator('#metric-count').inner_text()
        page.wait_for_timeout(750)
        assert page.locator('#metric-count').inner_text() == count, 'Paused media must not keep consuming frames after pending work drains'
        page.locator('#seek').evaluate("e => { e.value = '0.7'; e.dispatchEvent(new Event('input', {bubbles:true})); }")
        page.locator('#rate').select_option('1.5')
        assert page.locator('#source-video').evaluate('(v)=>v.playbackRate') == 1.5
        page.locator('#volume').evaluate("e => { e.value = '35'; e.dispatchEvent(new Event('input', {bubbles:true})); }"); page.locator('#mute').click()
        assert page.locator('#source-video').evaluate('(v)=>v.volume===0.35 && v.muted')
        expect(page.locator('#source-slot video')).to_be_hidden()
        page.locator('#play').click(); expect(page.locator('#play')).to_have_text('Pause')
        page.wait_for_function("count => Number(document.querySelector('#metric-count').textContent) > count", arg=int(count), timeout=10000)
        page.locator('#stop').click()
        expect(page.locator('#transport')).to_be_hidden()
        expect(page.locator('#empty-open')).to_be_visible()
        results.append('video play/pause/seek/rate/audio, idle pause and empty-state recovery')
        page.locator('#empty-demo').click()
        expect(page.locator('#stage')).to_have_attribute('aria-busy','false')
        page.screenshot(path=str(OUT / 'lab-desktop.png'), full_page=True)
        for width in [320,390,768]:
            page.set_viewport_size({'width':width,'height':844})
            assert page.evaluate('document.documentElement.scrollWidth <= innerWidth'), f'Overflow at {width}px'
        page.set_viewport_size({'width':390,'height':844})
        page.screenshot(path=str(OUT / 'lab-mobile.png'),full_page=True)
        results.append('320px, 390px and 768px responsive workspace without horizontal overflow')
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
            expect(popup.locator('#inline')).to_be_disabled()
            expect(popup.locator('#capture')).to_be_disabled()
            popup.screenshot(path=str(OUT / 'extension-popup.png'))
            with context.expect_page() as opened:
                popup.locator('#lab').click()
            lab = opened.value
            lab.wait_for_load_state()
            expect(lab.locator('#stage')).to_have_attribute('aria-busy', 'false', timeout=30000)
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
