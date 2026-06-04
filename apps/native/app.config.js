// app.config.js — dynamic Expo config layered on top of app.json.
//
// The only thing computed at config time is the EAS Update channel the build
// listens to. The APK is built via `expo prebuild` + gradle (not `eas build`),
// so the channel isn't taken from eas.json — it must be baked into the native
// manifest here. EAS matches a client to an update by channel + runtime
// fingerprint; without a channel the APK never matches and always runs the
// embedded bundle.
//
// CI sets EXPO_CHANNEL per trigger (see eas-update.yml / android-apk.yml):
//   develop push        → dev
//   tag vX.Y.Z-rc.N      → rc
//   tag vX.Y.Z (stable)  → production
//   PR #N (future)       → pr-<N>
// Local builds default to production.
//
// disableAntiBrickingMeasures unlocks the runtime channel override
// (Updates.setUpdateURLAndRequestHeadersOverride) used by the in-app channel
// picker — Expo gates that API behind this flag. It removes the anti-brick
// safeguard, so the picker is exposed only behind the "advanced" toggle.
module.exports = ({ config }) => {
  const channel = process.env.EXPO_CHANNEL || 'production';
  return {
    ...config,
    updates: {
      ...config.updates,
      disableAntiBrickingMeasures: true,
      requestHeaders: {
        ...(config.updates && config.updates.requestHeaders),
        'expo-channel-name': channel,
      },
    },
  };
};
