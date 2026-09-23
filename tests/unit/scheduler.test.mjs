import test from 'node:test';
import assert from 'node:assert/strict';
import { FrameLoop } from '../../dist/compiled/packages/gpu-web/scheduler.js';

/** Deterministic callback clock: no sleeps, GPU, or browser timing assumptions. */
function harness() {
  const saved = Object.fromEntries(['document','performance','requestAnimationFrame','cancelAnimationFrame'].map(k=>[k,Object.getOwnPropertyDescriptor(globalThis,k)]));
  let now=0,id=0;const callbacks=new Map();const doc=new EventTarget();doc.hidden=false;
  const request=(fn)=>{callbacks.set(++id,fn);return id;};
  const video=new EventTarget();Object.assign(video,{paused:true,ended:false,readyState:2,requestVideoFrameCallback:request,cancelVideoFrameCallback:(h)=>callbacks.delete(h)});
  for(const [key,value] of Object.entries({document:doc,performance:{now:()=>now},requestAnimationFrame:request,cancelAnimationFrame:(h)=>callbacks.delete(h)}))Object.defineProperty(globalThis,key,{configurable:true,value});
  const flush=()=>new Promise(resolve=>setImmediate(resolve));
  return {video,doc,callbacks,flush,
    async frame(ms=40){now+=ms;const entry=callbacks.entries().next().value;if(entry){callbacks.delete(entry[0]);entry[1]();}await flush();},
    advance(ms){now+=ms;},
    restore(){for(const [key,descriptor] of Object.entries(saved)){if(descriptor)Object.defineProperty(globalThis,key,descriptor);else delete globalThis[key];}}
  };
}
function deferred(){let resolve,reject;const promise=new Promise((a,b)=>{resolve=a;reject=b;});return {promise,resolve,reject};}

test('paused frame loop renders its initial refresh once, then has no queued callbacks',async()=>{
  const h=harness();let calls=0;const loop=new FrameLoop(h.video,async()=>{calls++;return 10;},assert.fail);
  try{loop.start();await h.frame();assert.equal(calls,1);assert.equal(h.callbacks.size,0);h.advance(60000);await h.frame();assert.equal(calls,1);}finally{loop.stop();h.restore();}
});
test('pause drains a slow in-flight frame and exactly one final refresh, not a continuous loop',async()=>{
  const h=harness(),pending=deferred();h.video.paused=false;let calls=0,samples=0;
  const loop=new FrameLoop(h.video,()=>{calls++;return calls===1?pending.promise:Promise.resolve(8);},assert.fail,()=>samples++);
  try{loop.start();await h.frame();h.video.paused=true;h.video.dispatchEvent(new Event('pause'));h.advance(2000);assert.equal(calls,1);assert.equal(h.callbacks.size,0);
    pending.resolve(2040);await h.flush();assert.equal(h.callbacks.size,1);await h.frame();assert.equal(calls,2);assert.equal(samples,2);assert.equal(h.callbacks.size,0);
    h.advance(60000);await h.frame();assert.equal(calls,2);
  }finally{loop.stop();h.restore();}
});
test('resuming a paused frame loop schedules new processing and stop removes queued work',async()=>{
  const h=harness();let calls=0;const loop=new FrameLoop(h.video,async()=>{calls++;return 8;},assert.fail);
  try{loop.start();await h.frame();h.video.paused=false;h.video.dispatchEvent(new Event('play'));await h.frame();assert.equal(calls,2);assert.equal(h.callbacks.size,1);loop.stop();assert.equal(h.callbacks.size,0);h.video.dispatchEvent(new Event('play'));await h.frame();assert.equal(calls,2);}finally{loop.stop();h.restore();}
});
test('stopping during an in-flight render discards completion and does not reschedule',async()=>{
  const h=harness(),pending=deferred();let samples=0;const loop=new FrameLoop(h.video,()=>pending.promise,assert.fail,()=>samples++);
  try{loop.start();await h.frame();loop.stop();pending.resolve(4000);await h.flush();assert.equal(samples,0);assert.equal(h.callbacks.size,0);}finally{loop.stop();h.restore();}
});
test('hidden and undecoded paused videos wait for lifecycle events without spinning',async()=>{
  const h=harness();h.video.readyState=0;let calls=0;const loop=new FrameLoop(h.video,async()=>{calls++;return 8;},assert.fail);
  try{loop.start();await h.frame();assert.equal(calls,0);assert.equal(h.callbacks.size,0);h.doc.hidden=true;h.video.readyState=2;h.video.dispatchEvent(new Event('loadeddata'));assert.equal(h.callbacks.size,0);h.doc.hidden=false;h.doc.dispatchEvent(new Event('visibilitychange'));await h.frame();assert.equal(calls,1);assert.equal(h.callbacks.size,0);}finally{loop.stop();h.restore();}
});
test('a render failure reports once, stops scheduling, and detaches media listeners',async()=>{
  const h=harness();let failures=0;const loop=new FrameLoop(h.video,async()=>{throw new Error('test render error');},()=>failures++);
  try{loop.start();await h.frame();assert.equal(failures,1);assert.equal(h.callbacks.size,0);h.video.dispatchEvent(new Event('seeked'));await h.frame();assert.equal(failures,1);assert.equal(h.callbacks.size,0);}finally{loop.stop();h.restore();}
});
