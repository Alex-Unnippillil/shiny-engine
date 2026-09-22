"""Load the real MV3 package and exercise its Neural Lab and worker approval boundary."""
import json
import os
from pathlib import Path
import shutil
import tempfile
from playwright.sync_api import sync_playwright, expect
ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'artifacts'
OUT.mkdir(exist_ok=True)
errors = []
with sync_playwright() as p:
    executable = os.environ.get('CHROMIUM_PATH') or (None if os.environ.get('CI') else shutil.which('chromium'))
    with tempfile.TemporaryDirectory(prefix='shiny-nr-extension-') as profile:
        extension = str(ROOT / 'dist/extension')
        context = p.chromium.launch_persistent_context(profile, executable_path=executable,
            channel=None if executable else 'chromium', headless=True,
            args=['--no-sandbox', '--use-angle=swiftshader', '--enable-unsafe-swiftshader',
                  f'--disable-extensions-except={extension}', f'--load-extension={extension}'])
        service = context.service_workers[0] if context.service_workers else context.wait_for_event('serviceworker')
        extension_id = service.url.split('/')[2]
        popup = context.new_page()
        popup.on('pageerror', lambda error: errors.append(str(error)))
        popup.goto(f'chrome-extension://{extension_id}/apps/extension/popup.html')
        expect(popup.locator('#neural')).to_be_enabled()
        with context.expect_page() as opened:
            popup.locator('#neural').click()
        lab = opened.value
        lab.on('pageerror', lambda error: errors.append(str(error)))
        lab.wait_for_load_state()
        assert '/apps/nr/index.html' in lab.url
        expect(lab.locator('#availability')).to_have_text('Neural inference locked in this build')
        expect(lab.locator('#prepare')).to_be_disabled()
        expect(lab.locator('#live')).to_be_disabled()
        lab.locator('#probe').click()
        expect(lab.locator('#gpu-status')).not_to_contain_text('Not checked', timeout=30000)
        probe = lab.locator('#gpu-status').inner_text()
        assert probe.startswith(('Candidate adapter.', 'Unavailable.')), probe
        rejected = lab.evaluate("""async () => {
          const {NeuralClient}=await import('/packages/nr/client.js');
          const client=new NeuralClient(),canvas=new OffscreenCanvas(64,48);
          const manifestBytes=new TextEncoder().encode('{}').buffer;
          try {
            await client.call('prepare',{canvas,width:64,height:48,
              model:{manifestBytes,manifestSha256:'0'.repeat(64),stages:new Map()}},[canvas,manifestBytes]);
            return 'UNEXPECTED SUCCESS';
          } catch(error) {return String(error);} finally {client.stop();}
        }""")
        assert 'no repository-reviewed' in rejected, rejected
        assert not errors, errors
        lab.screenshot(path=str(OUT/'neural-extension.png'),full_page=True)
        (OUT/'neural-extension-report.json').write_text(json.dumps({
            'passed':3,
            'scenarios':['actual MV3 popup opens Neural Lab', 'actual extension worker probes capabilities',
                         'actual extension worker rejects unapproved model before GPU construction'],
            'probe':probe,'trainedModelInferenceTested':False,'physicalGpuCertified':False
        },indent=2))
        context.close()
print('PASS: 3 MV3 Neural Lab/worker boundary scenarios; no trained-model inference claim.')
