// kvStore.web.ts — localStorage-backed KV for the browser.
// Mirrors the AsyncStorage surface in kvStore.ts.

export const kvStore = {
    async get(key: string): Promise<string | null> {
        try { return typeof localStorage !== 'undefined' ? localStorage.getItem(key) : null; }
        catch { return null; }
    },
    async set(key: string, value: string): Promise<void> {
        try { if (typeof localStorage !== 'undefined') localStorage.setItem(key, value); }
        catch { /* noop */ }
    },
    async remove(key: string): Promise<void> {
        try { if (typeof localStorage !== 'undefined') localStorage.removeItem(key); }
        catch { /* noop */ }
    },
};
