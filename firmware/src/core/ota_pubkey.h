#pragma once

// ─── OTA Signature Public Key (P8) ──────────────────────────
// ECDSA P-256 public key in PEM format.
//
// PROVISIONING:
//  - Private key (`firmware/secrets/ota_priv.pem`) is held by the maintainer;
//    NEVER committed (gitignored). CI signs each release `.bin` with it via
//    the GitHub Secret `OTA_SIGNING_KEY` and uploads `<bin>.sig` as a release
//    asset.
//  - Public key below is embedded in firmware. To rotate:
//      openssl ecparam -name prime256v1 -genkey -noout -out priv.pem
//      openssl ec -in priv.pem -pubout -out pub.pem
//      # paste pub.pem here, ship a release, then update the GitHub Secret.
//
// FAIL-SAFE: if OTA_PUBKEY_PLACEHOLDER is set, the firmware refuses to install
// ANY update — there is no "skip verification" path.

#define OTA_PUBKEY_PLACEHOLDER 0

constexpr const char OTA_PUBKEY_PEM[] =
    "-----BEGIN PUBLIC KEY-----\n"
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEy7IxMdT5VGBE27UJ8MCoKhff45PI\n"
    "ZM5cqr85qyV1i77wRGukNnL6RmUUDkwcLLJR0ry4UlZOI4/+LcCdxtJQbA==\n"
    "-----END PUBLIC KEY-----\n";
