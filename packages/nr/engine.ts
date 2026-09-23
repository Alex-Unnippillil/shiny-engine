import { APPROVALS } from './approvals.js';
import { revalidateModel, type VerifiedModel } from './policy.js';

/** Minimal, structural WebGPU interface for the worker-only compute path. */
export interface Device extends Omit<GPUDeviceHandle, 'limits' | 'createCommandEncoder'> {
  limits: { maxTextureDimension2D: number; maxBufferSize: number; maxStorageBufferBindingSize: number;
    maxComputeWorkgroupStorageSize: number; maxComputeInvocationsPerWorkgroup: number;
    maxComputeWorkgroupSizeX: number; maxStorageBuffersPerShaderStage: number; };
  createComputePipelineAsync(description: object): Promise<GPUPipelineHandle>;
  createCommandEncoder(): Encoder;
  addEventListener(type: string, callback: (event: { error: { message: string } }) => void): void;
}
interface ReadBuffer extends GPUBufferHandle { mapAsync(mode: number): Promise<void>; getMappedRange(): ArrayBuffer; unmap(): void; }
interface Encoder extends GPUEncoderHandle {
  copyBufferToBuffer(source: GPUBufferHandle, sourceOffset: number, target: GPUBufferHandle, targetOffset: number, size: number): void;
  beginComputePass(): { setPipeline(pipeline: GPUPipelineHandle): void; setBindGroup(index: number, group: object): void;
    dispatchWorkgroups(x: number, y: number): void; end(): void; };
}
interface Adapter { limits: Device['limits']; requestDevice(options: object): Promise<Device>; }
interface GPU { requestAdapter(options?: object): Promise<Adapter | null>; getPreferredCanvasFormat(): string; }
export interface Geometry { validWidth: number; validHeight: number; fullWidth: number; fullHeight: number; fullRows: number; }
interface Network {
  features: { buffer: GPUBufferHandle }; graph: { head: { buffer: GPUBufferHandle } };
  geometry: Geometry; recorder: { encode(encoder: Encoder): void; dispatchCount: number };
  tensors: { total: number }; destroy(): void;
}
interface Model { load(input: VerifiedModel): Promise<Model>; bytesUploaded: number; destroy(): void; }
interface Reference { Network: { create(options: object): Promise<Network> }; Model: new (device: Device) => Model;
  SHADERS: Record<string, string>; geometryFromValid(w: number, h: number): Geometry; }
const REFERENCE_PATH = '../../vendor/opendlss/api.js';
export const reference = async (): Promise<Reference> => import(REFERENCE_PATH);
export function gpu(): GPU | undefined { return navigator.gpu as unknown as GPU | undefined; }
export async function probe() {
  if (!gpu()) return { available: false, reason: 'WebGPU is unavailable in this context.' };
  const adapter = await gpu()!.requestAdapter({ powerPreference: 'high-performance' });
  if (!adapter) return { available: false, reason: 'No WebGPU adapter is available.' };
  const l = adapter.limits;
  const available = l.maxComputeWorkgroupStorageSize >= 32768 && l.maxStorageBuffersPerShaderStage >= 8 && l.maxBufferSize >= 64 * 1024 ** 2;
  return { available, reason: available ? 'Compute limits meet the initial probe. Model allocation and shaders still require validation.' : 'The reference requires 32 KiB workgroup storage, 8 storage bindings and larger buffers.',
    workgroupKiB: l.maxComputeWorkgroupStorageSize / 1024, bindingMiB: l.maxStorageBufferBindingSize / 1024 ** 2 };
}
export interface Controls { tone: number; structure: number; split: number; }
export function controls(input: unknown): Controls {
  const value = input as Controls;
  if (!value || Object.keys(value).some(k => !['tone', 'structure', 'split'].includes(k)) ||
    ![value.tone, value.structure, value.split].every(x => Number.isFinite(x) && x >= 0 && x <= 1)) throw new Error('Invalid neural controls.');
  return { tone: value.tone, structure: value.structure, split: value.split };
}
const VERTEX = `
struct Vertex { @builtin(position) position: vec4f, @location(0) uv: vec2f };
@vertex fn vs(@builtin(vertex_index) i:u32)->Vertex {
 var xy=array<vec2f,3>(vec2f(-1,-1),vec2f(3,-1),vec2f(-1,3));
 var o:Vertex; o.position=vec4f(xy[i],0,1); o.uv=vec2f((xy[i].x+1)*.5,(1-xy[i].y)*.5); return o;
}`;
const SOURCE = VERTEX + `
@group(0) @binding(0) var source: texture_2d<f32>;
@group(0) @binding(1) var samp: sampler;
@fragment fn fs(v:Vertex)->@location(0) vec4f {return textureSampleLevel(source,samp,v.uv,0.0);}`;
/** SDR proxy composition, no HDR transform, artistic style or fabricated motion/history. */
const COMPOSE = VERTEX + `
struct Output { width:u32,height:u32,pitch:u32,split:f32 };
@group(0) @binding(0) var image:texture_2d<f32>;
@group(0) @binding(1) var<storage,read> head:array<f32>;
@group(0) @binding(2) var<uniform> out:Output;
@fragment fn fs(v:Vertex)->@location(0) vec4f {
 let xy=min(vec2u(v.uv*vec2f(f32(out.width),f32(out.height))),vec2u(out.width-1u,out.height-1u));
 let c=textureLoad(image,vec2i(xy),0); let i=(xy.y*out.pitch+xy.x)*4u;
 let residual=vec3f(head[i],head[i+1u],head[i+2u]);
 let finite=all((bitcast<vec3u>(residual)&vec3u(0x7f800000u))!=vec3u(0x7f800000u));
 let value=clamp(c.rgb+residual*.25,vec3f(0),vec3f(1));
 let enhanced=vec3f(f16_to_f32(truncate_half(value.r)),f16_to_f32(truncate_half(value.g)),f16_to_f32(truncate_half(value.b)));
 let chosen=select(c.rgb,enhanced,finite && v.uv.x>=out.split);
 return vec4f(chosen*c.a,c.a);
}`;
const CHECK_HEAD = `
struct Output { width:u32,height:u32,pitch:u32,split:f32 };
@group(0) @binding(0) var<storage,read> bits:array<u32>;
@group(0) @binding(1) var<storage,read_write> invalid:atomic<u32>;
@group(0) @binding(2) var<uniform> params:Output;
@compute @workgroup_size(8,8) fn check(@builtin(global_invocation_id) p:vec3u) {
 if(p.x>=params.width || p.y>=params.height){return;}
 let i=(p.y*params.pitch+p.x)*4u;
 for(var c=0u;c<4u;c++){if((bits[i+c]&0x7f800000u)==0x7f800000u){atomicStore(&invalid,1u);}}
}`;
async function shader(device: Device, code: string) {
  const module = device.createShaderModule({ code });
  const issue = (await module.getCompilationInfo()).messages.find(x => x.type === 'error');
  if (issue) throw new Error(issue.message); return module;
}
/** Reuses textures/buffers and performs no per-frame JavaScript pixel-array readback. */
export class FramePipeline {
  private context: GPUCanvasHandle;
  private snapshot: GPUTextureHandle;
  private upload: GPUTextureHandle | null = null;
  private uploadSize = '';
  private uniform: GPUBufferHandle;
  private outputUniform: GPUBufferHandle;
  private sampler: object;
  private preprocessGroup: object;
  private outputGroup: object;
  private checkGroup: object;
  private invalid: GPUBufferHandle;
  private readback: ReadBuffer;
  private lost = '';
  private constructor(private canvas: HTMLCanvasElement | OffscreenCanvas, private device: Device, private geometry: Geometry,
    private snapshotPipeline: GPUPipelineHandle, private preprocess: GPUPipelineHandle, private output: GPUPipelineHandle, private check: GPUPipelineHandle,
    features: GPUBufferHandle, head: GPUBufferHandle) {
    this.canvas.width = geometry.validWidth; this.canvas.height = geometry.validHeight;
    this.context = (canvas as unknown as { getContext(type: string): GPUCanvasHandle }).getContext('webgpu');
    if (!this.context) throw new Error('WebGPU canvas context is unavailable.');
    this.context.configure({ device, format: gpu()!.getPreferredCanvasFormat(), alphaMode: 'premultiplied' });
    this.snapshot = device.createTexture({ size: [canvas.width, canvas.height], format: 'rgba8unorm', usage: 16 | 4 });
    this.uniform = device.createBuffer({ size: 48, usage: 64 | 8 }); this.outputUniform = device.createBuffer({ size: 16, usage: 64 | 8 });
    this.sampler = device.createSampler({ magFilter: 'linear', minFilter: 'linear', addressModeU: 'clamp-to-edge', addressModeV: 'clamp-to-edge' });
    this.preprocessGroup = device.createBindGroup({ layout: preprocess.getBindGroupLayout(0), entries: [
      { binding: 0, resource: { buffer: this.uniform } }, { binding: 1, resource: this.snapshot.createView() }, { binding: 5, resource: { buffer: features } }] });
    this.outputGroup = device.createBindGroup({ layout: output.getBindGroupLayout(0), entries: [
      { binding: 0, resource: this.snapshot.createView() }, { binding: 1, resource: { buffer: head } }, { binding: 2, resource: { buffer: this.outputUniform } }] });
    this.invalid = device.createBuffer({ size: 4, usage: 128 | 8 | 4 });
    this.readback = device.createBuffer({ size: 4, usage: 1 | 8 }) as ReadBuffer;
    this.checkGroup = device.createBindGroup({ layout: check.getBindGroupLayout(0), entries: [
      { binding: 0, resource: { buffer: head } }, { binding: 1, resource: { buffer: this.invalid } }, { binding: 2, resource: { buffer: this.outputUniform } }] });
    device.addEventListener('uncapturederror', event => { this.lost = event.error.message; });
    void device.lost.then(info => { this.lost = info.message || 'GPU device lost.'; });
  }
  static async create(canvas: HTMLCanvasElement | OffscreenCanvas, device: Device, geometry: Geometry,
    features: GPUBufferHandle, head: GPUBufferHandle, sources: Record<string, string>) {
    const numerics = sources['shaders/numerics.wgsl']!;
    let preprocess = sources['shaders/preprocess.wgsl']!;
    preprocess = preprocess.replace('var<storage, read> proxy : array<f32>;', 'var proxy : texture_2d<f32>;')
      .replace('select(2u * pre.valid_width - id.x - 2u, id.x, id.x < pre.valid_width)', 'mirror_pixel(id.x, pre.valid_width)')
      .replace('select(2u * pre.valid_height - id.y - 2u, id.y, id.y < pre.valid_height)', 'mirror_pixel(id.y, pre.valid_height)')
      .replace('let texel = (image_y * pre.source_width + image_x) * 4u;', 'let texel = textureLoad(proxy, vec2i(i32(image_x), i32(image_y)), 0);')
      .replaceAll('proxy[texel + 1u]', 'texel.g').replaceAll('proxy[texel + 2u]', 'texel.b').replaceAll('proxy[texel]', 'texel.r');
    if (preprocess.includes('proxy[') || !preprocess.includes('var proxy : texture_2d<f32>')) throw new Error('Pinned preprocessing interface changed.');
    const mirror = `fn mirror_pixel(x:u32,size:u32)->u32 { let period=2u*(size-1u);let r=x%period;return min(r,period-r); }`;
    const truncate = sources['shaders/frame.wgsl']!.match(/fn truncate_half\(value: f32\) -> u32 \{[\s\S]*?\n\}/)?.[0];
    if (!truncate) throw new Error('Pinned composition helper missing.');
    const sourceModule = await shader(device, SOURCE);
    const snapshot = await device.createRenderPipelineAsync({ layout: 'auto', vertex: { module: sourceModule, entryPoint: 'vs' },
      fragment: { module: sourceModule, entryPoint: 'fs', targets: [{ format: 'rgba8unorm' }] } });
    const preModule = await shader(device, numerics + '\n' + mirror + '\n' + preprocess);
    const pre = await device.createComputePipelineAsync({ layout: 'auto', compute: { module: preModule, entryPoint: 'preprocess' } });
    const module = await shader(device, numerics + '\n' + truncate + '\n' + COMPOSE);
    const output = await device.createRenderPipelineAsync({ layout: 'auto', vertex: { module, entryPoint: 'vs' }, fragment: { module, entryPoint: 'fs', targets: [{ format: gpu()!.getPreferredCanvasFormat() }] } });
    const checkModule = await shader(device, CHECK_HEAD);
    const check = await device.createComputePipelineAsync({ layout: 'auto', compute: { module: checkModule, entryPoint: 'check' } });
    return new FramePipeline(canvas, device, geometry, snapshot, pre, output, check, features, head);
  }
  private configuration(settings: Controls) {
    const s = controls(settings), g = this.geometry;
    const bytes = new ArrayBuffer(48), words = new Uint32Array(bytes), floats = new Float32Array(bytes);
    words.set([g.fullWidth, g.fullHeight, g.validWidth, g.validHeight, g.validWidth, g.validHeight, 0]);
    floats[7] = 1; floats[8] = s.tone; floats[9] = s.structure; floats[10] = -1; floats[11] = 0;
    this.device.queue.writeBuffer(this.uniform, 0, words);
    const out = new ArrayBuffer(16); new Uint32Array(out).set([g.validWidth, g.validHeight, g.fullWidth]); new Float32Array(out)[3] = s.split;
    this.device.queue.writeBuffer(this.outputUniform, 0, new Uint32Array(out));
  }
  private presentTo(encoder: Encoder) {
    const pass = encoder.beginRenderPass({ colorAttachments: [{ view: this.context.getCurrentTexture().createView(), loadOp: 'clear', storeOp: 'store', clearValue: { r: 0, g: 0, b: 0, a: 0 } }] });
    pass.setPipeline(this.output); pass.setBindGroup(0, this.outputGroup); pass.draw(3); pass.end();
  }
  async process(frame: VideoFrame | ImageBitmap, settings: Controls, encode: (encoder: Encoder) => void): Promise<number> {
    if (this.lost) throw new Error(this.lost);
    const start = performance.now(), device = this.device;
    this.configuration(settings); device.queue.writeBuffer(this.invalid, 0, new Uint32Array(1)); device.pushErrorScope('validation');
    try {
      // Explicit color/alpha conversion is intentional: direct video external textures
      // may carry premultiplied RGB. The neural proxy requires straight SDR RGB.
      // This reuses a GPU texture; no full-frame JavaScript pixel arrays are involved.
      const isVideo = typeof VideoFrame !== 'undefined' && frame instanceof VideoFrame;
      const width = isVideo ? frame.displayWidth : (frame as ImageBitmap).width;
      const height = isVideo ? frame.displayHeight : (frame as ImageBitmap).height;
      if (!Number.isInteger(width) || !Number.isInteger(height) || width < 1 || height < 1 ||
        width > device.limits.maxTextureDimension2D || height > device.limits.maxTextureDimension2D)
        throw new Error('Frame dimensions are invalid or exceed the GPU texture limit.');
      const key = `${width}x${height}`;
      if (key !== this.uploadSize) {
        this.upload?.destroy(); this.upload = device.createTexture({ size: [width, height], format: 'rgba8unorm', usage: 2 | 4 | 16 });
        this.uploadSize = key;
      }
      device.queue.copyExternalImageToTexture({ source: frame },
        { texture: this.upload, premultipliedAlpha: false, colorSpace: 'srgb' }, [width, height]);
      const resource = this.upload!.createView(), pipeline = this.snapshotPipeline;
      const group = device.createBindGroup({ layout: pipeline.getBindGroupLayout(0), entries: [{ binding: 0, resource }, { binding: 1, resource: this.sampler }] });
      const encoder = device.createCommandEncoder();
      const pass = encoder.beginRenderPass({ colorAttachments: [{ view: this.snapshot.createView(), loadOp: 'clear', storeOp: 'store', clearValue: { r: 0, g: 0, b: 0, a: 0 } }] });
      pass.setPipeline(pipeline); pass.setBindGroup(0, group); pass.draw(3); pass.end();
      const pre = encoder.beginComputePass(); pre.setPipeline(this.preprocess); pre.setBindGroup(0, this.preprocessGroup);
      pre.dispatchWorkgroups(Math.ceil(this.geometry.fullWidth / 8), Math.ceil(this.geometry.fullHeight / 8)); pre.end();
      encode(encoder);
      const check = encoder.beginComputePass(); check.setPipeline(this.check); check.setBindGroup(0, this.checkGroup);
      check.dispatchWorkgroups(Math.ceil(this.geometry.validWidth / 8), Math.ceil(this.geometry.validHeight / 8)); check.end();
      encoder.copyBufferToBuffer(this.invalid, 0, this.readback, 0, 4); this.presentTo(encoder);
      device.queue.submit([encoder.finish()]); await device.queue.onSubmittedWorkDone();
    } finally {
      const issue = await device.popErrorScope(); if (issue) throw new Error(issue.message);
    }
    await this.readback.mapAsync(1);
    const invalid = new Uint32Array(this.readback.getMappedRange())[0]; this.readback.unmap();
    if (invalid) throw new Error('Neural output contains non-finite values. Preview stopped.');
    if (this.lost) throw new Error(this.lost); return performance.now() - start;
  }
  async present(settings: Controls) {
    if (this.lost) throw new Error(this.lost); this.configuration(settings);
    this.device.pushErrorScope('validation');
    try { const encoder = this.device.createCommandEncoder(); this.presentTo(encoder); this.device.queue.submit([encoder.finish()]); await this.device.queue.onSubmittedWorkDone(); }
    finally { const issue = await this.device.popErrorScope(); if (issue) throw new Error(issue.message); }
  }
  /** Snapshot the freshly submitted canvas in the same task, before automatic texture expiry. */
  async exportPNG(): Promise<Blob> {
    if (this.lost) throw new Error(this.lost);
    this.device.pushErrorScope('validation');
    try {
      const encoder = this.device.createCommandEncoder(); this.presentTo(encoder);
      this.device.queue.submit([encoder.finish()]);
      // Do not await GPU completion before asking the canvas to copy its bitmap.
      const canvas = this.canvas;
      const image = 'convertToBlob' in canvas
        ? canvas.convertToBlob({ type: 'image/png' })
        : new Promise<Blob>((resolve, reject) => canvas.toBlob(
          blob => blob ? resolve(blob) : reject(new Error('PNG snapshot unavailable.')), 'image/png'));
      const [blob] = await Promise.all([image, this.device.queue.onSubmittedWorkDone()]);
      if (this.lost) throw new Error(this.lost);
      return blob;
    } finally {
      const issue = await this.device.popErrorScope(); if (issue) throw new Error(issue.message);
    }
  }
  destroy() { this.upload?.destroy(); this.snapshot.destroy(); this.uniform.destroy(); this.outputUniform.destroy(); this.invalid.destroy(); this.readback.destroy(); this.context.unconfigure(); }
}
export class NeuralEngine {
  private constructor(private device: Device, private model: Model, private network: Network, private frame: FramePipeline) {}
  static async create(canvas: OffscreenCanvas, verified: VerifiedModel, width: number, height: number, progress: (text: string) => void) {
    verified = await revalidateModel(verified, APPROVALS);
    if (![width, height].every(x => Number.isInteger(x) && x >= 33 && x <= 512)) throw new Error('Invalid NR processing geometry.');
    const ref = await reference(); progress('Checking worker GPU capabilities…');
    const adapter = await gpu()?.requestAdapter({ powerPreference: 'high-performance' });
    if (!adapter || adapter.limits.maxComputeWorkgroupStorageSize < 32768 || adapter.limits.maxStorageBuffersPerShaderStage < 8) throw new Error('Worker GPU does not meet the reference requirements.');
    const l = adapter.limits;
    const device = await adapter.requestDevice({ requiredLimits: {
      maxBufferSize: Math.min(l.maxBufferSize, 512 * 1024 ** 2), maxStorageBufferBindingSize: Math.min(l.maxStorageBufferBindingSize, 512 * 1024 ** 2),
      maxComputeWorkgroupStorageSize: l.maxComputeWorkgroupStorageSize, maxComputeInvocationsPerWorkgroup: l.maxComputeInvocationsPerWorkgroup,
      maxComputeWorkgroupSizeX: l.maxComputeWorkgroupSizeX, maxStorageBuffersPerShaderStage: l.maxStorageBuffersPerShaderStage,
    } });
    let totalAllocated = 0;
    const limited = new Proxy(device, { get(target, key) {
      if (key === 'createBuffer') return (descriptor: { size: number; usage: number }) => {
        const size = descriptor.size;
        if (!Number.isSafeInteger(size) || size < 1 || size > target.limits.maxBufferSize ||
          ((descriptor.usage & 128) !== 0 && size > target.limits.maxStorageBufferBindingSize) ||
          (totalAllocated += size) > 768 * 1024 ** 2) throw new Error('Neural allocation exceeds the configured GPU budget.');
        return target.createBuffer(descriptor);
      };
      const value = Reflect.get(target, key, target); return typeof value === 'function' ? value.bind(target) : value;
    } });
    try {
      progress('Loading verified local weights into the GPU…'); const model = await new ref.Model(limited).load(verified);
      const network = await ref.Network.create({ device: limited, model, width, height, onProgress: progress });
      const frame = await FramePipeline.create(canvas, device, network.geometry, network.features.buffer, network.graph.head.buffer, ref.SHADERS);
      return new NeuralEngine(device, model, network, frame);
    } catch (error) { device.destroy(); throw error; }
  }
  process(frame: VideoFrame | ImageBitmap, settings: Controls) { return this.frame.process(frame, settings, encoder => this.network.recorder.encode(encoder)); }
  present(settings: Controls) { return this.frame.present(settings); }
  exportPNG() { return this.frame.exportPNG(); }
  get info() { return { dispatches: this.network.recorder.dispatchCount, activationMiB: this.network.tensors.total / 1024 ** 2,
    weightsMiB: this.model.bytesUploaded / 1024 ** 2, temporalMode: 'independent-frames', parity: 'not-reverified-on-this-device' }; }
  destroy() { this.frame.destroy(); this.network.destroy(); this.model.destroy(); this.device.destroy(); }
}
