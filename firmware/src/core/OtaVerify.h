#pragma once

#include <Arduino.h>
#ifdef SIM_BUILD
// Simulator: skip mbedTLS entirely. OTA isn't exercised — the verifier
// stub below always returns false, refusing any install attempt.
#else
#include <mbedtls/pk.h>
#include <mbedtls/md.h>
#endif
#include "ota_pubkey.h"
#include "constants.h"

// ─── OTA Signature Verification (P8) ────────────────────────
// Streaming SHA-256 + ECDSA P-256 verification of a downloaded binary
// against the public key in `ota_pubkey.h`. Lets us protect the OTA path
// against MITM even with `WiFiClientSecure::setInsecure()` (Q-01).
//
// Usage:
//   OtaVerify v;
//   v.begin();
//   v.update(buf, n);   // call repeatedly during download
//   if (!v.finishAndVerify(sig, sigLen)) abort();

class OtaVerify {
public:
#ifdef SIM_BUILD
    // Simulator: no-op verifier. Always refuses installs; the sim doesn't
    // exercise the OTA path and we don't want to ship a "verify_unsafe"
    // implementation that could be confused with the real one.
    OtaVerify() = default;
    ~OtaVerify() = default;
    bool begin() { return true; }
    bool update(const uint8_t*, size_t) { return true; }
    bool finishAndVerify(const uint8_t*, size_t) { return false; }
#else
    OtaVerify() { mbedtls_md_init(&_md); }
    ~OtaVerify() { mbedtls_md_free(&_md); }

    bool begin() {
        const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
        if (!info) return false;
        if (mbedtls_md_setup(&_md, info, 0) != 0) return false;
        if (mbedtls_md_starts(&_md) != 0) return false;
        _ready = true;
        return true;
    }

    bool update(const uint8_t* data, size_t len) {
        if (!_ready) return false;
        return mbedtls_md_update(&_md, data, len) == 0;
    }

    // Returns true iff the embedded public key validates `sig` over the
    // SHA-256 of everything passed to update().
    bool finishAndVerify(const uint8_t* sig, size_t sigLen) {
        if (!_ready) return false;

#if OTA_PUBKEY_PLACEHOLDER
        DEBUG_PRINTLN("[OTA] REFUSED: signing key is placeholder — set OTA_PUBKEY_PEM");
        return false;
#endif

        uint8_t hash[32];
        if (mbedtls_md_finish(&_md, hash) != 0) return false;

        mbedtls_pk_context pk;
        mbedtls_pk_init(&pk);
        int ret = mbedtls_pk_parse_public_key(
            &pk,
            reinterpret_cast<const unsigned char*>(OTA_PUBKEY_PEM),
            sizeof(OTA_PUBKEY_PEM)  // includes null terminator (required by mbedTLS)
        );
        if (ret != 0) {
            DEBUG_PRINTF("[OTA] pk_parse_public_key failed: -0x%04X\n", -ret);
            mbedtls_pk_free(&pk);
            return false;
        }

        ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, sizeof(hash), sig, sigLen);
        mbedtls_pk_free(&pk);
        if (ret != 0) {
            DEBUG_PRINTF("[OTA] signature INVALID: -0x%04X\n", -ret);
            return false;
        }
        return true;
    }

private:
    mbedtls_md_context_t _md;
    bool _ready = false;
#endif
};
