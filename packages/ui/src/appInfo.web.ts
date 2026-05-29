// appInfo.web.ts — the web PWA doesn't use EAS Update; users get a
// fresh JS bundle through normal browser cache invalidation, so the
// metadata block in WizardAbout is hidden on web by returning null.

export interface AppBundleInfo {
    updateId: string | null;
    createdAt: string | null;
    runtimeVersion: string | null;
    channel: string | null;
    isEmbeddedLaunch: boolean;
    isEnabled: boolean;
}

export function getAppBundleInfo(): AppBundleInfo | null {
    return null;
}
