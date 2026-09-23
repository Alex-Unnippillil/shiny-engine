import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {join, resolve, dirname} from 'node:path';
import {safePath, approvals, parseManifest, inspectModel, verifyModel, revalidateModel, processingSize,
  sha256, UPSTREAM, assertApproved} from '../../dist/compiled/packages/nr/policy.js';
import {controls, NeuralEngine} from '../../dist/compiled/packages/nr/engine.js';
import {NeuralClient} from '../../dist/compiled/packages/nr/client.js';
import {APPROVALS} from '../../dist/compiled/packages/nr/approvals.js';
// Synthetic validator data only. This is NOT a functioning NR model or an inference approval.
const bytes = Uint8Array.of(0,1,2,3);
const stageHash = await sha256(bytes.buffer);
function manifest() { return {totals:{blockCount:71}, stages:[{id:0,file:'stages/test.bin',packedByteLength:4,sha256:stageHash}],
  tensors:[{name:'block70.layer0.blend_scale',block:70,layer:0,stage:0,stageOffset:0,byteLength:2}]}; }
const approval = digest => ({manifestSha256:digest,upstream:UPSTREAM,reviewId:'SYNTHETIC TEST ONLY',reviewedBy:'test fixture',
  runtimeUse:'approved',parityEvidence:'validator fixture, no inference claim',scope:'browser-independent-frames'});
function file(path, data) {const f=new File([data],path.split('/').at(-1));Object.defineProperty(f,'webkitRelativePath',{value:'fixture/'+path});return f;}
async function fixture(data=bytes) {return [file('manifest.json',JSON.stringify(manifest())),file('model/stages/test.bin',data)];}
const signal=()=>new AbortController().signal;
test('NR build ships no model approval or asset and engine refuses before GPU use',async()=>{
 assert.deepEqual(APPROVALS,[]);assert.throws(()=>assertApproved('0'.repeat(64),APPROVALS),/no repository-reviewed/);
 const inspection=await inspectModel(await fixture(),[],signal());assert.equal(inspection.approval,null);
 await assert.rejects(verifyModel(inspection,[],signal(),()=>{}),/no repository-reviewed/);
 await assert.rejects(NeuralEngine.create(null,{manifestBytes:await inspection.files.get('manifest.json').arrayBuffer(),manifestSha256:inspection.manifestSha256,stages:new Map()},320,180,()=>{}),/no repository-reviewed/);
});
for(const path of ['../a.bin','a/../../b.bin','/abs.bin','a\\b.bin','https://a/b.bin','a/%2e/b','a//b','a/./b','',null,'x'.repeat(241)])
 test(`NR rejects unsafe relative path ${JSON.stringify(path)}`,()=>assert.throws(()=>safePath(path)));
test('NR accepts only reviewed register schema and immutable upstream identity',()=>{
 const value=approval('0'.repeat(64));assert.equal(approvals([value])[0].scope,'browser-independent-frames');
 for(const update of [{upstream:'main'},{runtimeUse:'unchecked'},{parityEvidence:''},{scope:'native'},{manifestSha256:'x'}]) assert.throws(()=>approvals([{...value,...update}]));
 assert.throws(()=>approvals([value,value]));
});
for(const [name,change] of Object.entries({
 blocks:m=>m.totals.blockCount=70, duplicate:m=>m.stages.push({...m.stages[0]}),
 hash:m=>m.stages[0].sha256='bad', bytes:m=>m.stages[0].packedByteLength=0,
 traversal:m=>m.stages[0].file='../test.bin', dll:m=>m.stages[0].file='stage.dll',
 bounds:m=>m.tensors[0].byteLength=5, offset:m=>m.tensors[0].stageOffset=-1,
 tensorName:m=>m.tensors[0].name='block1.layer0.blend_scale', missing:m=>m.tensors=[],
 oversized:m=>m.stages[0].packedByteLength=257*1024*1024,
})) test(`NR manifest rejects ${name}`,()=>{const m=manifest();change(m);assert.throws(()=>parseManifest(m));});
test('NR local manifest and every stage are hash-bound; worker reparses manifest',async()=>{
 const inspected=await inspectModel(await fixture(),[],signal());const trusted=[approval(inspected.manifestSha256)];
 const verified=await verifyModel(inspected,trusted,signal(),()=>{});
 assert.deepEqual(verified.stages.get(0),bytes);
 verified.manifest={totals:{blockCount:1}};
 const restored=await revalidateModel(verified,trusted);assert.equal(restored.manifest.totals.blockCount,71);
 verified.stages.get(0)[0]=255;await assert.rejects(revalidateModel(verified,trusted),/stage fingerprint/);
});
test('NR stage corruption and caller-forged approval cannot authorize loading',async()=>{
 const inspected=await inspectModel(await fixture(Uint8Array.of(7,7,7,7)),[],signal());
 inspected.approval=approval(inspected.manifestSha256);
 await assert.rejects(verifyModel(inspected,[],signal(),()=>{}),/no repository-reviewed/);
 await assert.rejects(verifyModel(inspected,[inspected.approval],signal(),()=>{}),/SHA-256/);
});
test('NR intake rejects missing stages, duplicate paths, invalid roots and cancellation',async()=>{
 const files=await fixture();
 await assert.rejects(inspectModel([files[0]],[],signal()),/Missing or wrong-sized/);
 await assert.rejects(inspectModel([...files,files[0]],[],signal()),/Duplicate/);
 await assert.rejects(inspectModel([new File(['{}'],'manifest.json')],[],signal()),/folder/);
 const c=new AbortController();c.abort();await assert.rejects(inspectModel(files,[],c.signal),/abort/i);
 const inspected=await inspectModel(files,[],signal());await assert.rejects(verifyModel(inspected,[approval(inspected.manifestSha256)],c.signal,()=>{}),/abort/i);
});
test('NR preview sizes preserve aspect, never upscale, and reject tiny/extreme inputs',()=>{
 assert.deepEqual(processingSize(1920,1080,320),{width:320,height:180});
 assert.deepEqual(processingSize(1080,1920,512),{width:288,height:512});
 assert.deepEqual(processingSize(96,64,320),{width:96,height:64});
 for(const args of [[0,1,320],[NaN,1,512],[8200,10,320],[320,20,320],[100,100,4096]])assert.throws(()=>processingSize(...args));
});
test('NR controls reject partial, unknown, non-finite or out-of-range settings',()=>{
 assert.deepEqual(controls({tone:.5,structure:.5,split:.5}),{tone:.5,structure:.5,split:.5});
 for(const value of [null,{},[],{tone:NaN,structure:0,split:0},{tone:2,structure:0,split:0},{tone:0,structure:0,split:0,path:'/tmp'}])assert.throws(()=>controls(value));
});
class FakeWorker {
 static instances=[];terminated=false;onmessage=null;onerror=null;
 constructor(){FakeWorker.instances.push(this);}postMessage(value){this.last=value;}terminate(){this.terminated=true;}
 emit(data){this.onmessage?.({data});}
}
test('NR worker requests are serial, correlated and cancelled on termination',async()=>{
 const original=globalThis.Worker;globalThis.Worker=FakeWorker;
 try {const progress=[];const client=new NeuralClient(x=>progress.push(x));const worker=FakeWorker.instances.at(-1);
  const first=client.call('probe');await assert.rejects(client.call('frame'),/busy/);
  worker.emit({id:worker.last.id,progress:'checking'});assert.deepEqual(progress,['checking']);
  worker.emit({id:999,ok:true,result:'stale'});worker.emit({id:worker.last.id,ok:true,result:'current'});assert.equal(await first,'current');
  const second=client.call('prepare');client.stop();await assert.rejects(second,/stopped/);assert(worker.terminated);
  await assert.rejects(client.call('probe'),/stopped/);
 }finally{globalThis.Worker=original;}
});
test('NR watchdog terminates an unresponsive worker instead of retaining a pending frame',async()=>{
 const original=globalThis.Worker;globalThis.Worker=FakeWorker;
 try {const client=new NeuralClient();await assert.rejects(client.call('frame',{},[],5),/timed out/);assert(FakeWorker.instances.at(-1).terminated);}finally{globalThis.Worker=original;}
});
test('NR bundled graph has a closed relative-module dependency graph and retained license',async()=>{
 const root=resolve('dist/extension/vendor/opendlss'),seen=new Set();
 async function visit(path){if(seen.has(path))return;seen.add(path);const code=await readFile(path,'utf8');
  for(const m of code.matchAll(/(?:from\s*|import\s*)['"]([^'"]+)['"]/g)){
   assert(m[1].startsWith('.'),`non-local module ${m[1]}`);const target=resolve(dirname(path),m[1]);assert(target.startsWith(root+'/'));await visit(target);
  }
 }
 await visit(join(root,'api.js'));assert(seen.size>20);assert.match(await readFile(join(root,'LICENSE'),'utf8'),/MIT License/);
 const upstream=JSON.parse(await readFile(join(root,'upstream.json'),'utf8'));assert.equal(upstream.commit,UPSTREAM);assert.equal(upstream.weightsDistributed,false);
});
