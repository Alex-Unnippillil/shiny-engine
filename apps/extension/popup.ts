import { browser, request } from '../../packages/contracts/browser.js';
import { record } from '../../packages/contracts/index.js';
const api = browser!;
function action(id: string, callback: () => Promise<void>) {
  document.getElementById(id)!.addEventListener('click', () => {
    const buttons = document.querySelectorAll<HTMLButtonElement>('button'); buttons.forEach(button => { button.disabled = true; });
    void callback().catch(error => { const status = document.getElementById('status')!; status.textContent = error.message; status.classList.add('error'); })
      .finally(() => buttons.forEach(button => { button.disabled = false; }));
  });
}
async function current() {
  const [tab] = await api.tabs.query({ active: true, currentWindow: true });
  if (!tab?.id || !tab.url || !/^https?:\/\//.test(tab.url)) throw new Error('Choose a normal website tab first. For local files, use Media Lab.');
  return tab.id;
}
function report(value: unknown) {
  if (!record(value)) throw new Error('The page did not respond.');
  if (value.ok !== true) throw new Error(String(value.error ?? 'Inline mode is unavailable. Use separate tab capture.'));
  const status = document.getElementById('status')!; status.textContent = String(value.message ?? 'Done.'); status.classList.remove('error');
}
action('lab', async () => { await api.tabs.create({ url: api.runtime.getURL('apps/viewer/index.html') }); });
action('inline', async () => {
  const id = await current(); await api.scripting.executeScript({ target: { tabId: id }, files: ['apps/extension/content.js'] });
  report(await api.tabs.sendMessage(id, { type: 'shiny-inline', command: 'start' }));
});
action('stop', async () => {
  const id = await current();
  try { report(await api.tabs.sendMessage(id, { type: 'shiny-inline', command: 'stop' })); }
  catch { report({ ok: true, message: 'No active inline session on this page.' }); }
});
action('capture', async () => {
  if (!await api.permissions.request({ permissions: ['tabCapture'] })) throw new Error('Tab capture permission was not granted.');
  await request({ type: 'capture-open' });
});
