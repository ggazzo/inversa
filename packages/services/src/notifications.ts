// notifications.ts — platform-neutral fallback for the OS-level
// notification surface. Real RN notifications need `expo-notifications`
// and a permissions/channel dance that's out of scope for the Phase E
// audit pass; this file just keeps ConnectionManager from blowing up
// when it calls `Notifications.send(...)` on the wrong platform.
//
// Web replaces this via `notifications.web.ts` (resolved by Vite's
// `.web.ts` extension priority). Metro picks this fallback.

export interface NotificationsApi {
    isSupported(): boolean;
    requestPermission(): Promise<boolean>;
    permission(): 'default' | 'granted' | 'denied' | 'unsupported';
    send(title: string, body: string): void;
}

class NoopNotifications implements NotificationsApi {
    isSupported(): boolean { return false; }
    async requestPermission(): Promise<boolean> { return false; }
    permission(): 'default' | 'granted' | 'denied' | 'unsupported' { return 'unsupported'; }
    send(_title: string, _body: string): void { /* noop on native until expo-notifications lands */ }
}

export const Notifications: NotificationsApi = new NoopNotifications();
