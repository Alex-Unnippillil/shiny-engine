// Narrow browser API types keep extension privileges visible at call sites.
export interface Tab { id?: number; url?: string; }
export interface Sender { id?: string; url?: string; tab?: Tab; frameId?: number; }
export interface Port {
  postMessage(message: unknown): void;
  disconnect(): void;
  onMessage: { addListener(fn: (message: unknown) => void): void };
  onDisconnect: { addListener(fn: () => void): void };
}
export interface BrowserAPI {
  runtime: {
    id: string; lastError?: { message?: string };
    getURL(path: string): string;
    sendMessage(message: unknown): Promise<unknown>;
    connectNative(name: string): Port;
    onMessage: { addListener(fn: (message: unknown, sender: Sender, reply: (result: unknown) => void) => boolean | void): void };
  };
  tabs: {
    query(query: object): Promise<Tab[]>;
    create(options: object): Promise<Tab>;
    update(id: number, options: object): Promise<Tab>;
    sendMessage(id: number, message: unknown): Promise<unknown>;
  };
  scripting: { executeScript(options: object): Promise<unknown[]> };
  storage: { local: StorageArea; session: StorageArea };
  permissions: { request(permissions: object): Promise<boolean>; contains(permissions: object): Promise<boolean> };
  tabCapture: { getMediaStreamId(options: { targetTabId: number; consumerTabId: number }): Promise<string> };
}
export interface StorageArea {
  get(keys: string | string[]): Promise<Record<string, unknown>>;
  set(data: Record<string, unknown>): Promise<void>;
  remove(keys: string | string[]): Promise<void>;
}
export const browser = (globalThis as unknown as { chrome?: BrowserAPI }).chrome;
export async function request(message: unknown): Promise<Record<string, unknown>> {
  if (!browser?.runtime?.id) throw new Error('This action requires the installed extension.');
  const response = await browser.runtime.sendMessage(message);
  if (!response || typeof response !== 'object') throw new Error('No response from the extension.');
  const result = response as Record<string, unknown>;
  if (result.ok !== true) throw new Error(typeof result.error === 'string' ? result.error : 'The action was not completed.');
  return result;
}
