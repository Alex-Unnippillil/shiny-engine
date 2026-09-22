export function fixture() {
  const c = document.createElement('canvas'); c.width = 960; c.height = 540;
  const ctx = c.getContext('2d')!;
  const sky = ctx.createLinearGradient(0, 0, 0, 540); sky.addColorStop(0, '#254453'); sky.addColorStop(1, '#16292c');
  ctx.fillStyle = sky; ctx.fillRect(0, 0, 960, 540);
  ctx.fillStyle = '#9fd8ba'; ctx.beginPath(); ctx.arc(724, 144, 58, 0, 2 * Math.PI); ctx.fill();
  for (let layer = 0; layer < 4; layer++) {
    ctx.fillStyle = ['#315965', '#29454c', '#1b383f', '#102a2e'][layer]!;
    ctx.beginPath(); ctx.moveTo(0, 330 + layer * 25);
    for (let x = 0; x <= 960; x += 30) ctx.lineTo(x, 260 + layer * 47 + Math.sin(x / 71 + layer) * 53 + Math.cos(x / 33) * 12);
    ctx.lineTo(960, 540); ctx.lineTo(0, 540); ctx.fill();
  }
  ctx.globalAlpha = 0.6; ctx.strokeStyle = '#83b3a8'; ctx.lineWidth = 1;
  for (let i = 0; i < 19; i++) { ctx.beginPath(); ctx.moveTo(500 - i * 24, 350 + i * 10); ctx.lineTo(905 - i * 8, 350 + i * 10); ctx.stroke(); }
  ctx.globalAlpha = 1; ctx.fillStyle = '#d5e6dd'; ctx.font = '11px monospace'; ctx.fillText('SYNTHETIC DETAIL STUDY / 001', 38, 43);
  ctx.font = '38px sans-serif'; ctx.fillText('Keep the character.', 38, 445);
  ctx.fillStyle = '#98b9b1'; ctx.font = '15px sans-serif'; ctx.fillText('Refine the detail.', 40, 478);
  // Deterministic generated fixture; not a claimed photographic restoration example.
  let seed = 71; const image = ctx.getImageData(0, 0, c.width, c.height);
  for (let i = 0; i < image.data.length; i += 4) {
    seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
    const noise = ((seed >>> 24) / 255 - 0.5) * 16;
    for (let channel = 0; channel < 3; channel++) image.data[i + channel] = image.data[i + channel]! + noise;
  }
  ctx.putImageData(image, 0, 0); return c;
}
