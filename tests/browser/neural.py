"""NR lab integration and real WebGPU wrapper tests. No model inference/parity or physical-GPU claim."""
import hashlib
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
ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'artifacts'; OUT.mkdir(exist_ok=True)
PORT = 4184

def png(width=96, height=64):
    def chunk(name, data):
        return struct.pack('!I',len(data))+name+data+struct.pack('!I',zlib.crc32(name+data)&0xffffffff)
    raw=b''.join(b'\0'+bytes((64,128,192,128))*width for _ in range(height))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!2I5B',width,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')

results=[]; errors=[]; console=[]
server=subprocess.Popen(['node','scripts/serve.mjs'],cwd=ROOT,env={**os.environ,'PORT':str(PORT)},stdout=subprocess.DEVNULL,stderr=subprocess.STDOUT)
try:
    for _ in range(100):
        try:
            urlopen(f'http://127.0.0.1:{PORT}',timeout=.2).close();break
        except OSError:time.sleep(.1)
    else:raise RuntimeError('NR test server did not start')
    with sync_playwright() as p:
        executable=os.environ.get('CHROMIUM_PATH') or (None if os.environ.get('CI') else shutil.which('chromium'))
        # Use SwANGLE for browser composition and SwiftShader for Dawn independently.
        # Forcing all Chromium composition through Vulkan can lose imported canvas surfaces.
        browser=p.chromium.launch(executable_path=executable,channel=None if executable else 'chromium',headless=True,args=[
            '--no-sandbox','--enable-unsafe-webgpu','--use-angle=swiftshader',
            '--enable-unsafe-swiftshader','--use-webgpu-adapter=swiftshader'])
        page=browser.new_page(viewport={'width':1440,'height':1050},accept_downloads=True)
        page.on('pageerror',lambda error:errors.append(str(error)))
        page.on('console',lambda message:console.append(f'{message.type}: {message.text}'))
        page.goto(f'http://127.0.0.1:{PORT}/apps/nr/index.html')
        expect(page.locator('#availability')).to_have_text('Neural inference locked in this build')
        expect(page.locator('#prepare')).to_be_disabled();expect(page.locator('#live')).to_be_disabled()
        expect(page.locator('#output-mount canvas')).to_have_count(0)
        results.append('empty approval register leaves no simulated neural output or active inference')
        page.locator('#media-file').set_input_files({'name':'private-original.png','mimeType':'image/png','buffer':png()})
        expect(page.locator('#source-stage img')).to_have_count(1)
        expect(page.locator('#source-info')).to_contain_text('96 × 64')
        original=page.locator('#source-stage img').get_attribute('src')
        page.locator('#media-file').set_input_files({'name':'broken.png','mimeType':'image/png','buffer':b'not a PNG'})
        expect(page.locator('#status')).to_have_class('error')
        assert page.locator('#source-stage img').get_attribute('src')==original
        results.append('original image works without weights; decoding failure preserves it')
        page.evaluate("() => {navigator.mediaDevices.getDisplayMedia = async () => {throw new DOMException('Consent denied','NotAllowedError');};}")
        page.locator('#share').click();expect(page.locator('#status')).to_contain_text('Current source preserved')
        assert page.locator('#source-stage img').get_attribute('src')==original
        page.evaluate("""() => { navigator.mediaDevices.getDisplayMedia = () => new Promise(resolve => {
          const c=document.createElement('canvas');c.width=96;c.height=64;c.getContext('2d').fillRect(0,0,96,64);
          const stream=c.captureStream(1); window.__lateTrack=stream.getVideoTracks()[0];
          setTimeout(()=>resolve(stream),200); }); }""")
        page.locator('#share').click();page.locator('#cancel').click();page.wait_for_timeout(400)
        assert page.evaluate("() => window.__lateTrack.readyState")=='ended'
        assert page.locator('#source-stage img').get_attribute('src')==original
        results.append('capture denial and cancelled late capture preserve original and stop tracks')
        with tempfile.TemporaryDirectory() as tmp:
            folder=Path(tmp)/'test-model';(folder/'model/stages').mkdir(parents=True)
            data=bytes((0,1,2,3));(folder/'model/stages/test.bin').write_bytes(data)
            manifest={'totals':{'blockCount':71},'stages':[{'id':0,'file':'stages/test.bin','packedByteLength':4,'sha256':hashlib.sha256(data).hexdigest()}],
                'tensors':[{'name':'block70.layer0.blend_scale','block':70,'layer':0,'stage':0,'stageOffset':0,'byteLength':2}]}
            (folder/'manifest.json').write_text(json.dumps(manifest))
            page.locator('#model-files').set_input_files(str(folder))
            expect(page.locator('#model-approval')).to_have_text('Not approved in this build')
            expect(page.locator('#model-status')).to_contain_text('Metadata inspected only')
            expect(page.locator('#prepare')).to_be_disabled()
            expect(page.locator('#model-hash')).to_have_text(hashlib.sha256((folder/'manifest.json').read_bytes()).hexdigest())
            results.append('local model metadata inspected with SHA-256; selection does not authorize inference')
        page.locator('#probe').click();expect(page.locator('#gpu-status')).not_to_contain_text('Not checked',timeout=30000)
        assert any(s in page.locator('#gpu-status').inner_text() for s in ['Candidate adapter.','Unavailable.'])
        results.append('actual worker capability probe returns without enabling an unapproved model')
        with page.expect_download() as download:page.locator('#report').click()
        report=json.loads(Path(download.value.path()).read_text());assert report['frames']==0 and report['approved'] is False
        assert 'private-original' not in json.dumps(report) and report['sourceNamesIncluded'] is False
        assert report['modelInferenceValidatedHere'] is False
        results.append('diagnostics report distinguishes adapter presence from model inference')
        page.screenshot(path=str(OUT/'neural-desktop.png'),full_page=True)
        for width in [320,390,768]:
            page.set_viewport_size({'width':width,'height':900})
            assert page.evaluate('() => document.documentElement.scrollWidth <= window.innerWidth'),f'Horizontal overflow at {width}'
        page.set_viewport_size({'width':390,'height':900});page.screenshot(path=str(OUT/'neural-mobile.png'),full_page=True)
        page.locator('#unload').click();expect(page.locator('#model-info')).to_be_hidden()
        page.keyboard.press('Escape');expect(page.locator('#source-stage img')).to_have_count(0)
        expect(page.locator('#stop')).to_be_disabled()
        results.append('320/390/768 layouts and explicit release controls remain usable')
        # Actual GPU wrapper test with synthetic residuals, NOT trained-model inference.
        gpu_result=page.evaluate("""async () => {
          const {FramePipeline}=await import('/packages/nr/engine.js');
          const {SHADERS,geometryFromValid}=await import('/vendor/opendlss/api.js');
          const adapter=await navigator.gpu?.requestAdapter();if(!adapter)throw Error('WebGPU test adapter unavailable');
          console.info('NR GPU adapter: '+JSON.stringify(adapter.info));
          const device=await adapter.requestDevice();const g=geometryFromValid(64,48);
          device.addEventListener('uncapturederror',e=>console.error('NR GPU validation: '+e.error.message));
          device.lost.then(info=>console.info('NR GPU device state: '+info.reason+' '+info.message));
          const head=device.createBuffer({size:g.fullRows*4*4,usage:GPUBufferUsage.STORAGE|GPUBufferUsage.COPY_DST});
          const features=device.createBuffer({size:g.fullRows*16*4,usage:GPUBufferUsage.STORAGE|GPUBufferUsage.COPY_SRC});
          const canvas=document.createElement('canvas');document.body.append(canvas);
          const renderer=await FramePipeline.create(canvas,device,g,features,head,SHADERS);
          console.info('NR GPU pipelines created');
          const input=document.createElement('canvas');input.width=64;input.height=48;
          const ctx=input.getContext('2d');ctx.fillStyle='rgba(64,128,192,0.5)';ctx.fillRect(0,0,64,48);
          const source=ctx.getImageData(0,0,1,1).data;
          const headData=new Float32Array(g.fullRows*4);for(let i=0;i<headData.length;i+=4){headData[i]=.4;headData[i+1]=-.2;}
          device.queue.writeBuffer(head,0,headData);
          const image=await createImageBitmap(input);
          const ms=await renderer.process(image,{tone:.5,structure:.75,split:0},()=>{});image.close();
          console.info('NR GPU ImageBitmap frame completed');
          const read=()=>{const c=document.createElement('canvas');c.width=64;c.height=48;const x=c.getContext('2d');x.drawImage(canvas,0,0);return Array.from(x.getImageData(32,24,1,1).data);};
          const enhanced=read();
          if(Math.abs(enhanced[0]-(source[0]+25.5))>4 || Math.abs(enhanced[1]-(source[1]-12.75))>4 || Math.abs(enhanced[3]-source[3])>2)throw Error('Residual/alpha mismatch: '+enhanced);
          await renderer.present({tone:.5,structure:.75,split:1});const original=read();
          if(original.some((v,i)=>Math.abs(v-source[i])>3))throw Error('Original comparison mismatch: '+original);
          const rb=device.createBuffer({size:g.fullRows*16*4,usage:GPUBufferUsage.COPY_DST|GPUBufferUsage.MAP_READ});
          const encoder=device.createCommandEncoder();encoder.copyBufferToBuffer(features,0,rb,0,g.fullRows*16*4);device.queue.submit([encoder.finish()]);await rb.mapAsync(GPUMapMode.READ);
          const values=new Float32Array(rb.getMappedRange());
          if(!values.every(Number.isFinite))throw Error('Non-finite preprocessing, including padded pixels');
          for(const pixel of [0,63,64,g.fullRows-1]){const off=pixel*16;
            if(values[off+3]!==1 || values[off+11]!==.5 || values[off+12]!==1 || values[off+13]!==.75 || values[off+15]!==0)throw Error('Feature-lane/temporal contract mismatch');
            for(let c=0;c<3;c++)if(Math.abs(values[off+4+c]-(source[c]/255-.5)*.125)>.0003)throw Error('Proxy normalization mismatch');
          }
          rb.unmap();rb.destroy();
          const frame=new VideoFrame(input,{timestamp:0});await renderer.process(frame,{tone:.5,structure:.75,split:0},()=>{});frame.close();
          console.info('NR GPU VideoFrame completed');
          headData.fill(NaN);device.queue.writeBuffer(head,0,headData);
          const bad=await createImageBitmap(input);let rejected=false;
          try{await renderer.process(bad,{tone:0,structure:0,split:0},()=>{});}catch(e){rejected=String(e).includes('non-finite');}finally{bad.close();}
          if(!rejected)throw Error('Invalid model output was not rejected');
          renderer.destroy();head.destroy();features.destroy();device.destroy();canvas.remove();
          return {softwareAdapter:adapter.info?.description||'test adapter',input:Array.from(source),original,enhanced,completionMs:ms,
            scope:'synthetic residual and real frame wrapper only; full neural inference and parity NOT tested'};
        }""")
        results.append('real WebGPU preprocessing/composition, alpha, padded features, VideoFrame textures and non-finite-output rejection')
        worker_result=page.evaluate("""async () => {
          const {NeuralClient}=await import('/packages/nr/client.js');const c=new NeuralClient(),canvas=new OffscreenCanvas(64,48);
          const manifestBytes=new TextEncoder().encode('{}').buffer;
          try{await c.call('prepare',{canvas,width:64,height:48,model:{manifestBytes,manifestSha256:'0'.repeat(64),stages:new Map()}},[canvas,manifestBytes],10000);return 'BAD';}
          catch(e){return String(e);}finally{c.stop();}
        }""")
        assert 'no repository-reviewed' in worker_result,worker_result
        results.append('actual worker denies unapproved model before GPU graph construction')
        assert not errors,errors
        (OUT/'neural-report.json').write_text(json.dumps({'passed':len(results),'scenarios':results,'gpuWrapper':gpu_result,
          'fullNeuralInferenceTested':False,'proprietaryWeightsUsed':False,'physicalGpuCertified':False},indent=2))
        browser.close()
        print(json.dumps({'passed':len(results),'scenarios':results},indent=2))
finally:
    (OUT/'neural-console.json').write_text(json.dumps({'console':console,'pageErrors':errors},indent=2))
    print('\n'.join(console[-20:]),flush=True)
    server.terminate()
    try:server.wait(timeout=5)
    except subprocess.TimeoutExpired:server.kill()
