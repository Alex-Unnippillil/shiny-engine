// Minimal structural API surface used by this project; no third-party runtime types.
interface GPUTextureHandle { createView(): object; destroy(): void; }
interface GPUBufferHandle { destroy(): void; }
interface GPUPipelineHandle { getBindGroupLayout(index: number): object; }
interface GPUShaderHandle { getCompilationInfo(): Promise<{ messages: { type: string; message: string }[] }>; }
interface GPUEncoderHandle {
  beginRenderPass(descriptor: object): { setPipeline(p: GPUPipelineHandle): void;
    setBindGroup(index: number, group: object): void; draw(count: number): void; end(): void; };
  finish(): object;
}
interface GPUDeviceHandle {
  limits: { maxTextureDimension2D: number };
  lost: Promise<{ message: string }>;
  queue: { writeBuffer(buffer: GPUBufferHandle, offset: number, data: ArrayBufferView): void;
    copyExternalImageToTexture(source: object, destination: object, size: number[]): void;
    submit(commands: object[]): void; onSubmittedWorkDone(): Promise<void>; };
  createShaderModule(descriptor: object): GPUShaderHandle;
  createRenderPipelineAsync(descriptor: object): Promise<GPUPipelineHandle>;
  createBuffer(descriptor: object): GPUBufferHandle;
  createSampler(descriptor: object): object;
  createTexture(descriptor: object): GPUTextureHandle;
  importExternalTexture(descriptor: object): object;
  createBindGroup(descriptor: object): object;
  createCommandEncoder(): GPUEncoderHandle;
  pushErrorScope(kind: string): void; popErrorScope(): Promise<{ message: string } | null>;
  destroy(): void;
}
interface GPUCanvasHandle { configure(descriptor: object): void; unconfigure(): void; getCurrentTexture(): GPUTextureHandle; }
interface Navigator {
  readonly gpu?: { requestAdapter(options?: object): Promise<{ requestDevice(): Promise<GPUDeviceHandle> } | null>;
    getPreferredCanvasFormat(): string; };
}
