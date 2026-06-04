// updateChannel.web.ts — web stub. The PWA doesn't use EAS Update (users get
// fresh JS via a normal page reload), so the channel picker is suppressed on
// web by reporting the override as unsupported.

export const KNOWN_CHANNELS = ['production', 'dev', 'rc'] as const;

export function isChannelOverrideSupported(): boolean {
  return false;
}

export function currentChannel(): string | null {
  return null;
}

export async function applyChannel(_channel: string): Promise<void> {
  throw new Error('Channel override is not available on web');
}
