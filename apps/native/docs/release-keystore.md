# Android release keystore (optional)

The `Build Android APK` workflow ships a working APK out of the box by
falling back to the standard Android debug keystore. That's fine for
sideloading and personal testing, but Google Play / large-scale
distribution requires a stable release keystore that you control. This
note explains how to provision one once and forget about it.

## Why bother

Without a release keystore:

- Every CI build is signed with the runner's auto-generated debug key.
  Users can install, but each release effectively looks like a new app
  to Android — uninstall/reinstall workflows lose app data on every
  upgrade.
- Play Store / Amazon Appstore reject debug-signed APKs outright.

With a release keystore:

- Same signing identity across every release, so OTA-style sideload
  updates keep the user's data.
- Ready for store publication when you decide to.

The keystore is **forever**: if you lose it you can never publish an
update under the same package id without forcing every user to
uninstall first. Back it up alongside the OTA signing key.

## Generate once

```bash
keytool -genkeypair \
  -v \
  -keystore brewpilot-release.keystore \
  -alias brewpilot \
  -keyalg RSA -keysize 2048 -validity 36500 \
  -storepass "<keystore password>" \
  -keypass   "<key password>"
```

Pick distinct passwords (one for the keystore container, one for the
key itself). Store both in a password manager — they cannot be reset.

## Upload to GitHub Secrets

```bash
base64 -i brewpilot-release.keystore | gh secret set ANDROID_KEYSTORE_BASE64 -R ggazzo/brewpilot
gh secret set ANDROID_KEYSTORE_PASSWORD -R ggazzo/brewpilot --body "<keystore password>"
gh secret set ANDROID_KEY_ALIAS         -R ggazzo/brewpilot --body "brewpilot"
gh secret set ANDROID_KEY_PASSWORD      -R ggazzo/brewpilot --body "<key password>"
```

The next `Build Android APK` run picks them up automatically (the
workflow checks for `ANDROID_KEYSTORE_BASE64`; everything else is
threaded through gradle properties).

## Verify

After the workflow runs the asset filename will be
`brewpilot-<tag>-android-release.apk` instead of
`brewpilot-<tag>-android-debug.apk`. Confirm the signature with
`apksigner verify -v <apk>` — the output should show your custom key
fingerprint, not the Android debug certificate.

## Lose the keystore? (recovery)

There isn't one. Regenerate, change the Android package id (e.g.
`com.brewpilot.controller` → `com.brewpilot.controller2`), and tell
existing users to uninstall + reinstall. Avoid this — back up the
keystore file plus both passwords in two physical locations.
