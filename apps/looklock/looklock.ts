import { compose, bounds, geometry, rasterHash, recipe, parseRecipe, zones, History, DEFAULTS, MAX_ZONES, type Raster, type Zone, type Adjustments, type Identity } from '../../packages/looklock/core.js';
import { encodePNG } from '../../packages/looklock/png.js';
function el<T extends HTMLElement>(id: string): T { const node=document.getElementById(id); if(!node) throw new Error(`Missing element ${id}`); return node as T; }
const canvas=el<HTMLCanvasElement>('preview'),stage=el('stage'),overlay=document.getElementById('zones-overlay')!;
const video=el<HTMLVideoElement>('video'), dialog=el<HTMLDialogElement>('video-dialog');
let source: Raster|null=null,candidate: Raster|null=null,sourceHash='',candidateHash='';
let history=new History<Zone[]>([]), config:Adjustments={...DEFAULTS}, mode='output', split=.5, drawing=false;
let result:ReturnType<typeof compose>|null=null, originalCanvas:HTMLCanvasElement|null=null,treatmentCanvas:HTMLCanvasElement|null=null;
let outputCanvas:HTMLCanvasElement|null=null,heatCanvas:HTMLCanvasElement|null=null, sourceKind='synthetic-demo',frameTime:number|null=null;
let job=0,busy=false,scheduled=0,videoUrl:string|null=null;
let operation:AbortController|null=null;
let drag:{x:number;y:number;pointer:number}|null=null;
const pairButtons=['export-output','export-board','export-mask','export-recipe','export-report'];
const labels:Record<string,string>={source:'Original · decoded source pixels',candidate:'Raw treatment · provenance not verified',output:'Protected composite · locked regions use exact source pixels',split:'Original / protected composite',heatmap:'Raw treatment differences · NOT an AI or quality detector'};
function status(text:string,error=false) {el('status').textContent=text;el('status').classList.toggle('error',error);}
function message(error:unknown) {return error instanceof Error?error.message:'This operation could not be completed.';}
function setBusy(value:boolean) {busy=value;el('busy-overlay').hidden=!value;stage.setAttribute('aria-busy',String(value));controls();}
function controls() {
  const paired=!!source&&!!candidate,aligned=el<HTMLInputElement>('aligned').checked;
  for(const id of pairButtons) el<HTMLButtonElement>(id).disabled=!paired||!aligned||busy;
  el<HTMLButtonElement>('import-recipe').disabled=!paired||busy;
  el<HTMLButtonElement>('export-source').disabled=!source||busy;
  for(const id of ['draw','subtitle','hud','clear']) el<HTMLButtonElement>(id).disabled=!source||busy;
  el<HTMLButtonElement>('open-candidate').disabled=!source||busy;
  el<HTMLButtonElement>('undo').disabled=!history.canUndo||busy;el<HTMLButtonElement>('redo').disabled=!history.canRedo||busy;
  el<HTMLInputElement>('aligned').disabled=!paired||busy;
  el<HTMLSelectElement>('origin').disabled=!paired||busy||el<HTMLSelectElement>('origin').value==='synthetic';
  el('zone-count').textContent=`${history.value.length} / ${MAX_ZONES}`;
  el('split-row').hidden=mode!=='split';el('split-value').textContent=`${Math.round(split*100)}%`;
  for(const button of document.querySelectorAll<HTMLButtonElement>('[data-mode]')) {button.setAttribute('aria-pressed',String(button.dataset.mode===mode));button.disabled=!source||busy||(!paired&&button.dataset.mode!=='source');}
  for(const key of ['blend','feather','threshold'] as const) {el<HTMLInputElement>(key).value=String(config[key]*(key==='blend'?100:1));el<HTMLInputElement>(key).disabled=!paired||busy;}
  el('blend-value').textContent=`${Math.round(config.blend*100)}%`;el('feather-value').textContent=`${config.feather} px`;el('threshold-value').textContent=`${config.threshold} / 255`;
  el('draw').setAttribute('aria-pressed',String(drawing));el('draw').textContent=drawing?'Drawing active · drag on preview':'Draw a protected region';stage.classList.toggle('drawing',drawing);
  el('pair-badge').textContent=!paired?'ORIGINAL ONLY':el<HTMLSelectElement>('origin').value==='synthetic'?'SYNTHETIC DEMO':aligned?'USER-MATCHED PAIR':'CONFIRM FRAME MATCH';
}
function asCanvas(r:Raster) {const c=document.createElement('canvas');c.width=r.width;c.height=r.height;c.getContext('2d')!.putImageData(new ImageData(r.data,r.width,r.height),0,0);return c;}
function fromCanvas(c:HTMLCanvasElement):Raster {return {width:c.width,height:c.height,data:c.getContext('2d',{willReadFrequently:true})!.getImageData(0,0,c.width,c.height).data};}
function present() {
  const ctx=canvas.getContext('2d')!;ctx.clearRect(0,0,canvas.width,canvas.height);if(!originalCanvas){el('view-caption').textContent='No active frame';return;}
  const view=mode==='candidate'?treatmentCanvas:mode==='heatmap'?heatCanvas:mode==='source'?originalCanvas:outputCanvas;
  ctx.drawImage(view??originalCanvas,0,0);
  if(mode==='split'&&outputCanvas){ctx.save();ctx.beginPath();ctx.rect(0,0,Math.round(canvas.width*split),canvas.height);ctx.clip();ctx.drawImage(originalCanvas,0,0);ctx.restore();ctx.fillStyle='#d7f5db';ctx.fillRect(Math.round(canvas.width*split),0,Math.max(1,canvas.width/1000),canvas.height);}
  el('view-caption').textContent=labels[mode]??labels.source!;controls();
}
function refresh() {
  if(scheduled)cancelAnimationFrame(scheduled);scheduled=0;
  if(source&&candidate){result=compose(source,candidate,history.value,config);outputCanvas=asCanvas(result.output);heatCanvas=asCanvas(result.heatmap);
    el('protected-pixels').textContent=result.metrics.protectedPixels.toLocaleString();el('mismatches').textContent=String(result.metrics.protectedMismatches);
    el('changed-pixels').textContent=`${(result.metrics.rawChanged/result.metrics.pixels*100).toFixed(1)}%`;
  } else {result=null;outputCanvas=null;heatCanvas=null;for(const id of ['protected-pixels','mismatches','changed-pixels'])el(id).textContent='—';}
  present();renderZones();controls();
}
function schedule() {if(!scheduled)scheduled=requestAnimationFrame(()=>{scheduled=0;refresh();});}
function drawRect(z:Zone,index:number,pending=false) {
  if(!source)return;const b=bounds(z,source.width,source.height),ns='http://www.w3.org/2000/svg';
  const r=document.createElementNS(ns,'rect');for(const [key,value] of Object.entries({x:b.x,y:b.y,width:b.right-b.x,height:b.bottom-b.y}))r.setAttribute(key,String(value));if(pending)r.classList.add('pending');overlay.append(r);
  if(!pending){const t=document.createElementNS(ns,'text');t.setAttribute('x',String(b.x+5));t.setAttribute('y',String(Math.min(source.height-4,b.y+16)));t.textContent=`${index+1} · SOURCE LOCK`;overlay.append(t);}
}
function renderZones(pending?:Zone) {overlay.replaceChildren();history.value.forEach((z,i)=>drawRect(z,i));if(pending)drawRect(pending,0,true);}
function listZones() {
  const list=el('zone-list');list.replaceChildren();history.value.forEach((z,i)=>{
    const li=document.createElement('li'),n=document.createElement('span'),body=document.createElement('span'),title=document.createElement('strong'),size=document.createElement('small'),remove=document.createElement('button');
    n.className='zone-index';n.textContent=String(i+1);body.className='zone-label';title.textContent=z.label||`Region ${i+1}`;
    if(source){const b=bounds(z,source.width,source.height);size.textContent=`${b.right-b.x} × ${b.bottom-b.y} at ${b.x}, ${b.y}`;}
    remove.textContent='×';remove.setAttribute('aria-label',`Remove ${z.label||'region'} ${i+1}`);remove.addEventListener('click',()=>commitZones(history.value.filter(item=>item.id!==z.id)));
    body.append(title,size);li.append(n,body,remove);list.append(li);
  });
}
function commitZones(value:Zone[]) {try{history.set(zones(value));listZones();refresh();status('Source locks updated. Review the composite before exporting.');}catch(error){status(message(error),true);}}
function addZone(x:number,y:number,width:number,height:number,label:string) {
  if(!source||busy)return; if(![x,y,width,height].every(Number.isInteger)||x<0||y<0||width<1||height<1||x+width>source.width||y+height>source.height){status('Enter whole-pixel coordinates entirely inside the source frame.',true);return;}
  commitZones([...history.value,{id:crypto.randomUUID(),label,x:x/source.width,y:y/source.height,width:width/source.width,height:height/source.height}]);
}
function adjustStage() {if(!source)return;canvas.width=source.width;canvas.height=source.height;stage.style.aspectRatio=String(source.width/source.height);stage.style.maxWidth=source.width<source.height?`${480*source.width/source.height}px`:'none';overlay.setAttribute('viewBox',`0 0 ${source.width} ${source.height}`);el('dimensions').textContent=`${source.width} × ${source.height}`;}
function releaseVideo() {video.pause();video.removeAttribute('src');video.load();if(videoUrl)URL.revokeObjectURL(videoUrl);videoUrl=null;el<HTMLButtonElement>('freeze').disabled=true;}
function cancel() {job++;operation?.abort();operation=null;setBusy(false);drag=null;renderZones();if(dialog.open)dialog.close();releaseVideo();status('Cancelled. The current frame pair is unchanged.');}
async function installSource(r:Raster,title:string,kind:string,time:number|null,ticket:number) {
  const hash=await rasterHash(r);if(ticket!==job)return;
  source=r;sourceHash=hash;sourceKind=kind;frameTime=time;candidate=null;candidateHash='';result=null;history=new History<Zone[]>([]);drawing=false;drag=null;
  originalCanvas=asCanvas(r);treatmentCanvas=null;mode='source';el('source-title').textContent=title;el('source-meta').textContent=`${r.width} × ${r.height}${time===null?'':` · ${time.toFixed(3)} s`}`;
  el('candidate-title').textContent='Open treated frame';el('candidate-meta').textContent='Same dimensions, frame, and crop';el<HTMLInputElement>('aligned').checked=false;el<HTMLSelectElement>('origin').value='external';
  el('empty').hidden=true;adjustStage();listZones();refresh();status('Original loaded. Save this frame for your renderer, then import its same-sized result.');
}
function mediaKind(file:File) {
  const extension=file.name.toLowerCase().split('.').pop()??'';
  const mime=file.type||({png:'image/png',jpg:'image/jpeg',jpeg:'image/jpeg',webp:'image/webp',avif:'image/avif',mp4:'video/mp4',webm:'video/webm',ogv:'video/ogg'} as Record<string,string>)[extension]||'';
  if(/^image\/(png|jpeg|webp|avif)$/.test(mime))return 'image';if(/^video\/(mp4|webm|ogg)$/.test(mime))return 'video';throw new Error('Choose a PNG, JPEG, WebP, AVIF, MP4, WebM or Ogg file.');
}
async function decodeImage(file:File,signal:AbortSignal):Promise<Raster> {
  const url=URL.createObjectURL(file),img=new Image();
  try {
    img.src=url;
    await new Promise<void>((resolve,reject)=>{
      let finished=false;
      const finish=(error?:unknown)=>{if(finished)return;finished=true;clearTimeout(timer);signal.removeEventListener('abort',aborted);error?reject(error):resolve();};
      const aborted=()=>finish(new Error('Image loading cancelled.'));
      const timer=setTimeout(()=>finish(new Error('Image decoding timed out.')),15000);
      signal.addEventListener('abort',aborted,{once:true});
      if(signal.aborted){aborted();return;}void img.decode().then(()=>finish(),finish);
    });
    geometry(img.naturalWidth,img.naturalHeight);
    const c=document.createElement('canvas');c.width=img.naturalWidth;c.height=img.naturalHeight;c.getContext('2d')!.drawImage(img,0,0);return fromCanvas(c);
  } finally {img.src='';URL.revokeObjectURL(url);}
}
async function openFile(file:File,which:'source'|'candidate') {
  const ticket=++job;operation?.abort();operation=new AbortController();const signal=operation.signal;
  let kind:string;try{kind=mediaKind(file);if(file.size===0||file.size>(kind==='image'?50*1024**2:2*1024**3))throw new Error('Choose a nonempty image below 50 MiB or video below 2 GiB.');if(which==='candidate'&&kind!=='image')throw new Error('Import one treated image, not a video. Use the exact matching frame.');}catch(error){operation=null;setBusy(false);status(message(error),true);return;}
  setBusy(true);status('Decoding on your device. Your current pair is preserved until loading succeeds.');
  try {
    if(kind==='video'){releaseVideo();videoUrl=URL.createObjectURL(file);video.src=videoUrl;video.load();dialog.showModal();
      await new Promise<void>((resolve,reject)=>{const done=(error?:Error)=>{clearTimeout(timer);video.removeEventListener('loadeddata',ready);video.removeEventListener('error',failed);signal.removeEventListener('abort',aborted);error?reject(error):resolve();};const aborted=()=>done(new Error('Video loading cancelled.'));const ready=()=>done(),failed=()=>done(new Error('This browser could not decode the video.'));const timer=setTimeout(()=>done(new Error('Video decoding timed out.')),15000);video.addEventListener('loadeddata',ready,{once:true});video.addEventListener('error',failed,{once:true});signal.addEventListener('abort',aborted,{once:true});if(signal.aborted)aborted();else if(video.readyState>=2)done();});
      if(ticket!==job)return;geometry(video.videoWidth,video.videoHeight);el<HTMLButtonElement>('freeze').disabled=false;status('Seek to the matching frame, then choose Use this frame.');return;
    }
    const r=await decodeImage(file,signal);if(ticket!==job)return;
    if(which==='source')await installSource(r,file.name,'local-image',null,ticket);
    else{if(!source||r.width!==source.width||r.height!==source.height)throw new Error('Treatment dimensions must exactly match the original. No automatic resizing or alignment is performed.');const hash=await rasterHash(r);if(ticket!==job)return;
      candidate=r;candidateHash=hash;treatmentCanvas=asCanvas(r);el('candidate-title').textContent=file.name;el('candidate-meta').textContent=`${r.width} × ${r.height} · provenance unverified`;el<HTMLInputElement>('aligned').checked=false;el<HTMLSelectElement>('origin').value='external';mode='split';refresh();status('Treatment loaded. Confirm the same frame and crop before export.');}
  }catch(error){if(ticket===job){if(dialog.open)dialog.close();releaseVideo();status(message(error),true);}}
  finally{if(ticket===job){operation=null;setBusy(false);}}
}
async function freeze() {
  if(video.readyState<2||video.seeking)return;video.pause();const time=video.currentTime;const c=document.createElement('canvas');c.width=video.videoWidth;c.height=video.videoHeight;
  try{geometry(c.width,c.height);c.getContext('2d')!.drawImage(video,0,0);const ticket=++job;setBusy(true);dialog.close();releaseVideo();await installSource(fromCanvas(c),`Video frame at ${time.toFixed(3)} s`,'video-frame',time,ticket);if(ticket===job)setBusy(false);}catch(error){setBusy(false);status(message(error),true);}
}
function save(blob:Blob,name:string){const url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),3000);}
function identity():Identity{if(!source||!candidate)throw new Error('Load an original and a treatment first.');return {width:source.width,height:source.height,sourceHash,candidateHash};}
function ready(){if(busy||!source||!candidate||!el<HTMLInputElement>('aligned').checked)throw new Error('Load and confirm a matching pair first.');refresh();if(!result||result.metrics.protectedMismatches!==0)throw new Error('Protected pixel check failed; export was blocked.');return result;}
function action(id:string,fn:()=>void|Promise<void>){el(id).addEventListener('click',()=>{try{void Promise.resolve(fn()).catch(error=>status(message(error),true));}catch(error){status(message(error),true);}});}
function makeBoard(){const r=ready(),w=source!.width,h=source!.height,pw=440,ph=Math.max(160,Math.min(350,pw*h/w));const c=document.createElement('canvas');c.width=3*pw+64;c.height=ph+190;const ctx=c.getContext('2d')!;
  ctx.fillStyle='#101918';ctx.fillRect(0,0,c.width,c.height);ctx.fillStyle='#c5e7cf';ctx.font='bold 24px sans-serif';ctx.fillText('LOOKLOCK / CREATOR REVIEW',16,38);ctx.font='12px sans-serif';ctx.fillStyle='#bac6c0';ctx.fillText('Still-frame comparison. Imported treatment provenance is not independently verified.',16,63);
  const images=[originalCanvas!,treatmentCanvas!,outputCanvas!],titles=['01 ORIGINAL','02 RAW TREATMENT','03 PROTECTED COMPOSITE'];
  for(let i=0;i<3;i++){const x=16+i*(pw+16);ctx.fillStyle='#0a0f14';ctx.fillRect(x,82,pw,ph);const scale=Math.min(pw/w,ph/h),dw=w*scale,dh=h*scale;ctx.drawImage(images[i]!,x+(pw-dw)/2,82+(ph-dh)/2,dw,dh);ctx.fillStyle='#d8eadd';ctx.font='bold 12px sans-serif';ctx.fillText(titles[i]!,x,ph+108);}
  ctx.fillStyle='#a7b7ad';ctx.font='12px sans-serif';ctx.fillText(`${history.value.length} regions | ${r.metrics.protectedPixels.toLocaleString()} source-locked pixels | ${r.metrics.protectedMismatches} mismatches | ${Math.round(config.blend*100)}% treatment blend`,16,ph+139);
  ctx.fillText(el<HTMLSelectElement>('origin').value==='synthetic'?'SYNTHETIC DEMO. No neural inference was performed.':'USER-MATCHED PAIR. No automatic alignment or model verification. Selected regions only.',16,ph+163);
  return fromCanvas(c);
}
function demo(){job++;operation?.abort();operation=null;releaseVideo();if(dialog.open)dialog.close();setBusy(false);const s=document.createElement('canvas');s.width=960;s.height=540;const ctx=s.getContext('2d')!;
  const sky=ctx.createLinearGradient(0,0,960,540);sky.addColorStop(0,'#152a46');sky.addColorStop(.6,'#23334b');sky.addColorStop(1,'#956856');ctx.fillStyle=sky;ctx.fillRect(0,0,960,540);
  let seed=91;for(let i=0;i<160;i++){seed=(Math.imul(seed,1664525)+1013904223)>>>0;const x=seed%960;seed=(Math.imul(seed,1664525)+1013904223)>>>0;const y=seed%420;ctx.fillStyle=i%3?'#c8d9d9':'#7895b9';ctx.fillRect(x,y,i%4===0?2:1,1);}
  const moon=ctx.createRadialGradient(675,140,4,725,170,109);moon.addColorStop(0,'#b6bbb8');moon.addColorStop(1,'#4a5a6f');ctx.fillStyle=moon;ctx.beginPath();ctx.arc(730,174,104,0,Math.PI*2);ctx.fill();ctx.strokeStyle='#c7c0b078';ctx.lineWidth=1;for(let i=0;i<10;i++){ctx.beginPath();ctx.ellipse(730,174,106+i*6,23+i*3,-.3,0,Math.PI*2);ctx.stroke();}
  ctx.fillStyle='#101e32';ctx.beginPath();ctx.moveTo(0,450);for(let x=0;x<=960;x+=40)ctx.lineTo(x,365+Math.sin(x/115)*46+Math.cos(x/40)*20);ctx.lineTo(960,540);ctx.lineTo(0,540);ctx.fill();
  ctx.fillStyle='#adbbb9';ctx.beginPath();ctx.moveTo(400,238);ctx.lineTo(535,332);ctx.lineTo(405,306);ctx.lineTo(294,352);ctx.closePath();ctx.fill();ctx.fillStyle='#394f68';ctx.beginPath();ctx.moveTo(400,238);ctx.lineTo(432,297);ctx.lineTo(375,314);ctx.closePath();ctx.fill();ctx.fillStyle='#9acbc3';ctx.fillRect(372,319,37,4);
  const hud=(score:string,caption:string)=>{ctx.fillStyle='#0c1629ee';ctx.fillRect(22,20,230,92);ctx.fillStyle='#a9c8d0';ctx.font='11px monospace';ctx.fillText('ORBITAL RUN / FICTIONAL GAME',35,43);ctx.fillStyle='#ecf4ee';ctx.font='bold 22px monospace';ctx.fillText(`SCORE  ${score}`,35,74);ctx.fillStyle='#99b2bf';ctx.font='11px monospace';ctx.fillText('SECTOR 07       REPLAY',35,97);ctx.fillStyle='#101b28e8';ctx.fillRect(175,466,610,45);ctx.fillStyle='#e4eade';ctx.font='17px sans-serif';ctx.textAlign='center';ctx.fillText(caption,480,495);ctx.textAlign='left';};
  hud('1280','Approach the landing zone.');const original=fromCanvas(s);
  ctx.fillStyle='#bb8b3a25';ctx.fillRect(0,0,960,540);hud('1980','Approach the loading zone.');const treatment=fromCanvas(s),ticket=job;
  setBusy(true);void Promise.all([rasterHash(original),rasterHash(treatment)]).then(([sh,ch])=>{if(ticket!==job)return;source=original;candidate=treatment;sourceHash=sh;candidateHash=ch;sourceKind='synthetic-demo';frameTime=null;originalCanvas=asCanvas(original);treatmentCanvas=asCanvas(treatment);config={...DEFAULTS};history=new History<Zone[]>([{id:'hud-demo',label:'Score / HUD',x:20/960,y:18/540,width:235/960,height:98/540},{id:'caption-demo',label:'Replay caption',x:170/960,y:460/540,width:620/960,height:57/540}]);mode='output';drawing=false;drag=null;el('source-title').textContent='Orbital Run · original';el('candidate-title').textContent='Synthetic edited treatment';el('source-meta').textContent='960 × 540 · fictional game replay';el('candidate-meta').textContent='Deliberately altered text · not neural';el<HTMLSelectElement>('origin').value='synthetic';el<HTMLInputElement>('aligned').checked=true;el('empty').hidden=true;adjustStage();listZones();refresh();status('Synthetic demo: the treatment changes 1280 to 1980 and alters the caption. Source locks restore both. No neural inference was used.');}).catch(error=>status(message(error),true)).finally(()=>{if(ticket===job)setBusy(false);});
}
action('open-source',()=>el<HTMLInputElement>('source-file').click());action('open-candidate',()=>el<HTMLInputElement>('candidate-file').click());
for(const which of ['source','candidate'] as const)el<HTMLInputElement>(`${which}-file`).addEventListener('change',()=>{const input=el<HTMLInputElement>(`${which}-file`),file=input.files?.[0];input.value='';if(file)void openFile(file,which);});
action('demo',demo);action('demo-footer',demo);action('cancel',cancel);action('close-video',cancel);action('freeze',freeze);dialog.addEventListener('cancel',e=>{e.preventDefault();cancel();});
video.addEventListener('seeking',()=>{el<HTMLButtonElement>('freeze').disabled=true;});video.addEventListener('seeked',()=>{el<HTMLButtonElement>('freeze').disabled=video.readyState<2;});
action('draw',()=>{drawing=!drawing;drag=null;renderZones();controls();status(drawing?'Drag across the image to source-lock a region. Escape cancels drawing.':'Drawing off. Existing locks are unchanged.');});
action('subtitle',()=>{if(source)addZone(0,Math.floor(source.height*.82),source.width,source.height-Math.floor(source.height*.82),'Subtitle band');});
action('hud',()=>{if(source)addZone(0,0,Math.max(1,Math.floor(source.width*.28)),Math.max(1,Math.floor(source.height*.22)),'HUD corner');});
action('clear',()=>commitZones([]));for(const op of ['undo','redo'] as const)action(op,()=>{history[op]();listZones();refresh();});
el('coordinate-form').addEventListener('submit',e=>{e.preventDefault();addZone(Number(el<HTMLInputElement>('rect-x').value),Number(el<HTMLInputElement>('rect-y').value),Number(el<HTMLInputElement>('rect-w').value),Number(el<HTMLInputElement>('rect-h').value),el<HTMLInputElement>('region-label').value);});
function point(e:PointerEvent){const r=stage.getBoundingClientRect();return {x:Math.max(0,Math.min(1,(e.clientX-r.left)/r.width)),y:Math.max(0,Math.min(1,(e.clientY-r.top)/r.height))};}
function dragZone(e:PointerEvent):Zone|null {if(!drag)return null;const p=point(e);return {id:'pending',label:'Source lock',x:Math.min(drag.x,p.x),y:Math.min(drag.y,p.y),width:Math.abs(drag.x-p.x),height:Math.abs(drag.y-p.y)};}
stage.addEventListener('pointerdown',e=>{if(!source||busy||!drawing||!e.isPrimary||e.button!==0)return;e.preventDefault();const p=point(e);drag={...p,pointer:e.pointerId};stage.setPointerCapture(e.pointerId);});
stage.addEventListener('pointermove',e=>{if(drag?.pointer!==e.pointerId)return;const z=dragZone(e);if(z)renderZones(z);});
stage.addEventListener('pointerup',e=>{if(drag?.pointer!==e.pointerId||!source)return;const z=dragZone(e);drag=null;if(stage.hasPointerCapture(e.pointerId))stage.releasePointerCapture(e.pointerId);if(z){const b=bounds(z,source.width,source.height);if(b.right-b.x>=2&&b.bottom-b.y>=2)addZone(b.x,b.y,b.right-b.x,b.bottom-b.y,`Source lock ${history.value.length+1}`);}renderZones();});
stage.addEventListener('pointercancel',()=>{drag=null;renderZones();});
for(const key of ['blend','feather','threshold'] as const)el<HTMLInputElement>(key).addEventListener('input',()=>{config={...config,[key]:Number(el<HTMLInputElement>(key).value)/(key==='blend'?100:1)};controls();schedule();});
for(const button of document.querySelectorAll<HTMLButtonElement>('[data-mode]'))button.addEventListener('click',()=>{mode=button.dataset.mode!;present();});
el<HTMLInputElement>('split').addEventListener('input',()=>{split=Number(el<HTMLInputElement>('split').value)/100;present();});el('aligned').addEventListener('change',controls);el('origin').addEventListener('change',controls);
action('export-output',()=>{const r=ready();save(encodePNG(r.output),'looklock-protected.png');status('Protected PNG exported. Exact source pixels were copied inside the selected locks.');});
action('export-source',()=>{if(source&&!busy){save(encodePNG(source),'looklock-original.png');status('Decoded original PNG exported locally. Use this exact frame in your renderer.');}});
action('export-mask',()=>{const r=ready(),data=new Uint8ClampedArray(r.mask.length*4);for(let i=0;i<r.mask.length;i++){const v=255-r.mask[i]!;data.set([v,v,v,255],i*4);}save(encodePNG({width:source!.width,height:source!.height,data}),'looklock-mask.png');status('Mask exported: black = keep original, white = allow treatment. Gray is outer feather. Check mask polarity in other tools.');});
action('export-board',()=>{save(encodePNG(makeBoard()),'looklock-review-board.png');status('Three-panel review board exported. It is a visual summary, not an inference-quality certificate.');});
action('export-recipe',()=>{ready();save(new Blob([JSON.stringify(recipe(identity(),history.value,config),null,2)],{type:'application/json'}),'looklock-recipe.json');status('Recipe saved. It only applies to this exact decoded frame pair; no media was included.');});
action('import-recipe',()=>el<HTMLInputElement>('recipe-file').click());el<HTMLInputElement>('recipe-file').addEventListener('change',()=>{const input=el<HTMLInputElement>('recipe-file'),file=input.files?.[0];input.value='';if(!file)return;const ticket=job;
  void(async()=>{if(file.size>32768)throw new Error('Recipe exceeds 32 KiB.');const text=await file.text();if(ticket!==job)return;const r=parseRecipe(text,identity());config={blend:r.blend,feather:r.feather,threshold:r.threshold};commitZones(r.zones);status('Recipe loaded and matched to both decoded frame hashes. Your alignment confirmation is still required.');})().catch(error=>status(message(error),true));});
action('export-report',async()=>{const r=ready(),ticket=job,settings=recipe(identity(),history.value,config),origin=el<HTMLSelectElement>('origin').value,kind=sourceKind,time=frameTime;
  const hash=await rasterHash(r.output);if(ticket!==job)return;save(new Blob([JSON.stringify({schema:1,product:'shiny-looklock',version:'0.4.0',createdAt:new Date().toISOString(),recipe:settings,outputRGBAHash:hash,metrics:r.metrics,sourceKind:kind,sourceFrameTimeSeconds:time,treatmentOrigin:origin,alignment:'user-confirmed; not automatically verified',modelProvenanceVerified:false,inferencePerformedByLookLock:false,scope:'Independent decoded SDR still frames. No semantic truth, original file bytes, HDR, or temporal quality certification.',pixelIdentity:'SHA-256 of looklock-rgba-v1:width:height: followed by decoded RGBA bytes',mediaIncluded:false,filenamesIncluded:false},null,2)],{type:'application/json'}),'looklock-review.json');status('Review report exported with frame hashes, settings, and measured pixel differences.');});
action('reset',()=>{job++;operation?.abort();operation=null;if(scheduled)cancelAnimationFrame(scheduled);scheduled=0;releaseVideo();if(dialog.open)dialog.close();source=candidate=null;originalCanvas=treatmentCanvas=outputCanvas=heatCanvas=null;sourceHash=candidateHash='';result=null;history=new History<Zone[]>([]);drawing=false;drag=null;config={...DEFAULTS};mode='source';frameTime=null;el('empty').hidden=false;stage.style.aspectRatio='16 / 9';stage.style.maxWidth='none';el('source-title').textContent='Open original';el('source-meta').textContent='Image or a frame from local video';el('candidate-title').textContent='Open treated frame';el('candidate-meta').textContent='Same dimensions, frame, and crop';el('dimensions').textContent='No frame';el<HTMLInputElement>('aligned').checked=false;setBusy(false);listZones();refresh();status('Session cleared. No images or recipes were stored.');});
document.addEventListener('keydown',e=>{if(dialog.open)return;if(e.key==='Escape'){if(busy){cancel();return;}drawing=false;drag=null;renderZones();controls();return;}if(e.ctrlKey||e.metaKey||e.altKey||e.target instanceof HTMLInputElement||e.target instanceof HTMLSelectElement||e.target instanceof HTMLTextAreaElement||e.target instanceof HTMLButtonElement||((e.target as HTMLElement)?.isContentEditable))return;const selected=['source','candidate','output','split','heatmap'][Number(e.key)-1];if(selected&&source&&(candidate||selected==='source')){mode=selected;present();}});
window.addEventListener('pagehide',()=>{job++;operation?.abort();operation=null;releaseVideo();if(scheduled)cancelAnimationFrame(scheduled);});
demo();
