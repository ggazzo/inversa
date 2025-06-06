#include "semver.h"

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int parse_int(const char **str) {
    int val = 0;
    while (is_digit(**str)) {
        val = val * 10 + (**str - '0');
        (*str)++;
    }
    return val;
}

static void skip_char(const char **str, char c) {
    if (**str == c) (*str)++;
}

static void parse_core(const char *v, int *major, int *minor, int *patch, const char **suffix) {
    if (*v == 'v') v++; // skip prefix

    *major = parse_int(&v);
    skip_char(&v, '.');
    *minor = parse_int(&v);
    skip_char(&v, '.');
    *patch = parse_int(&v);

    *suffix = v;
}

static int is_valid_core(const char *v) {
    int dots = 0;
    if (*v == 'v') v++;
    while (*v && *v != '-') {
        if (*v == '.') dots++;
        else if (!is_digit(*v)) return 0;
        v++;
    }
    return dots == 2;
}

static int compare_ident(const char *a, const char *b) {
    while (*a && *a != '.' && *a != '-' && *b && *b != '.' && *b != '-') {
        if (*a != *b) return (*a < *b) ? -1 : 1;
        a++; b++;
    }
    if ((*a == 0 || *a == '.' || *a == '-') && (*b == 0 || *b == '.' || *b == '-')) return 0;
    return (*a == 0 || *a == '.' || *a == '-') ? -1 : 1;
}

static int compare_pre(const char *a, const char *b) {
    const char *a_pre = "";
    const char *b_pre = "";

    while (*a && *a != '-') a++;
    if (*a == '-') a_pre = a + 1;

    while (*b && *b != '-') b++;
    if (*b == '-') b_pre = b + 1;

    // Handle: release > pre-release
    if (*a_pre == '\0' && *b_pre != '\0') return 1;
    if (*a_pre != '\0' && *b_pre == '\0') return -1;
    if (*a_pre == '\0' && *b_pre == '\0') return 0;

    // Now compare identifiers
    const char *pa = a_pre;
    const char *pb = b_pre;

    while (*pa || *pb) {
        if (*pa == 0) return -1;
        if (*pb == 0) return 1;

        int is_num_a = is_digit(*pa);
        int is_num_b = is_digit(*pb);

        int val_a = 0, val_b = 0;
        const char *start_a = pa;
        const char *start_b = pb;

        if (is_num_a) {
            while (is_digit(*pa)) val_a = val_a * 10 + (*pa++ - '0');
        } else {
            while (*pa && *pa != '.' && *pa != '-') pa++;
        }

        if (is_num_b) {
            while (is_digit(*pb)) val_b = val_b * 10 + (*pb++ - '0');
        } else {
            while (*pb && *pb != '.' && *pb != '-') pb++;
        }

        if (is_num_a && is_num_b) {
            if (val_a < val_b) return -1;
            if (val_a > val_b) return 1;
        } else if (!is_num_a && !is_num_b) {
            int cmp = compare_ident(start_a, start_b);
            if (cmp != 0) return cmp;
        } else {
            return is_num_a ? -1 : 1;
        }

        if (*pa == '.') pa++;
        if (*pb == '.') pb++;
    }

    return 0;
}


int compareSemVer(const char *va, const char *vb) {
    if (!is_valid_core(va) || !is_valid_core(vb)) return 0;

    int ma = 0, mi = 0, pa = 0;
    int mb = 0, mj = 0, pb = 0;
    const char *sa, *sb;

    parse_core(va, &ma, &mi, &pa, &sa);
    parse_core(vb, &mb, &mj, &pb, &sb);

    if (ma != mb) return (ma > mb) ? 1 : -1;
    if (mi != mj) return (mi > mj) ? 1 : -1;
    if (pa != pb) return (pa > pb) ? 1 : -1;

    return compare_pre(va, vb);
}
