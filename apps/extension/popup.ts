import { browser, request } from '../../packages/contracts/browser.js';
import { record } from '../../packages/contracts/index.js';
const api = browser!;
let eligible = false, busy = false;
const tabButtons = ['inline', 'stop', 'capture'];
function status(text: string, error = false) {
  const output = document.getElementById('status')!; output.textContent = text; output.classList.toggle('error', error);
}
function controls() {
  for (const id of tabButtons) (document.getElementById(id) as HTMLButtonElement).disabled = busy || !eligible;
  for (const id of ['lab', 'neural']) (document.getElementById(id) as HTMLButtonElement).disabled = busy;
}
function action(id: string, progress: string, callback: () => Promise<void>) {
  document.getElementById(id)!.addEventListener('click', () => {
    if (busy) return; busy = true; controls(); status(progress);
    // Call synchronously so optional permission requests retain user activation.
    void callback().catch(error => status(error instanceof Error ? error.message : 'Action unavailable. Try Media Lab.', true))
      .finally(() => { busy = false; controls(); });
  });
}
async function current() {
  const [tab] = await api.tabs.query({ active: true, currentWindow: true });
  if (!tab?.id || !tab.url || !/^https?:\/\//.test(tab.url)) throw new Error('Choose a normal website tab first. Local files open in Media Lab.');
  return tab.id;
}
function report(value: unknown) {
  if (!record(value)) throw new Error('The page did not respond. Try opening it in the separate viewer.');
  if (value.ok !== true) throw new Error(String(value.error ?? 'Inline mode is unavailable. Use separate tab capture.'));
  status(String(value.message ?? 'Done.'));
}
action('lab', 'Opening your local workbench…', async () => { await api.tabs.create({ url: api.runtime.getURL('apps/viewer/index.html') }); status('Media Lab opened. Choose a local file to begin.'); });
action('neural', 'Opening the isolated neural workspace…', async () => { await api.tabs.create({ url: api.runtime.getURL('apps/nr/index.html') }); status('Neural Lab opened. The model approval gate is enforced before inference.'); });
action('inline', 'Checking the player and preparing local graphics…', async () => {
  const id = await current(); await api.scripting.executeScript({ target: { tabId: id }, files: ['apps/extension/content.js'] });
  report(await api.tabs.sendMessage(id, { type: 'shiny-inline', command: 'start' }));
});
action('stop', 'Restoring the original player…', async () => {
  const id = await current();
  try { report(await api.tabs.sendMessage(id, { type: 'shiny-inline', command: 'stop' })); }
  catch { report({ ok: true, message: 'No active inline session on this page.' }); }
});
action('capture', 'Requesting capture in a separate viewer…', async () => {
  if (!await api.permissions.request({ permissions: ['tabCapture'] })) throw new Error('Permission was not granted. Nothing was captured.');
  await request({ type: 'capture-open' }); status('Separate viewer opened. Stop session there ends capture.');
});
void api.tabs.query({ active: true, currentWindow: true }).then(([tab]) => {
  eligible = !!tab?.url && /^https?:\/\//.test(tab.url);
  document.getElementById('current-tab')!.textContent = eligible ? new URL(tab!.url!).hostname : 'Use a website tab';
  if (!eligible && !busy) status('Open Media Lab for local files. Page enhancement requires a normal website tab.');
  controls();
}).catch(() => { controls(); status('This tab is unavailable. Media Lab still works for local files.'); });
