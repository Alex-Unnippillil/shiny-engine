import { geometry, settings, type Settings } from '../contracts/index.js';
export type Source = HTMLVideoElement | HTMLImageElement | HTMLCanvasElement | ImageBitmap;
export interface Renderer {
  readonly label: string;
  render(source: Source, config: Settings): Promise<number>;
  release(): void;
}
export function dimensions(source: Source) {
  return source instanceof HTMLVideoElement ? [source.videoWidth, source.videoHeight] as const :
    source instanceof HTMLImageElement ? [source.naturalWidth, source.naturalHeight] as const :
      [source.width, source.height] as const;
}
const vertexWGSL = `
struct Vertex { @builtin(position) position: vec4f, @location(0) uv: vec2f };
@vertex fn vs(@builtin(vertex_index) i: u32) -> Vertex {
  var xy = array<vec2f,3>(vec2f(-1,-1), vec2f(3,-1), vec2f(-1,3));
  var o: Vertex; o.position = vec4f(xy[i],0,1); o.uv = vec2f((xy[i].x+1)*0.5,(1-xy[i].y)*0.5); return o;
}`;
function fragmentWGSL(video: boolean) {
  const texture = video ? 'texture_external' : 'texture_2d<f32>';
  const read = video ? 'textureSampleBaseClampToEdge(tex,samp,uv)' : 'textureSampleLevel(tex,samp,uv,0.0)';
  return `
struct Params { texel: vec2f, strength: f32, denoise: f32, split: f32, enabled: f32, padding: vec2f };
@group(0) @binding(0) var tex: ${texture};
@group(0) @binding(1) var samp: sampler;
@group(0) @binding(2) var<uniform> p: Params;
fn read(uv: vec2f) -> vec4f { return ${read}; }
@fragment fn fs(v: Vertex) -> @location(0) vec4f {
  let c = read(v.uv);
  let a = read(v.uv + vec2f(p.texel.x,0)); let b = read(v.uv - vec2f(p.texel.x,0));
  let d = read(v.uv + vec2f(0,p.texel.y)); let e = read(v.uv - vec2f(0,p.texel.y));
  let blur = (a.rgb+b.rgb+d.rgb+e.rgb)*0.25;
  let edge = length(c.rgb-blur);
  let denoised = mix(c.rgb,blur,p.denoise*(1.0-smoothstep(0.02,0.16,edge)));
  let lo = min(c.rgb,min(min(a.rgb,b.rgb),min(d.rgb,e.rgb)));
  let hi = max(c.rgb,max(max(a.rgb,b.rgb),max(d.rgb,e.rgb)));
  let enhanced = clamp(denoised+(c.rgb-blur)*p.strength,lo,hi);
  let original = p.enabled < 0.5 || v.uv.x < p.split;
  return vec4f(select(enhanced,c.rgb,original)*c.a,c.a);
}`;
}
class WebGPU implements Renderer {
  readonly label = 'Spatial · WebGPU';
  private image: GPUTextureHandle | null = null;
  private imageSize = '';
  private released = false;
  private lost = '';
  private sampler: object;
  private uniform: GPUBufferHandle;
  private config: string;
  private constructor(private canvas: HTMLCanvasElement, private device: GPUDeviceHandle,
    private context: GPUCanvasHandle, private pipelines: [GPUPipelineHandle, GPUPipelineHandle], format: string) {
    this.config = format;
    this.sampler = device.createSampler({ magFilter: 'linear', minFilter: 'linear', addressModeU: 'clamp-to-edge', addressModeV: 'clamp-to-edge' });
    this.uniform = device.createBuffer({ size: 32, usage: 64 | 8 }); // UNIFORM | COPY_DST
    void device.lost.then(info => { if (!this.released) this.lost = info.message || 'GPU device lost.'; });
  }
  static async create(canvas: HTMLCanvasElement) {
    const api = navigator.gpu;
    if (!api) throw new Error('WebGPU is unavailable.');
    const adapter = await api.requestAdapter();
    if (!adapter) throw new Error('No WebGPU adapter.');
    const device = await adapter.requestDevice();
    try {
      const format = api.getPreferredCanvasFormat();
      const pipelines = [] as GPUPipelineHandle[];
      for (const video of [false, true]) {
        const module = device.createShaderModule({ code: vertexWGSL + fragmentWGSL(video) });
        const info = await module.getCompilationInfo();
        if (info.messages.some(m => m.type === 'error')) throw new Error('GPU shader compilation failed.');
        pipelines.push(await device.createRenderPipelineAsync({ layout: 'auto',
          vertex: { module, entryPoint: 'vs' }, fragment: { module, entryPoint: 'fs', targets: [{ format }] },
          primitive: { topology: 'triangle-list' } }));
      }
      const context = canvas.getContext('webgpu') as unknown as GPUCanvasHandle | null;
      if (!context) throw new Error('WebGPU canvas unavailable.');
      context.configure({ device, format, alphaMode: 'premultiplied' });
      return new WebGPU(canvas, device, context, pipelines as [GPUPipelineHandle, GPUPipelineHandle], format);
    } catch (error) { device.destroy(); throw error; }
  }
  async render(source: Source, input: Settings) {
    if (this.released || this.lost) throw new Error(this.lost || 'Renderer has been released.');
    const start = performance.now();
    const s = settings(input);
    const [width, height] = dimensions(source);
    const size = geometry(width, height, s.scale, Math.min(4096, this.device.limits.maxTextureDimension2D));
    if (width > this.device.limits.maxTextureDimension2D || height > this.device.limits.maxTextureDimension2D) {
      throw new Error('Source exceeds this GPU’s texture limit. Use a smaller image.');
    }
    if (this.canvas.width !== size.width || this.canvas.height !== size.height) {
      this.canvas.width = size.width; this.canvas.height = size.height;
      this.context.configure({ device: this.device, format: this.config, alphaMode: 'premultiplied' });
    }
    const video = source instanceof HTMLVideoElement;
    const pipeline = this.pipelines[video ? 1 : 0];
    this.device.pushErrorScope('validation');
    try {
      let resource: object;
      if (video) resource = this.device.importExternalTexture({ source, colorSpace: 'srgb' });
      else {
        const key = `${width}x${height}`;
        if (this.imageSize !== key) {
          this.image?.destroy();
          this.image = this.device.createTexture({ size: [width, height], format: 'rgba8unorm', usage: 2 | 4 | 16 });
          this.imageSize = key;
        }
        this.device.queue.copyExternalImageToTexture({ source }, { texture: this.image }, [width, height]);
        resource = this.image!.createView();
      }
      this.device.queue.writeBuffer(this.uniform, 0, new Float32Array([1 / width, 1 / height, s.strength, s.denoise, s.split, +s.enabled, 0, 0]));
      const group = this.device.createBindGroup({ layout: pipeline.getBindGroupLayout(0), entries: [
        { binding: 0, resource }, { binding: 1, resource: this.sampler }, { binding: 2, resource: { buffer: this.uniform } },
      ] });
      const encoder = this.device.createCommandEncoder();
      const pass = encoder.beginRenderPass({ colorAttachments: [{ view: this.context.getCurrentTexture().createView(),
        clearValue: [0, 0, 0, 0], loadOp: 'clear', storeOp: 'store' }] });
      pass.setPipeline(pipeline); pass.setBindGroup(0, group); pass.draw(3); pass.end();
      this.device.queue.submit([encoder.finish()]);
      await this.device.queue.onSubmittedWorkDone();
    } finally {
      const error = await this.device.popErrorScope();
      if (error) throw new Error(`GPU validation failed: ${error.message}`);
    }
    return performance.now() - start; // CPU submission-to-completion, not a GPU timestamp.
  }
  release() {
    if (this.released) return;
    this.released = true; this.image?.destroy(); this.uniform.destroy();
    this.context.unconfigure(); this.device.destroy();
  }
}
const vsGL = `#version 300 es
out vec2 uv;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);uv=vec2(p.x,1.0-p.y);gl_Position=vec4(p*2.0-1.0,0,1);}`;
const fsGL = `#version 300 es
precision highp float;
in vec2 uv;out vec4 outputColor;
uniform sampler2D tex;uniform vec2 texel;uniform float strength,denoise,split,enabled;
void main(){vec4 c=texture(tex,uv);vec3 a=texture(tex,uv+vec2(texel.x,0)).rgb;
vec3 b=texture(tex,uv-vec2(texel.x,0)).rgb;vec3 d=texture(tex,uv+vec2(0,texel.y)).rgb;vec3 e=texture(tex,uv-vec2(0,texel.y)).rgb;
vec3 blur=(a+b+d+e)*0.25;float edge=length(c.rgb-blur);
vec3 clean=mix(c.rgb,blur,denoise*(1.0-smoothstep(0.02,0.16,edge)));
vec3 lo=min(c.rgb,min(min(a,b),min(d,e)));vec3 hi=max(c.rgb,max(max(a,b),max(d,e)));
vec3 result=clamp(clean+(c.rgb-blur)*strength,lo,hi);
outputColor=vec4(enabled<0.5||uv.x<split?c.rgb:result,c.a);}`;
class WebGL implements Renderer {
  readonly label = 'Spatial · WebGL2';
  private gl: WebGL2RenderingContext;
  private program: WebGLProgram;
  private texture: WebGLTexture;
  private released = false;
  constructor(private canvas: HTMLCanvasElement) {
    const gl = canvas.getContext('webgl2', { alpha: true, premultipliedAlpha: false, preserveDrawingBuffer: true, antialias: false });
    if (!gl) throw new Error('No WebGPU or WebGL2 backend. The original remains available.');
    this.gl = gl;
    const shaders: WebGLShader[] = [];
    try {
      for (const [kind, code] of [[gl.VERTEX_SHADER, vsGL], [gl.FRAGMENT_SHADER, fsGL]] as const) {
        const shader = gl.createShader(kind);
        if (!shader) throw new Error('Shader allocation failed.');
        shaders.push(shader); gl.shaderSource(shader, code); gl.compileShader(shader);
        if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(shader) || 'Shader failed.');
      }
      const program = gl.createProgram();
      if (!program) throw new Error('Program allocation failed.');
      this.program = program;
      shaders.forEach(shader => gl.attachShader(program, shader)); gl.linkProgram(program);
      if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw new Error('GPU program link failed.');
      const texture = gl.createTexture();
      if (!texture) throw new Error('Texture allocation failed.');
      this.texture = texture; gl.bindTexture(gl.TEXTURE_2D, texture);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    } finally { shaders.forEach(shader => gl.deleteShader(shader)); }
  }
  async render(source: Source, input: Settings) {
    const gl = this.gl;
    if (this.released || gl.isContextLost()) throw new Error('GPU context lost. Reopen the media to retry.');
    const start = performance.now(); const s = settings(input);
    const [width, height] = dimensions(source);
    const limit = gl.getParameter(gl.MAX_TEXTURE_SIZE) as number;
    if (width > limit || height > limit) throw new Error('Source exceeds this GPU’s texture limit.');
    const size = geometry(width, height, s.scale, Math.min(4096, limit));
    if (this.canvas.width !== size.width || this.canvas.height !== size.height) {
      this.canvas.width = size.width; this.canvas.height = size.height;
    }
    gl.viewport(0, 0, size.width, size.height); gl.useProgram(this.program);
    gl.activeTexture(gl.TEXTURE0); gl.bindTexture(gl.TEXTURE_2D, this.texture);
    gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, false);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, source);
    gl.uniform1i(gl.getUniformLocation(this.program, 'tex'), 0);
    gl.uniform2f(gl.getUniformLocation(this.program, 'texel'), 1 / width, 1 / height);
    for (const key of ['strength', 'denoise', 'split', 'enabled'] as const) {
      gl.uniform1f(gl.getUniformLocation(this.program, key), Number(s[key]));
    }
    gl.drawArrays(gl.TRIANGLES, 0, 3);
    const fence = gl.fenceSync(gl.SYNC_GPU_COMMANDS_COMPLETE, 0); gl.flush();
    if (!fence) throw new Error('GPU synchronization unavailable.');
    try {
      await new Promise<void>((resolve, reject) => {
        const check = () => {
          if (this.released || gl.isContextLost()) return reject(new Error('GPU session ended.'));
          const status = gl.clientWaitSync(fence, 0, 0);
          if (status === gl.WAIT_FAILED || performance.now() - start > 5000) return reject(new Error('GPU response timed out.'));
          if (status === gl.TIMEOUT_EXPIRED) setTimeout(check, 0); else resolve();
        }; check();
      });
    } finally { gl.deleteSync(fence); }
    if (gl.getError() !== gl.NO_ERROR) throw new Error('GPU rejected this media frame.');
    return performance.now() - start;
  }
  release() {
    if (this.released) return; this.released = true;
    this.gl.deleteTexture(this.texture); this.gl.deleteProgram(this.program);
    this.gl.getExtension('WEBGL_lose_context')?.loseContext();
  }
}
/** Separate candidates: a canvas already bound to WebGPU cannot become a WebGL canvas. */
export async function createRenderer(mount: HTMLElement): Promise<{ renderer: Renderer; canvas: HTMLCanvasElement }> {
  let canvas = document.createElement('canvas');
  canvas.setAttribute('aria-label', 'Media comparison: original on the left, spatial enhancement on the right');
  let renderer: Renderer;
  try { renderer = await WebGPU.create(canvas); }
  catch { canvas = canvas.cloneNode() as HTMLCanvasElement; renderer = new WebGL(canvas); }
  mount.replaceChildren(canvas);
  return { renderer, canvas };
}
