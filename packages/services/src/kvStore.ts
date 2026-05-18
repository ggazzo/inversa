// kvStore.ts — minimal async key/value persistence. RN bundle picks
// this file (uses AsyncStorage); Vite resolves kvStore.web.ts
// instead (uses localStorage).

import AsyncStorage from '@react-native-async-storage/async-storage';

export const kvStore = {
    async get(key: string): Promise<string | null> {
        try { return await AsyncStorage.getItem(key); }
        catch { return null; }
    },
    async set(key: string, value: string): Promise<void> {
        try { await AsyncStorage.setItem(key, value); } catch { /* noop */ }
    },
    async remove(key: string): Promise<void> {
        try { await AsyncStorage.removeItem(key); } catch { /* noop */ }
    },
};
