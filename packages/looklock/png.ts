import { validateRaster, type Raster } from './core.js';
/** Lossless RGBA PNG: stored DEFLATE blocks avoid canvas premultiplication on export.
 * No external codec, network, executable data, or hidden metadata. */
const table = new Uint32Array(256);
for (let i=0; i<256; i++) { let c=i; for(let k=0;k<8;k++) c=(c&1)?0xedb88320^(c>>>1):c>>>1; table[i]=c>>>0; }
function chunk(type: string, data: Uint8Array) {
  const out = new Uint8Array(data.length+12), v=new DataView(out.buffer); v.setUint32(0,data.length);
  out.set(new TextEncoder().encode(type),4); out.set(data,8); let crc=0xffffffff;
  for(let i=4;i<out.length-4;i++) crc=table[(crc^out[i]!)&255]!^(crc>>>8);
  v.setUint32(out.length-4,(crc^0xffffffff)>>>0); return out;
}
export function encodePNG(frame: Raster): Blob {
  validateRaster(frame); const {width,height,data}=frame;
  const rows=new Uint8Array((width*4+1)*height);
  for(let y=0;y<height;y++) rows.set(data.subarray(y*width*4,(y+1)*width*4),y*(width*4+1)+1);
  const blocks=Math.ceil(rows.length/65535), z=new Uint8Array(2+rows.length+blocks*5+4);
  z[0]=0x78;z[1]=0x01;let p=2,a=1,b=0;
  for(let at=0;at<rows.length;at+=65535) {
    const n=Math.min(65535,rows.length-at); z[p++]=at+n===rows.length?1:0;
    z[p++]=n&255;z[p++]=n>>>8;z[p++]=(~n)&255;z[p++]=((~n)>>>8)&255;
    z.set(rows.subarray(at,at+n),p);p+=n;
    for(let i=at;i<at+n;i++) { a=(a+rows[i]!)%65521;b=(b+a)%65521; }
  }
  new DataView(z.buffer).setUint32(p,((b<<16)|a)>>>0);
  const header=new Uint8Array(13),hv=new DataView(header.buffer);hv.setUint32(0,width);hv.setUint32(4,height);header[8]=8;header[9]=6;
  return new Blob([new Uint8Array([137,80,78,71,13,10,26,10]),chunk('IHDR',header),chunk('IDAT',z),chunk('IEND',new Uint8Array())],{type:'image/png'});
}
