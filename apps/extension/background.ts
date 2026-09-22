import { browser, type Port, type Sender } from '../../packages/contracts/browser.js';
import { record, nativeRequest } from '../../packages/contracts/index.js';
const api = browser!;
let nativePort: Port | null = null;
const pending = new Map<string, { resolve: (value: unknown) => void; reject: (error: Error) => void; timer: ReturnType<typeof setTimeout> }>();
function trusted(sender: Sender) {
  if (sender.id !== api.runtime.id || !sender.url) return false;
  try {
    const url = new URL(sender.url);
    return url.protocol === 'chrome-extension:' && url.hostname === api.runtime.id &&
      ['/apps/extension/popup.html', '/apps/viewer/index.html', '/index.html'].includes(url.pathname);
  } catch { return false; }
}
function connect() {
  if (nativePort) return nativePort;
  nativePort = api.runtime.connectNative('com.shinyengine.companion');
  nativePort.onMessage.addListener(message => {
    if (!record(message) || message.v !== 1 || typeof message.id !== 'string') return;
    const waiter = pending.get(message.id); if (!waiter) return;
    clearTimeout(waiter.timer); pending.delete(message.id); waiter.resolve(message);
  });
  nativePort.onDisconnect.addListener(() => {
    const reason = api.runtime.lastError?.message ?? 'Native companion disconnected.';
    nativePort = null;
    for (const waiter of pending.values()) { clearTimeout(waiter.timer); waiter.reject(new Error(reason)); }
    pending.clear();
  });
  return nativePort;
}
async function handle(message: unknown, sender: Sender) {
  if (!trusted(sender) || !record(message) || typeof message.type !== 'string') throw new Error('Untrusted extension request.');
  if (message.type === 'native') {
    const payload = nativeRequest(message.payload);
    if (!await api.permissions.contains({ permissions: ['nativeMessaging'] })) throw new Error('Native messaging permission is required.');
    if (pending.has(payload.id)) throw new Error('Duplicate request ID.');
    if (pending.size >= 8) throw new Error('Too many outstanding companion requests.');
    const port = connect();
    const result = await new Promise<unknown>((resolve, reject) => {
      const timer = setTimeout(() => { pending.delete(payload.id); reject(new Error('Companion timed out. Check the source picker or native window.')); }, 120_000);
      pending.set(payload.id, { resolve, reject, timer });
      try { port.postMessage(payload); } catch (error) { clearTimeout(timer); pending.delete(payload.id); reject(error); }
    });
    return { ok: true, result };
  }
  if (message.type === 'capture-open') {
    if (!sender.url?.endsWith('/apps/extension/popup.html')) throw new Error('Capture must start in the toolbar popup.');
    if (!await api.permissions.contains({ permissions: ['tabCapture'] })) throw new Error('Tab capture permission is required.');
    const [source] = await api.tabs.query({ active: true, currentWindow: true });
    if (!source?.id || !source.url || !/^https?:\/\//.test(source.url)) throw new Error('Select a normal website tab first.');
    const viewer = await api.tabs.create({ url: 'about:blank', active: false });
    if (!viewer.id) throw new Error('Could not open the separate viewer.');
    await api.storage.session.set({ [`capture:${viewer.id}`]: { source: source.id, created: Date.now() } });
    await api.tabs.update(viewer.id, { url: api.runtime.getURL('apps/viewer/index.html?capture=1'), active: true });
    return { ok: true };
  }
  if (message.type === 'capture-token') {
    if (!sender.tab?.id || !sender.url?.includes('/apps/viewer/index.html?capture=1')) throw new Error('Capture requires the separate authorized viewer.');
    const key = `capture:${sender.tab.id}`;
    const result = (await api.storage.session.get(key))[key];
    await api.storage.session.remove(key);
    if (!record(result) || typeof result.source !== 'number' || typeof result.created !== 'number' || Date.now() - result.created > 30_000) {
      throw new Error('Capture authorization expired. Start again from the toolbar.');
    }
    const token = await api.tabCapture.getMediaStreamId({ targetTabId: result.source, consumerTabId: sender.tab.id });
    return { ok: true, token };
  }
  throw new Error('Unknown extension action.');
}
api.runtime.onMessage.addListener((message, sender, reply) => {
  if (!trusted(sender)) return false;
  void handle(message, sender).then(reply).catch(error => reply({ ok: false, error: error instanceof Error ? error.message : 'Action failed.' }));
  return true;
});
