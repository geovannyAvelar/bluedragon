/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
/* Unit tests for libm711 against a simulated device (no hardware needed). */
#include <bluedragon.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures, checks;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
    fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_ERR(call, err) do { errno = 0; int r_ = (call); CHECK(r_ == -1 && errno == (err)); } while (0)
#define RUN(fn) do { int f0 = failures; fn(); printf("%-32s %s\n", #fn, failures == f0 ? "ok" : "FAILED"); } while (0)

/* ---- simulated device ------------------------------------------------- */
enum { LOG_READ_REQ, LOG_WRITE, LOG_COMMIT, LOG_BEGIN, LOG_END };
struct event { int kind; unsigned addr, len, flag; };

struct dev {
    uint8_t mem[0x800];
    struct event log[512];
    int nlog;
    int pend_valid; uint8_t pend_id; unsigned pend_addr, pend_len;
    int fail_set_after;     /* -1 = never; else fail exactly the (n+1)th set_feature */
    int nset;
    int drop_writes;        /* accept writes but do not store them */
    int reports[4];         /* count per report id */
};

static void ev(struct dev *d, int kind, unsigned addr, unsigned len, unsigned flag) {
    if (d->nlog < 512) d->log[d->nlog++] = (struct event){kind, addr, len, flag};
}

static int dev_set(void *ctx, const uint8_t *b, size_t n) {
    struct dev *d = ctx;
    if (d->fail_set_after >= 0 && d->nset++ == d->fail_set_after) { errno = EIO; return -1; }
    if ((b[0] != 2 || n != 16) && (b[0] != 3 || n != 64)) return -1;     /* only report 2/3 */
    d->reports[b[0]]++;
    unsigned addr = b[2] | b[3] << 8, len = b[4];
    switch (b[1]) {
    case 0xf2:
        if (len > (b[0] == 2 ? 7u : 32u) || addr + len > sizeof d->mem) return -1;
        d->pend_valid = 1; d->pend_id = b[0]; d->pend_addr = addr; d->pend_len = len;
        ev(d, LOG_READ_REQ, addr, len, 0);
        return 0;
    case 0xf3:
        if (len > (b[0] == 2 ? 7u : 32u) || addr + len > sizeof d->mem) return -1;
        if (!d->drop_writes) memcpy(d->mem + addr, b + 8, len);
        ev(d, LOG_WRITE, addr, len, 0);
        return 0;
    case 0xf1:
        if (b[2] != 2) return -1;
        ev(d, LOG_COMMIT, 0, 0, b[3]);
        return 0;
    case 0xf5:
        ev(d, b[2] ? LOG_END : LOG_BEGIN, 0, 0, 0);
        return 0;
    }
    return -1;
}

static int dev_get(void *ctx, uint8_t *b, size_t n) {
    struct dev *d = ctx;
    if (!d->pend_valid || b[0] != d->pend_id || n != (b[0] == 2 ? 16u : 64u)) return -1;
    memset(b + 1, 0, n - 1);
    b[1] = 0xf2; b[2] = d->pend_addr & 255; b[3] = d->pend_addr >> 8; b[4] = d->pend_len;
    memcpy(b + 8, d->mem + d->pend_addr, d->pend_len);
    d->pend_valid = 0;
    return 0;
}

/* Layout seeded from a real M711 dump. */
static void dev_init(struct dev *d) {
    static const uint8_t dpi[2][5] = {{0x12, 0x16, 0x2e, 0x45, 0x73}, {0x0b, 0x16, 0x2e, 0x45, 0x73}};
    memset(d, 0, sizeof *d);
    d->fail_set_after = -1;
    for (int p = 0; p < M711_PROFILES; p++) {
        uint16_t b = m711_profile_base(p);
        d->mem[b] = 5;
        for (int l = 0; l < 5; l++) { d->mem[b + 4 + 6 * l] = 1; d->mem[b + 5 + 6 * l] = dpi[p ? 1 : 0][l]; }
        static const uint8_t btn[12][4] = {{0x81}, {0x82}, {0x83}, {0x85}, {0x84}, {0x8a},
                                           {0x89}, {0x9b, 4}, {0x8d}, {0}, {0x8b}, {0x8c}};
        for (int k = 0; k < 12; k++) memcpy(d->mem + b + 0x42 + 4 * k, btn[k], 4);
        d->mem[0x32 + 2 * p] = p ? 2 : 1;                 /* polling */
        memcpy(d->mem + 0x449 + 8 * p, (uint8_t[]){0xff, 0, 0, 1, 2, 2, 3}, 7);
    }
}

static m711 *attach(struct dev *d) {
    static const m711_ops ops_t = {dev_set, dev_get, 0};
    m711 *m = m711_open_custom(&ops_t, d);
    CHECK(m != NULL);
    return m;
}

static int count_kind(const struct dev *d, int kind) {
    int n = 0;
    for (int i = 0; i < d->nlog; i++) n += d->log[i].kind == kind;
    return n;
}

/* ---- tests ------------------------------------------------------------ */
static void test_profile_base(void) {
    CHECK(m711_profile_base(0) == 0x040);
    CHECK(m711_profile_base(1) == 0x100);
    CHECK(m711_profile_base(2) == 0x1b0);
    CHECK(m711_profile_base(3) == 0x260);
    CHECK(m711_profile_base(4) == 0x310);
}

static void test_read_chunking(void) {
    struct dev d; dev_init(&d);
    for (int i = 0; i < 0x200; i++) d.mem[0x400 + i] = (uint8_t)(i * 7 + 1);
    m711 *m = attach(&d);
    uint8_t buf[300];
    int sizes[] = {1, 7, 8, 32, 33, 100, 257};
    for (size_t s = 0; s < sizeof sizes / sizeof *sizes; s++) {
        memset(buf, 0, sizeof buf);
        CHECK(m711_read(m, 0x400, buf, sizes[s]) == 0);
        CHECK(memcmp(buf, d.mem + 0x400, sizes[s]) == 0);
        CHECK(buf[sizes[s]] == 0);                     /* no overrun */
    }
    d.reports[2] = d.reports[3] = 0;
    CHECK(m711_read(m, 0x400, buf, 7) == 0);
    CHECK(d.reports[2] == 1 && d.reports[3] == 0);     /* small read -> report 2 */
    d.reports[2] = d.reports[3] = 0;
    CHECK(m711_read(m, 0x400, buf, 8) == 0);
    CHECK(d.reports[3] == 1 && d.reports[2] == 0);     /* >7 -> report 3 */
    m711_close(m);
}

static void test_write_chunking(void) {
    struct dev d; dev_init(&d);
    m711 *m = attach(&d);
    uint8_t src[20];
    for (int i = 0; i < 20; i++) src[i] = 0xa0 + i;
    CHECK(m711_write(m, 0x500, src, 20) == 0);
    CHECK(memcmp(d.mem + 0x500, src, 20) == 0);
    CHECK(d.mem[0x514] == 0);                          /* no overrun */
    CHECK(count_kind(&d, LOG_WRITE) == 3);             /* 7 + 7 + 6 */
    CHECK(d.log[0].addr == 0x500 && d.log[0].len == 7);
    CHECK(d.log[1].addr == 0x507 && d.log[1].len == 7);
    CHECK(d.log[2].addr == 0x50e && d.log[2].len == 6);
    m711_close(m);
}

#define FAIL_FIRST(d) ((d).fail_set_after = 0, (d).nset = 0)
static void test_transport_errors(void) {
    struct dev d; dev_init(&d);
    m711 *m = attach(&d);
    uint8_t b[4] = {0};
    FAIL_FIRST(d); CHECK_ERR(m711_read(m, 0x40, b, 4), EIO);
    FAIL_FIRST(d); CHECK_ERR(m711_write(m, 0x40, b, 4), EIO);
    FAIL_FIRST(d); CHECK_ERR(m711_commit(m, 1), EIO);
    FAIL_FIRST(d); CHECK_ERR(m711_set_profile(m, 1), EIO);
    FAIL_FIRST(d); CHECK_ERR(m711_set_dpi(m, 0, 0, 1000), EIO);
    FAIL_FIRST(d); CHECK_ERR(m711_set_polling(m, 0, 500), EIO);
    FAIL_FIRST(d); CHECK_ERR(m711_set_button(m, 0, 0, (uint8_t[]){0x82, 0, 0, 0}), EIO);
    FAIL_FIRST(d); CHECK_ERR(m711_set_led(m, 0, &(m711_led){1, 2, 3, 1, 4, 4, 1}), EIO);
    FAIL_FIRST(d); CHECK_ERR(m711_get_dpi(m, 0, 0), EIO);
    d.fail_set_after = -1;
    CHECK(m711_get_dpi(m, 0, 0) == 800);                 /* device usable again afterwards */
    m711_close(m);
}

static void test_set_profile(void) {
    struct dev d; dev_init(&d);
    m711 *m = attach(&d);
    CHECK(m711_set_profile(m, 3) == 0);
    CHECK(d.mem[0x2c] == 3);
    CHECK(d.log[d.nlog - 1].kind == LOG_COMMIT && d.log[d.nlog - 1].flag == 1);
    d.nlog = 0;
    CHECK_ERR(m711_set_profile(m, 5), EINVAL);
    CHECK_ERR(m711_set_profile(m, -1), EINVAL);
    CHECK(d.nlog == 0);                                /* nothing sent */
    m711_close(m);
}

static void test_dpi_table(void) {
    CHECK(m711_dpi_code[0] == 0 && m711_dpi_code[50] == 115);
    for (int i = 1; i < 51; i++) CHECK(m711_dpi_code[i] > m711_dpi_code[i - 1]);
    CHECK(m711_dpi_code[8] == 0x12 && m711_dpi_code[10] == 0x16 &&
          m711_dpi_code[20] == 0x2e && m711_dpi_code[30] == 0x45);
}

static void test_dpi_get(void) {
    struct dev d; dev_init(&d);
    m711 *m = attach(&d);
    int want[5] = {800, 1000, 2000, 3000, 5000};
    for (int l = 0; l < 5; l++) CHECK(m711_get_dpi(m, 0, l) == want[l]);
    CHECK(m711_get_dpi(m, 1, 0) == 500);
    d.mem[0x40 + 5] = 0x13;                            /* code not in table */
    CHECK_ERR(m711_get_dpi(m, 0, 0), EPROTO);
    CHECK_ERR(m711_get_dpi(m, 5, 0), EINVAL);
    CHECK_ERR(m711_get_dpi(m, 0, 5), EINVAL);
    CHECK_ERR(m711_get_dpi(m, -1, 0), EINVAL);
    m711_close(m);
}

static void test_dpi_set(void) {
    struct dev d; dev_init(&d);
    m711 *m = attach(&d);
    CHECK(m711_set_dpi(m, 2, 3, 1600) == 0);
    uint16_t a = 0x1b0 + 4 + 6 * 3;
    CHECK(d.mem[a] == 1);                              /* unknown byte preserved */
    CHECK(d.mem[a + 1] == m711_dpi_code[16]);
    CHECK(d.mem[a + 2] == 0 && d.mem[a + 3] == 0);
    CHECK(m711_get_dpi(m, 2, 3) == 1600);
    CHECK(d.log[d.nlog - 4].kind == LOG_WRITE || count_kind(&d, LOG_COMMIT) == 1);
    int ci = -1;
    for (int i = 0; i < d.nlog; i++) if (d.log[i].kind == LOG_COMMIT) ci = i;
    CHECK(ci >= 0 && d.log[ci].flag == 2);
    /* neighbours untouched */
    CHECK(m711_get_dpi(m, 2, 2) == 500 || m711_get_dpi(m, 2, 2) == 2000);
    CHECK(m711_get_dpi(m, 1, 3) == 3000);
    for (int step = 1; step <= 50; step++) {           /* every legal value round-trips */
        CHECK(m711_set_dpi(m, 0, 0, step * 100) == 0);
        CHECK(m711_get_dpi(m, 0, 0) == step * 100);
    }
    m711_close(m);
}

static void test_dpi_set_invalid(void) {
    struct dev d; dev_init(&d);
    m711 *m = attach(&d);
    int bad[] = {0, 50, 99, 850, 5100, 10000, -100};
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) CHECK_ERR(m711_set_dpi(m, 0, 0, bad[i]), EINVAL);
    CHECK_ERR(m711_set_dpi(m, 5, 0, 800), EINVAL);
    CHECK_ERR(m711_set_dpi(m, 0, 5, 800), EINVAL);
    CHECK_ERR(m711_set_dpi(m, -1, 0, 800), EINVAL);
    CHECK(d.nlog == 0);                                /* validation happens before I/O */
    m711_close(m);
}

static void test_dpi_verify_failure(void) {
    struct dev d; dev_init(&d);
    d.drop_writes = 1;
    m711 *m = attach(&d);
    CHECK_ERR(m711_set_dpi(m, 0, 0, 1600), EIO);       /* read-back mismatch is reported */
    m711_close(m);
}

static void test_dpi_stage(void) {
    struct dev d; dev_init(&d);
    d.mem[0x1b0 + 2] = 2;
    m711 *m = attach(&d);
    CHECK(m711_get_dpi_stage(m, 0) == 0);
    CHECK(m711_get_dpi_stage(m, 2) == 2);
    CHECK_ERR(m711_get_dpi_stage(m, 5), EINVAL);
    m711_close(m);
}

static void test_polling(void) {
    struct dev d; dev_init(&d);
    m711 *m = attach(&d);
    CHECK(m711_get_polling_raw(m, 0) == 1);
    CHECK(m711_get_polling_raw(m, 1) == 2);
    CHECK(m711_polling_hz(1) == 1000 && m711_polling_hz(2) == 500 &&
          m711_polling_hz(4) == 250 && m711_polling_hz(8) == 125);
    CHECK(m711_polling_hz(0) == 0 && m711_polling_hz(3) == 0 && m711_polling_hz(255) == 0);
    CHECK(m711_set_polling(m, 0, 500) == 0);
    CHECK(d.mem[0x32] == 2 && d.mem[0x33] == 0);
    CHECK(m711_set_polling(m, 4, 125) == 0);
    CHECK(d.mem[0x3a] == 8);
    CHECK(d.mem[0x34] == 2);                           /* other profiles untouched */
    int ci = -1;
    for (int i = 0; i < d.nlog; i++) if (d.log[i].kind == LOG_COMMIT) ci = i;
    CHECK(ci >= 0 && d.log[ci].flag == 8);
    CHECK(m711_set_polling_raw(m, 2, 3) == 0 && m711_get_polling_raw(m, 2) == 3);
    d.nlog = 0;
    CHECK_ERR(m711_set_polling(m, 0, 700), EINVAL);
    CHECK_ERR(m711_set_polling(m, 9, 500), EINVAL);
    CHECK_ERR(m711_set_polling_raw(m, 0, 0), EINVAL);
    CHECK_ERR(m711_set_polling_raw(m, 0, 256), EINVAL);
    CHECK_ERR(m711_get_polling_raw(m, 5), EINVAL);
    CHECK(d.nlog == 0);
    d.drop_writes = 1;
    CHECK_ERR(m711_set_polling(m, 0, 1000), EIO);
    m711_close(m);
}

static void test_action_parse(void) {
    static const struct { const char *spec; uint8_t e[4]; } ok[] = {
        {"left", {0x81, 0, 0, 0}},   {"RIGHT", {0x82, 0, 0, 0}},  {"middle", {0x83, 0, 0, 0}},
        {"button4", {0x84, 0, 0, 0}}, {"back", {0x84, 0, 0, 0}},
        {"button5", {0x85, 0, 0, 0}}, {"forward", {0x85, 0, 0, 0}}, {"none", {0, 0, 0, 0}},
        {"dpi+", {0x8a, 0, 0, 0}}, {"dpi-", {0x89, 0, 0, 0}}, {"ledmode", {0x9b, 4, 0, 0}},
        {"scrollup", {0x8b, 0, 0, 0}}, {"scrolldown", {0x8c, 0, 0, 0}},
        {"dpi+", {0x8a, 0, 0, 0}}, {"dpi-", {0x89, 0, 0, 0}}, {"ledmode", {0x9b, 4, 0, 0}},
        {"scrollup", {0x8b, 0, 0, 0}}, {"scrolldown", {0x8c, 0, 0, 0}},
        {"stop", {0x8e, 1, 0xb7, 0}}, {"playpause", {0x8e, 1, 0xcd, 0}}, {"prev", {0x8e, 1, 0xb6, 0}},
        {"next", {0x8e, 1, 0xb5, 0}}, {"volup", {0x8e, 1, 0xe9, 0}}, {"voldown", {0x8e, 1, 0xea, 0}},
        {"mute", {0x8e, 1, 0xe2, 0}},
        {"key:a", {0x90, 0, 4, 0}},   {"key:Z", {0x90, 0, 29, 0}}, {"key:1", {0x90, 0, 30, 0}},
        {"key:0", {0x90, 0, 39, 0}},  {"key:f1", {0x90, 0, 58, 0}}, {"key:F12", {0x90, 0, 69, 0}},
        {"key:enter", {0x90, 0, 40, 0}}, {"key:space", {0x90, 0, 44, 0}}, {"key:delete", {0x90, 0, 76, 0}},
        {"key:up", {0x90, 0, 82, 0}}, {"key:pagedown", {0x90, 0, 78, 0}},
        {"key:ctrl", {0x90, 0, 0xe0, 0}}, {"key:shift", {0x90, 0, 0xe1, 0}},
        {"key:alt", {0x90, 0, 0xe2, 0}}, {"key:win", {0x90, 0, 0xe3, 0}},
        {"key:ctrl+c", {0x8f, 1, 6, 0}}, {"key:ctrl+shift+a", {0x8f, 3, 4, 0}},
        {"key:alt+f4", {0x8f, 4, 61, 0}}, {"key:win+ctrl+alt+shift+delete", {0x8f, 15, 76, 0}},
        {"raw:8a000000", {0x8a, 0, 0, 0}}, {"raw:9B040000", {0x9b, 4, 0, 0}},
    };
    for (size_t i = 0; i < sizeof ok / sizeof *ok; i++) {
        uint8_t e[4] = {9, 9, 9, 9};
        int r = m711_action_parse(ok[i].spec, e);
        if (r || memcmp(e, ok[i].e, 4)) fprintf(stderr, "  spec '%s'\n", ok[i].spec);
        CHECK(r == 0 && memcmp(e, ok[i].e, 4) == 0);
    }
    static const char *bad[] = {"", "bogus", "key:", "key:foo", "key:ctrl+foo+a", "key:shift+shift",
        "key:a+b", "key:f13", "key:f0", "key:ctrl+ctrl", "raw:", "raw:8a", "raw:8a00000", "raw:8a0000000",
        "raw:zz000000", "left ", "key:ctrl+a+b", "key:ctrl++a", "key:+a", "key:a+", "key:ctrl+"};
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        uint8_t e[4];
        if (m711_action_parse(bad[i], e) == 0) fprintf(stderr, "  accepted '%s'\n", bad[i]);
        CHECK(m711_action_parse(bad[i], e) == -1);
    }
}

static void test_action_name(void) {
    static const char *roundtrip[] = {"left", "right", "middle", "button4", "button5", "none", "stop",
        "dpi+", "dpi-", "ledmode", "scrollup", "scrolldown",
        "playpause", "prev", "next", "volup", "voldown", "mute", "key:a", "key:f5", "key:enter",
        "key:ctrl", "key:shift", "key:alt", "key:win", "key:ctrl+shift+a", "key:ctrl+alt+delete",
        "key:win+e", "raw:8d000000", "raw:88000000"};
    for (size_t i = 0; i < sizeof roundtrip / sizeof *roundtrip; i++) {
        uint8_t e[4]; char n[64];
        CHECK(m711_action_parse(roundtrip[i], e) == 0);
        m711_action_name(e, n, sizeof n);
        if (strcmp(n, roundtrip[i])) fprintf(stderr, "  '%s' -> '%s'\n", roundtrip[i], n);
        CHECK(strcmp(n, roundtrip[i]) == 0);
    }
    char n[64];
    m711_action_name((uint8_t[]){0x77, 1, 2, 3}, n, sizeof n);
    CHECK(strcmp(n, "raw:77010203") == 0);               /* unknown -> raw hex */
    m711_action_name((uint8_t[]){0x81, 0, 0, 0}, n, 4);  /* tiny buffer: truncates, no overrun */
    CHECK(strlen(n) <= 3);
    n[0] = 'x';
    m711_action_name((uint8_t[]){0x81, 0, 0, 0}, n, 0);  /* zero-size buffer: untouched */
    CHECK(n[0] == 'x');
}

static void test_buttons(void) {
    struct dev d; dev_init(&d);
    m711 *m = attach(&d);
    uint8_t e[4];
    CHECK(m711_get_button(m, 0, 3, e) == 0 && e[0] == 0x85);
    CHECK(m711_get_button(m, 3, 7, e) == 0 && e[0] == 0x9b && e[1] == 4);
    CHECK_ERR(m711_get_button(m, 5, 0, e), EINVAL);
    CHECK_ERR(m711_get_button(m, 0, 12, e), EINVAL);
    CHECK_ERR(m711_get_button(m, 0, -1, e), EINVAL);

    uint8_t act[4] = {0x8f, 3, 4, 0};
    CHECK(m711_set_button(m, 1, 5, act) == 0);
    CHECK(memcmp(d.mem + 0x100 + 0x42 + 20, act, 4) == 0);
    /* neighbours untouched */
    CHECK(d.mem[0x100 + 0x42 + 16] == 0x84 && d.mem[0x100 + 0x42 + 24] == 0x89);
    /* sequence: begin, write(4), commit 4, end */
    CHECK(d.nlog >= 5);
    int b = -1, w = -1, c = -1, en = -1;
    for (int i = 0; i < d.nlog; i++) {
        if (d.log[i].kind == LOG_BEGIN) b = i;
        if (d.log[i].kind == LOG_WRITE) w = i;
        if (d.log[i].kind == LOG_COMMIT && d.log[i].flag == 4) c = i;
        if (d.log[i].kind == LOG_END) en = i;
    }
    CHECK(b >= 0 && b < w && w < c && c < en);
    CHECK(d.log[w].addr == 0x100 + 0x42 + 20 && d.log[w].len == 4);
    CHECK(count_kind(&d, LOG_BEGIN) == 1 && count_kind(&d, LOG_END) == 1);

    d.nlog = 0;
    CHECK_ERR(m711_set_button(m, 5, 0, act), EINVAL);
    CHECK_ERR(m711_set_button(m, 0, 12, act), EINVAL);
    CHECK(d.nlog == 0);
    d.drop_writes = 1;
    CHECK_ERR(m711_set_button(m, 0, 0, (uint8_t[]){0x82, 0, 0, 0}), EIO);
    CHECK(count_kind(&d, LOG_END) == 1);                 /* session still closed on failure path */
    m711_close(m);
}

static void test_session_closed_on_error(void) {
    /* write or commit fails mid-sequence: the end marker must still go out */
    for (int fail_at = 1; fail_at <= 2; fail_at++) {     /* 0 = begin, 1 = write, 2 = commit */
        struct dev d; dev_init(&d);
        m711 *m = attach(&d);
        d.fail_set_after = fail_at; d.nset = 0;
        CHECK_ERR(m711_set_button(m, 0, 0, (uint8_t[]){0x82, 0, 0, 0}), EIO);
        CHECK(count_kind(&d, LOG_BEGIN) == 1 && count_kind(&d, LOG_END) == 1);
        m711_close(m);

        dev_init(&d);
        m = attach(&d);
        d.fail_set_after = fail_at; d.nset = 0;
        CHECK_ERR(m711_set_led(m, 0, &(m711_led){1, 2, 3, 1, 4, 4, 1}), EIO);
        CHECK(count_kind(&d, LOG_BEGIN) == 1 && count_kind(&d, LOG_END) == 1);
        m711_close(m);
    }
}

static void test_led_modes(void) {
    uint8_t t, s;
    static const uint8_t want[8][2] = {{1, 4}, {1, 8}, {1, 2}, {2, 0}, {6, 0}, {7, 0}, {1, 0x10}, {0, 0}};
    for (int mode = 0; mode < 8; mode++) {
        CHECK(m711_led_mode_to_type(mode, &t, &s) == 0);
        CHECK(t == want[mode][0] && s == want[mode][1]);
        m711_led l = {.type = t, .sub = s};
        CHECK(m711_led_mode_of(&l) == mode);
    }
    CHECK(m711_led_mode_to_type(8, &t, &s) == -1);
    CHECK(m711_led_mode_to_type(-1, &t, &s) == -1);
    m711_led u = {.type = 9, .sub = 9};
    CHECK(m711_led_mode_of(&u) == -1);
}

static void test_led_get_set(void) {
    struct dev d; dev_init(&d);
    m711 *m = attach(&d);
    m711_led l;
    CHECK(m711_get_led(m, 0, &l) == 0);
    CHECK(l.r == 0xff && l.g == 0 && l.b == 0 && l.type == 1 && l.value == 2 && l.sub == 2 && l.level == 3);
    CHECK(m711_led_mode_of(&l) == 2);

    l = (m711_led){0x12, 0x34, 0x56, 1, 8, 4, 2};
    d.nlog = 0;
    CHECK(m711_set_led(m, 3, &l) == 0);
    struct event seq[32]; int ns = 0;                    /* drop read-back requests from the log */
    for (int k = 0; k < d.nlog; k++) if (d.log[k].kind != LOG_READ_REQ) seq[ns++] = d.log[k];
    memcpy(d.log, seq, ns * sizeof *seq); d.nlog = ns;
    CHECK(memcmp(d.mem + 0x449 + 24, (uint8_t[]){0x12, 0x34, 0x56, 1, 8, 4, 2}, 7) == 0);
    m711_led r;
    CHECK(m711_get_led(m, 3, &r) == 0 && memcmp(&l, &r, sizeof l) == 0);
    CHECK(m711_get_led(m, 0, &r) == 0 && r.r == 0xff);   /* other profile untouched */

    /* sequence: begin, 6-byte write, 1-byte write, commits 10 4 1 2 8, end */
    int i = 0;
    CHECK(d.log[i++].kind == LOG_BEGIN);
    CHECK(d.log[i].kind == LOG_WRITE && d.log[i].addr == 0x449 + 24 && d.log[i].len == 6); i++;
    CHECK(d.log[i].kind == LOG_WRITE && d.log[i].addr == 0x449 + 24 + 6 && d.log[i].len == 1); i++;
    static const unsigned flags[] = {0x10, 4, 1, 2, 8};
    for (int f = 0; f < 5; f++) { CHECK(d.log[i].kind == LOG_COMMIT && d.log[i].flag == flags[f]); i++; }
    CHECK(d.log[i].kind == LOG_END);

    d.nlog = 0;
    CHECK_ERR(m711_set_led(m, 5, &l), EINVAL);
    CHECK_ERR(m711_set_led(m, -1, &l), EINVAL);
    CHECK_ERR(m711_get_led(m, 5, &r), EINVAL);
    CHECK(d.nlog == 0);
    d.drop_writes = 1;
    CHECK_ERR(m711_set_led(m, 0, &(m711_led){1, 2, 3, 1, 4, 4, 1}), EIO);
    m711_close(m);
}

static void test_open_close(void) {
    m711_close(NULL);                                    /* must not crash */
    struct dev d; dev_init(&d);
    for (int i = 0; i < 100; i++) m711_close(attach(&d));
    errno = 0;
    CHECK(m711_open("/nonexistent/hidraw") == NULL && errno == ENOENT);
}

int main(void) {
    RUN(test_profile_base);
    RUN(test_read_chunking);
    RUN(test_write_chunking);
    RUN(test_transport_errors);
    RUN(test_set_profile);
    RUN(test_dpi_table);
    RUN(test_dpi_get);
    RUN(test_dpi_set);
    RUN(test_dpi_set_invalid);
    RUN(test_dpi_verify_failure);
    RUN(test_dpi_stage);
    RUN(test_polling);
    RUN(test_action_parse);
    RUN(test_action_name);
    RUN(test_buttons);
    RUN(test_session_closed_on_error);
    RUN(test_led_modes);
    RUN(test_led_get_set);
    RUN(test_open_close);
    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
