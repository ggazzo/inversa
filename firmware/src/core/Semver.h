#pragma once

// ─── Semantic Version Comparison ────────────────────────────
// Compares two semantic version strings (e.g., "v1.2.3", "1.0.0-rc.1").
// Returns: -1 if a < b, 0 if a == b, 1 if a > b
// Returns 0 for invalid versions.
//
// Supports:
// - Optional 'v' prefix
// - Major.minor.patch format
// - Pre-release identifiers (e.g., -alpha, -beta, -rc.1)
// - Release > pre-release (1.0.0 > 1.0.0-rc.1)

namespace Semver {

namespace {

inline bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

inline int parseInt(const char** str) {
    int val = 0;
    while (isDigit(**str)) {
        val = val * 10 + (**str - '0');
        (*str)++;
    }
    return val;
}

inline void skipChar(const char** str, char c) {
    if (**str == c) (*str)++;
}

inline void parseCore(const char* v, int* major, int* minor, int* patch, const char** suffix) {
    if (*v == 'v') v++;  // skip prefix

    *major = parseInt(&v);
    skipChar(&v, '.');
    *minor = parseInt(&v);
    skipChar(&v, '.');
    *patch = parseInt(&v);

    *suffix = v;
}

inline bool isValidCore(const char* v) {
    int dots = 0;
    if (*v == 'v') v++;
    while (*v && *v != '-') {
        if (*v == '.') dots++;
        else if (!isDigit(*v)) return false;
        v++;
    }
    return dots == 2;
}

inline int compareIdent(const char* a, const char* b) {
    while (*a && *a != '.' && *a != '-' && *b && *b != '.' && *b != '-') {
        if (*a != *b) return (*a < *b) ? -1 : 1;
        a++; b++;
    }
    if ((*a == 0 || *a == '.' || *a == '-') && (*b == 0 || *b == '.' || *b == '-')) return 0;
    return (*a == 0 || *a == '.' || *a == '-') ? -1 : 1;
}

inline int comparePre(const char* a, const char* b) {
    const char* a_pre = "";
    const char* b_pre = "";

    while (*a && *a != '-') a++;
    if (*a == '-') a_pre = a + 1;

    while (*b && *b != '-') b++;
    if (*b == '-') b_pre = b + 1;

    // Release > pre-release
    if (*a_pre == '\0' && *b_pre != '\0') return 1;
    if (*a_pre != '\0' && *b_pre == '\0') return -1;
    if (*a_pre == '\0' && *b_pre == '\0') return 0;

    // Compare pre-release identifiers
    const char* pa = a_pre;
    const char* pb = b_pre;

    while (*pa || *pb) {
        if (*pa == 0) return -1;
        if (*pb == 0) return 1;

        bool is_num_a = isDigit(*pa);
        bool is_num_b = isDigit(*pb);

        int val_a = 0, val_b = 0;
        const char* start_a = pa;
        const char* start_b = pb;

        if (is_num_a) {
            while (isDigit(*pa)) val_a = val_a * 10 + (*pa++ - '0');
        } else {
            while (*pa && *pa != '.' && *pa != '-') pa++;
        }

        if (is_num_b) {
            while (isDigit(*pb)) val_b = val_b * 10 + (*pb++ - '0');
        } else {
            while (*pb && *pb != '.' && *pb != '-') pb++;
        }

        if (is_num_a && is_num_b) {
            if (val_a < val_b) return -1;
            if (val_a > val_b) return 1;
        } else if (!is_num_a && !is_num_b) {
            int cmp = compareIdent(start_a, start_b);
            if (cmp != 0) return cmp;
        } else {
            return is_num_a ? -1 : 1;
        }

        if (*pa == '.') pa++;
        if (*pb == '.') pb++;
    }

    return 0;
}

}  // anonymous namespace

// Compare two semantic versions.
// Returns: -1 if va < vb, 0 if va == vb, 1 if va > vb
// Returns 0 for invalid versions.
inline int compare(const char* va, const char* vb) {
    if (!isValidCore(va) || !isValidCore(vb)) return 0;

    int ma = 0, mia = 0, pa = 0;
    int mb = 0, mib = 0, pb = 0;
    const char* sa;
    const char* sb;

    parseCore(va, &ma, &mia, &pa, &sa);
    parseCore(vb, &mb, &mib, &pb, &sb);

    if (ma != mb) return (ma > mb) ? 1 : -1;
    if (mia != mib) return (mia > mib) ? 1 : -1;
    if (pa != pb) return (pa > pb) ? 1 : -1;

    return comparePre(va, vb);
}

// Check if version 'a' is newer than version 'b'
inline bool isNewer(const char* a, const char* b) {
    return compare(a, b) > 0;
}

}  // namespace Semver
