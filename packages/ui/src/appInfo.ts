// appInfo.ts — native build pulls EAS Update metadata via expo-updates.
// The web bundle re-routes to `appInfo.web.ts` (Vite resolves
// `.web.ts` first), which returns a constant null so the WizardAbout
// "App" section is suppressed on the browser PWA — Web Bluetooth
// users get their JS through a normal page reload, not EAS Update.

import * as Updates from 'expo-updates';

export interface AppBundleInfo {
    updateId: string | null;
    createdAt: string | null;
    runtimeVersion: string | null;
    channel: string | null;
    isEmbeddedLaunch: boolean;
    isEnabled: boolean;
}

export function getAppBundleInfo(): AppBundleInfo | null {
    return {
        updateId: Updates.updateId ?? null,
        // `createdAt` is a Date on production builds and `null` while
        // running off the embedded bundle for the very first time
        // (before any OTA has applied). Normalize to ISO so the row
        // renderer doesn't have to special-case.
        createdAt: Updates.createdAt
            ? new Date(Updates.createdAt).toISOString()
            : null,
        runtimeVersion: Updates.runtimeVersion ?? null,
        channel: Updates.channel ?? null,
        isEmbeddedLaunch: Updates.isEmbeddedLaunch ?? false,
        isEnabled: Updates.isEnabled,
    };
}
