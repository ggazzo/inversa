// notifications.web.ts — browser Notification API. Used by Vite via
// `.web.ts` extension resolution; ConnectionManager imports
// `./notifications` and gets this on web, the noop stub on RN.

import type { NotificationsApi } from './notifications';

class WebNotifications implements NotificationsApi {
    isSupported(): boolean {
        return typeof Notification !== 'undefined';
    }

    async requestPermission(): Promise<boolean> {
        if (!this.isSupported()) return false;
        const result = await Notification.requestPermission();
        return result === 'granted';
    }

    permission(): 'default' | 'granted' | 'denied' | 'unsupported' {
        if (!this.isSupported()) return 'unsupported';
        return Notification.permission;
    }

    send(title: string, body: string): void {
        if (!this.isSupported() || Notification.permission !== 'granted') return;
        try {
            new Notification(title, {
                body,
                icon:  '/icon-192.png',
                badge: '/icon-192.png',
                tag:   'inversa-notification',
                renotify: true,
            } as any);
        } catch (e) {
            console.warn('Notification failed:', e);
        }
    }
}

export const Notifications: NotificationsApi = new WebNotifications();
