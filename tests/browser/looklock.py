"""Real Chromium still-frame review and MV3 entry points. No neural inference claim."""
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import time
from urllib.request import urlopen
import zlib
from playwright.sync_api import sync_playwright, expect
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'artifacts';OUT.mkdir(exist_ok=True)
PORT=4187

def png(w=96,h=64,shift=0):
    def chunk(name,data):return struct.pack('!I',len(data))+name+data+struct.pack('!I',zlib.crc32(name+data)&0xffffffff)
    raw=b''.join(b'\0'+b''.join(bytes(((x*2+shift)%256,(y*3+shift)%256,(x+y+shift)%256,255)) for x in range(w)) for y in range(h))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!2I5B',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')

def decode(data):
    assert data[:8]==b'\x89PNG\r\n\x1a\n';p=8;blocks=[];w=h=0
    while p<len(data):
        n=struct.unpack('!I',data[p:p+4])[0];name=data[p+4:p+8];body=data[p+8:p+8+n]
        assert zlib.crc32(name+body)&0xffffffff==struct.unpack('!I',data[p+8+n:p+12+n])[0]
        if name==b'IHDR':w,h=struct.unpack('!2I',body[:8]);assert body[8:10]==bytes([8,6])
        if name==b'IDAT':blocks.append(body)
        p+=n+12
    raw=zlib.decompress(b''.join(blocks));assert len(raw)==(w*4+1)*h
    assert all(raw[y*(w*4+1)]==0 for y in range(h))
    return w,h,b''.join(raw[y*(w*4+1)+1:(y+1)*(w*4+1)] for y in range(h))

def upload(page,id,name,data,mime='image/png'):
    page.locator(id).set_input_files({'name':name,'mimeType':mime,'buffer':data})
    expect(page.locator('#stage')).to_have_attribute('aria-busy','false',timeout=20000)

def download(page,id):
    with page.expect_download() as d:page.locator(id).click()
    return Path(d.value.path()).read_bytes()

server=subprocess.Popen(['node','scripts/serve.mjs'],cwd=ROOT,env={**os.environ,'PORT':str(PORT)},stdout=subprocess.DEVNULL)
passed=[];errors=[];violations=[]
try:
    for _ in range(100):
        try:urlopen(f'http://127.0.0.1:{PORT}',timeout=.2).close();break
        except OSError:time.sleep(.1)
    else:raise RuntimeError('LookLock server unavailable')
    with sync_playwright() as p:
        exe=os.environ.get('CHROMIUM_PATH') or (None if os.environ.get('CI') else shutil.which('chromium'))
        browser=p.chromium.launch(executable_path=exe,channel=None if exe else 'chromium',headless=True,args=['--no-sandbox'])
        context=browser.new_context(viewport={'width':1440,'height':1050},accept_downloads=True)
        page=context.new_page();page.on('pageerror',lambda e:errors.append(str(e)))
        page.on('console',lambda m:violations.append(m.text) if 'Content Security Policy' in m.text else None)
        page.goto(f'http://127.0.0.1:{PORT}/apps/looklock/index.html')
        expect(page.locator('#stage')).to_have_attribute('aria-busy','false')
        expect(page.locator('#zone-list li')).to_have_count(2)
        expect(page.locator('#mismatches')).to_have_text('0')
        expect(page.locator('#status')).to_contain_text('No neural inference')
        page.screenshot(path=str(OUT/'looklock-desktop.png'),full_page=True)
        passed.append('synthetic demo loads with explicit non-neural origin and zero protected mismatches')
        original=decode(download(page,'#export-source'));output=decode(download(page,'#export-output'))
        assert original[:2]==output[:2]==(960,540)
        for x,y,w,h in [(20,18,235,98),(170,460,620,57)]:
            for row in range(y,y+h):
                at=(row*960+x)*4
                assert original[2][at:at+w*4]==output[2][at:at+w*4]
        assert original[2]!=output[2]
        passed.append('independent PNG decoding confirms complete locked regions survive export exactly')
        mask=decode(download(page,'#export-mask'))
        assert mask[2][(30*960+30)*4:(30*960+30)*4+4]==bytes([0,0,0,255])
        assert mask[2][(300*960+800)*4:(300*960+800)*4+4]==bytes([255,255,255,255])
        board=download(page,'#export-board');assert decode(board)[0]==1384
        (OUT/'looklock-review-board.png').write_bytes(board)
        passed.append('mask polarity and three-panel review-board PNG exports are valid')
        saved=download(page,'#export-recipe');r=json.loads(saved);report=json.loads(download(page,'#export-report'))
        assert report['inferencePerformedByLookLock'] is False and report['modelProvenanceVerified'] is False
        assert report['metrics']['protectedMismatches']==0 and report['mediaIncluded'] is False and report['filenamesIncluded'] is False
        assert report['recipe']==r and len(report['outputRGBAHash'])==64
        passed.append('review report binds source/treatment/output hashes without media or filename disclosure')
        page.locator('#clear').click();expect(page.locator('#zone-list li')).to_have_count(0)
        page.locator('#undo').click();expect(page.locator('#zone-list li')).to_have_count(2)
        page.locator('#redo').click();expect(page.locator('#zone-list li')).to_have_count(0)
        upload(page,'#recipe-file','recipe.json',saved,'application/json');expect(page.locator('#zone-list li')).to_have_count(2)
        wrong={**r,'sourceHash':'f'*64}
        upload(page,'#recipe-file','wrong.json',json.dumps(wrong).encode(),'application/json')
        expect(page.locator('#status')).to_contain_text('different decoded frames');expect(page.locator('#zone-list li')).to_have_count(2)
        passed.append('undo/redo and hash-bound recipe restore; wrong-frame recipe rejection is transactional')
        page.locator('[data-mode=candidate]').click();raw=page.locator('#preview').evaluate('(c)=>c.toDataURL()')
        page.locator('[data-mode=output]').click();safe=page.locator('#preview').evaluate('(c)=>c.toDataURL()');assert raw!=safe
        page.locator('[data-mode=heatmap]').click();assert page.locator('#preview').evaluate('(c)=>c.toDataURL()')!=safe
        page.locator('[data-mode=split]').click();expect(page.locator('#split-row')).to_be_visible()
        page.locator('#split').evaluate("e=>{e.value='25';e.dispatchEvent(new Event('input',{bubbles:true}));}");expect(page.locator('#split-value')).to_have_text('25%')
        passed.append('original/treatment/protected/split/change-map controls show actual distinct pixels')
        page.locator('#draw').click();page.locator('#stage').scroll_into_view_if_needed();b=page.locator('#stage').bounding_box()
        page.mouse.move(b['x']+b['width']*.35,b['y']+b['height']*.4);page.mouse.down();page.mouse.move(b['x']+b['width']*.55,b['y']+b['height']*.6,steps=5);page.mouse.up()
        expect(page.locator('#zone-list li')).to_have_count(3);page.keyboard.press('Escape');expect(page.locator('#draw')).to_have_attribute('aria-pressed','false')
        passed.append('real pointer drawing adds a normalized source lock and Escape cancels drawing mode')
        page.locator('#coordinates summary').click();page.locator('#region-label').fill('<img src=x onerror=alert(1)>')
        for id,value in [('rect-x','300'),('rect-y','100'),('rect-w','20'),('rect-h','10')]:page.locator('#'+id).fill(value)
        page.locator('#coordinate-form button').click();expect(page.locator('#zone-list li')).to_have_count(4);assert page.locator('#zone-list img').count()==0
        page.locator('#rect-w').fill('9999');page.locator('#coordinate-form button').click();expect(page.locator('#status')).to_contain_text('inside the source');expect(page.locator('#zone-list li')).to_have_count(4)
        passed.append('keyboard pixel-coordinate locks validate bounds and render labels as text, not HTML')
        upload(page,'#source-file','original.png',png());expect(page.locator('#candidate-title')).to_have_text('Open treated frame');expect(page.locator('#zone-list li')).to_have_count(0)
        upload(page,'#candidate-file','treatment.png',png(96,64,40));expect(page.locator('#export-output')).to_be_disabled()
        page.locator('#aligned').check();expect(page.locator('#export-output')).to_be_enabled()
        page.locator('#origin').select_option('dlss5');desc=json.loads(download(page,'#export-report'));assert desc['treatmentOrigin']=='dlss5' and desc['modelProvenanceVerified'] is False
        passed.append('local pair import resets locks and requires human frame/crop confirmation; DLSS label remains user-described')
        for name,data,mime in [('wrong.png',png(97,64),'image/png'),('broken.png',b'not an image','image/png'),('script.svg',b'<svg/>','image/svg+xml')]:
            upload(page,'#candidate-file',name,data,mime);expect(page.locator('#candidate-title')).to_have_text('treatment.png');expect(page.locator('#export-output')).to_be_enabled()
        passed.append('wrong geometry, corrupt media, and unsupported formats preserve the working pair')
        page.locator('#blend').evaluate("e=>{e.value='0';e.dispatchEvent(new Event('input',{bubbles:true}));}")
        assert decode(download(page,'#export-output'))==decode(download(page,'#export-source'))
        page.locator('#blend').evaluate("e=>{e.value='100';e.dispatchEvent(new Event('input',{bubbles:true}));}");page.locator('#hud').click();page.locator('#subtitle').click();expect(page.locator('#zone-list li')).to_have_count(2)
        passed.append('zero blend exports the exact original and quick HUD/subtitle protections work')
        page.evaluate("""() => {const decode=HTMLImageElement.prototype.decode;let calls=0;HTMLImageElement.prototype.decode=async function(){await decode.call(this);if(++calls===1)await new Promise(r=>setTimeout(r,300));};} """)
        page.locator('#source-file').set_input_files({'name':'slow.png','mimeType':'image/png','buffer':png(96,64,7)})
        page.locator('#source-file').set_input_files({'name':'newest.png','mimeType':'image/png','buffer':png(96,64,8)})
        expect(page.locator('#source-title')).to_have_text('newest.png');page.wait_for_timeout(400);expect(page.locator('#source-title')).to_have_text('newest.png')
        passed.append('newest source wins out-of-order image decoding')
        page.evaluate("""() => {const decode=HTMLImageElement.prototype.decode;HTMLImageElement.prototype.decode=async function(){await decode.call(this);await new Promise(r=>setTimeout(r,500));};} """)
        page.locator('#source-file').set_input_files({'name':'cancelled.png','mimeType':'image/png','buffer':png(96,64,9)})
        page.locator('#cancel').click();page.wait_for_timeout(600);expect(page.locator('#source-title')).to_have_text('newest.png');expect(page.locator('#stage')).to_have_attribute('aria-busy','false')
        passed.append('cancelled image decode cannot replace the active source')
        # Locally generated animated video, not downloaded or inference-generated media.
        data=page.evaluate("""async () => {const c=document.createElement('canvas');c.width=320;c.height=180;const g=c.getContext('2d');g.fillStyle='#406070';g.fillRect(0,0,320,180);const stream=c.captureStream(12);const recorder=new MediaRecorder(stream,{mimeType:'video/webm'});const parts=[];recorder.ondataavailable=e=>parts.push(e.data);const done=new Promise(r=>recorder.onstop=r);recorder.start();for(let i=0;i<5;i++){g.fillStyle=i%2?'#805030':'#305080';g.fillRect(i*20,30,60,60);await new Promise(r=>setTimeout(r,80));}recorder.stop();await done;stream.getTracks().forEach(t=>t.stop());return Array.from(new Uint8Array(await new Blob(parts).arrayBuffer()));}""")
        upload(page,'#source-file','replay.webm',bytes(data),'video/webm');expect(page.locator('#video-dialog')).to_be_visible();expect(page.locator('#freeze')).to_be_enabled()
        page.locator('#close-video').click();expect(page.locator('#source-title')).to_have_text('newest.png');assert page.locator('#video').get_attribute('src') is None
        upload(page,'#source-file','replay.webm',bytes(data),'video/webm');page.locator('#freeze').click();expect(page.locator('#source-title')).to_contain_text('Video frame at');expect(page.locator('#dimensions')).to_have_text('320 × 180');expect(page.locator('#video-dialog')).not_to_be_visible()
        assert decode(download(page,'#export-source'))[:2]==(320,180) and page.locator('#video').get_attribute('src') is None
        passed.append('real local video decoding supports cancel, frame freeze, PNG export and source cleanup')
        page.locator('#demo').click();expect(page.locator('#stage')).to_have_attribute('aria-busy','false');expect(page.locator('#zone-list li')).to_have_count(2)
        for width in [320,390,768,1024]:
            page.set_viewport_size({'width':width,'height':900});assert page.evaluate('document.documentElement.scrollWidth <= innerWidth'),f'Horizontal overflow at {width}'
        page.set_viewport_size({'width':390,'height':844});page.screenshot(path=str(OUT/'looklock-mobile.png'),full_page=True)
        passed.append('320/390/768/1024 layouts have no horizontal overflow')
        touch_context=browser.new_context(viewport={'width':390,'height':844},has_touch=True,is_mobile=True)
        touch=touch_context.new_page();touch.goto(f'http://127.0.0.1:{PORT}/apps/looklock/index.html');expect(touch.locator('#stage')).to_have_attribute('aria-busy','false');touch.locator('#draw').click();touch.locator('#stage').scroll_into_view_if_needed();b=touch.locator('#stage').bounding_box();cdp=touch_context.new_cdp_session(touch)
        for kind,dx,dy in [('touchStart',.3,.3),('touchMove',.6,.6),('touchEnd',0,0)]:
            cdp.send('Input.dispatchTouchEvent',{'type':kind,'touchPoints':[] if kind=='touchEnd' else [{'x':b['x']+b['width']*dx,'y':b['y']+b['height']*dy}]})
        expect(touch.locator('#zone-list li')).to_have_count(3);touch_context.close()
        passed.append('real emulated touch gesture creates a region on the mobile preview')
        page.locator('#reset').click();expect(page.locator('#empty')).to_be_visible();expect(page.locator('#export-output')).to_be_disabled();expect(page.locator('#zone-list li')).to_have_count(0)
        assert page.evaluate('localStorage.length')==0
        assert not errors,errors;assert not violations,violations
        passed.append('session clear removes media/locks, nothing persists, and no page/CSP errors occurred')
        context.close();browser.close()
        with tempfile.TemporaryDirectory(prefix='looklock-extension-') as profile:
            extension=str(ROOT/'dist/extension')
            ctx=p.chromium.launch_persistent_context(profile,executable_path=exe,channel=None if exe else 'chromium',headless=True,args=['--no-sandbox',f'--disable-extensions-except={extension}',f'--load-extension={extension}'])
            service=ctx.service_workers[0] if ctx.service_workers else ctx.wait_for_event('serviceworker');eid=service.url.split('/')[2]
            popup=ctx.new_page();popup.goto(f'chrome-extension://{eid}/apps/extension/popup.html')
            with ctx.expect_page() as opened:popup.locator('#looklock').click()
            lab=opened.value;lab.wait_for_load_state();expect(lab.locator('#stage')).to_have_attribute('aria-busy','false');expect(lab.locator('#export-output')).to_be_enabled();assert '/apps/looklock/' in lab.url
            protected=decode(download(lab,'#export-output'));assert protected[:2]==(960,540)
            ctx.close()
        passed.append('actual MV3 popup launches LookLock and exports a protected PNG under extension CSP')
        (OUT/'looklock-report.json').write_text(json.dumps({'passed':passed,'count':len(passed),'neuralInferenceTested':False,'inferencePerformedByLookLock':False,'physicalGpuCertified':False,'scope':'Real Chromium, synthetic/local media, Canvas2D composition, independent PNG byte validation, actual MV3 entry point.'},indent=2))
        print(json.dumps({'passed':passed},indent=2))
finally:server.terminate();server.wait(timeout=10)
