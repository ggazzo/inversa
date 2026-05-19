// kvStore.ts — minimal async key/value persistence. RN bundle picks
// this file (uses AsyncStorage); Vite resolves kvStore.web.ts
// instead (uses localStorage).
//
// AsyncStorage is a native module. If the dev-client was built before
// the dependency was added, `require()` returns an object whose
// methods throw "AsyncStorage is null" — we detect that defensively
// and degrade to in-memory storage so the rest of the app keeps
// working until `expo run:ios` relinks the pods.

let AsyncStorage: any;
try {
    AsyncStorage = require('@react-native-async-storage/async-storage').default;
    // Confirm the native side actually loaded; on a stale dev-client
    // the import succeeds but the module's prototype methods are
    // shimmed with throwers.
    if (!AsyncStorage || typeof AsyncStorage.getItem !== 'function') {
        AsyncStorage = null;
    }
} catch {
    AsyncStorage = null;
}

// In-memory fallback so the app still boots when AsyncStorage isn't
// linked. Values are lost on reload — auto-reconnect just won't fire
// until the user rebuilds the dev-client.
const memory = new Map<string, string>();

export const kvStore = {
    async get(key: string): Promise<string | null> {
        if (!AsyncStorage) return memory.has(key) ? memory.get(key)! : null;
        try { return await AsyncStorage.getItem(key); }
        catch { return null; }
    },
    async set(key: string, value: string): Promise<void> {
        if (!AsyncStorage) { memory.set(key, value); return; }
        try { await AsyncStorage.setItem(key, value); } catch { /* noop */ }
    },
    async remove(key: string): Promise<void> {
        if (!AsyncStorage) { memory.delete(key); return; }
        try { await AsyncStorage.removeItem(key); } catch { /* noop */ }
    },
};
