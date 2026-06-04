// updateChannel.ts — native runtime EAS Update channel override.
//
// Lets the user point the installed APK at a different channel at runtime
// (dev/rc/production or a custom one) and pull that channel's latest JS
// bundle, without reinstalling. Backed by expo-updates'
// setUpdateURLAndRequestHeadersOverride, which Expo only permits when
// `updates.disableAntiBrickingMeasures` is set (see app.config.js).
//
// Bound by the runtime fingerprint: switching channels only fetches an update
// whose native fingerprint matches this build. dev/rc/production built from
// the same source share a fingerprint, so cross-channel works; a channel whose
// latest update was built from different native code yields "no update".
//
// The web bundle re-routes to updateChannel.web.ts (no-op) — the PWA gets JS
// through a normal reload, not EAS Update.
import * as Updates from 'expo-updates';

// Keep in sync with apps/native/app.json updates.url (we avoid an
// expo-constants dependency just to read it back).
const UPDATE_URL = 'https://u.expo.dev/0754be71-5229-4862-8d43-538f33ffe4fd';

export const KNOWN_CHANNELS = ['production', 'dev', 'rc'] as const;

// True only where the override is actually usable: a release build with
// updates enabled and the (SDK 51+) override API present.
export function isChannelOverrideSupported(): boolean {
  return (
    Updates.isEnabled &&
    typeof (Updates as any).setUpdateURLAndRequestHeadersOverride === 'function'
  );
}

export function currentChannel(): string | null {
  return Updates.channel ?? null;
}

// Point at `channel`, check for a matching update, and reload into it.
// Throws with a user-facing message on no-match / fetch failure so the caller
// can surface it; the override itself persists across reloads.
export async function applyChannel(channel: string): Promise<void> {
  const name = channel.trim();
  if (!name) throw new Error('Canal vazio');

  await Updates.setUpdateURLAndRequestHeadersOverride({
    updateUrl: UPDATE_URL,
    requestHeaders: { 'expo-channel-name': name },
  });

  const res = await Updates.checkForUpdateAsync();
  if (!res.isAvailable) {
    throw new Error(`Sem update no canal "${name}" (fingerprint incompatível?)`);
  }
  await Updates.fetchUpdateAsync();
  await Updates.reloadAsync();
}
