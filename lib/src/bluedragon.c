/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#include "bluedragon.h"
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct m711 {
    int fd;
    m711_ops ops;
    void *ctx;
};

static int hidraw_set(void *ctx, const uint8_t *b, size_t n) {
    return ioctl(*(int *)ctx, HIDIOCSFEATURE(n), b) < 0 ? -1 : 0;
}

static int hidraw_get(void *ctx, uint8_t *b, size_t n) {
    return ioctl(*(int *)ctx, HIDIOCGFEATURE(n), b) < 0 ? -1 : 0;
}

static const uint16_t base[M711_PROFILES] = {0x040, 0x100, 0x1b0, 0x260, 0x310};
const uint8_t m711_dpi_code[51] = {
    0, 2, 4, 6, 8, 11, 13, 15, 18, 20, 22, 25, 27, 29, 32, 34, 36, 39, 41, 43, 46, 48, 50, 52, 55, 57,
    59, 62, 64, 66, 69, 71, 73, 76, 78, 80, 83, 85, 87, 90, 92, 94, 97, 99, 101, 104, 106, 108, 111, 113, 115};

uint16_t m711_profile_base(int p) { return base[p]; }

static long sysfs_hex(const char *dir, const char *rel, const char *name) {
    char path[PATH_MAX + 32], line[32];
    snprintf(path, sizeof path, "%s/%s%s", dir, rel, name);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    long v = fgets(line, sizeof line, f) ? strtol(line, NULL, 16) : -1;
    fclose(f);
    return v;
}

static int find_node(char *out, size_t n) {
    DIR *d = opendir("/sys/class/hidraw");
    struct dirent *e;
    if (!d) return -1;
    while ((e = readdir(d))) {
        char dev[512], real[PATH_MAX];
        if (strncmp(e->d_name, "hidraw", 6)) continue;
        snprintf(dev, sizeof dev, "/sys/class/hidraw/%s/device", e->d_name);
        if (!realpath(dev, real)) continue;
        if (sysfs_hex(real, "../", "bInterfaceNumber") == M711_IFACE &&
            sysfs_hex(real, "../../", "idVendor") == M711_VID &&
            sysfs_hex(real, "../../", "idProduct") == M711_PID) {
            snprintf(out, n, "/dev/%s", e->d_name);
            closedir(d);
            return 0;
        }
    }
    closedir(d);
    return -1;
}

m711 *m711_open(const char *path) {
    char auto_path[300];
    if (!path) {
        if (find_node(auto_path, sizeof auto_path)) { errno = ENODEV; return NULL; }
        path = auto_path;
    }
    int fd = open(path, O_RDWR);
    if (fd < 0) return NULL;
    m711 *d = malloc(sizeof *d);
    if (!d) { close(fd); return NULL; }
    d->fd = fd;
    d->ops = (m711_ops){hidraw_set, hidraw_get, 100000};
    d->ctx = &d->fd;
    return d;
}

m711 *m711_open_custom(const m711_ops *ops, void *ctx) {
    m711 *d = calloc(1, sizeof *d);
    if (!d) return NULL;
    d->fd = -1;
    d->ops = *ops;
    d->ctx = ctx;
    return d;
}

void m711_close(m711 *d) {
    if (!d) return;
    if (d->fd >= 0) close(d->fd);
    free(d);
}

static int set_feature(m711 *d, const uint8_t *b, size_t n) {
    return d->ops.set_feature(d->ctx, b, n);
}

static int get_feature(m711 *d, uint8_t *b, size_t n) {
    return d->ops.get_feature(d->ctx, b, n);
}

int m711_read(m711 *d, uint16_t addr, void *buf, size_t len) {
    uint8_t *out = buf;
    while (len) {
        size_t n = len > 7 ? (len > 32 ? 32 : len) : len;
        uint8_t id = n > 7 ? 3 : 2;
        size_t size = id == 3 ? 64 : 16;
        uint8_t req[64] = {id, 0xf2, addr & 0xff, addr >> 8, n};
        if (set_feature(d, req, size)) return -1;
        memset(req, 0, size);
        req[0] = id;
        if (get_feature(d, req, size)) return -1;
        memcpy(out, req + 8, n);
        out += n; addr += n; len -= n;
    }
    return 0;
}

int m711_write(m711 *d, uint16_t addr, const void *buf, size_t len) {
    const uint8_t *in = buf;
    while (len) {
        size_t n = len > 7 ? 7 : len;
        uint8_t req[16] = {2, 0xf3, addr & 0xff, addr >> 8, n};
        memcpy(req + 8, in, n);
        if (set_feature(d, req, sizeof req)) return -1;
        in += n; addr += n; len -= n;
    }
    return 0;
}

int m711_commit(m711 *d, uint8_t flag) {
    uint8_t req[16] = {2, 0xf1, 2, flag};
    return set_feature(d, req, sizeof req);
}

int m711_set_profile(m711 *d, int p) {
    uint8_t v[2] = {p, 0};
    if (p < 0 || p >= M711_PROFILES) { errno = EINVAL; return -1; }
    if (m711_write(d, 0x2c, v, 2) || m711_commit(d, 1)) return -1;
    return 0;
}

static int step_of(uint8_t code) {
    for (int i = 0; i < 51; i++)
        if (m711_dpi_code[i] == code) return i;
    return -1;
}

int m711_get_dpi(m711 *d, int p, int lvl) {
    uint8_t e[4];
    if (p < 0 || p >= M711_PROFILES || lvl < 0 || lvl >= M711_DPI_LEVELS) { errno = EINVAL; return -1; }
    if (m711_read(d, base[p] + 4 + 6 * lvl, e, 4)) return -1;
    int s = step_of(e[1]);
    if (s < 0) { errno = EPROTO; return -1; }
    return s * 100;
}

int m711_set_dpi(m711 *d, int p, int lvl, int dpi) {
    uint8_t e[4];
    uint16_t a;
    if (p < 0 || p >= M711_PROFILES || lvl < 0 || lvl >= M711_DPI_LEVELS ||
        dpi < 100 || dpi > 5000 || dpi % 100) { errno = EINVAL; return -1; }
    a = base[p] + 4 + 6 * lvl;
    if (m711_read(d, a, e, 4)) return -1;   /* keep unknown byte 0 */
    e[1] = m711_dpi_code[dpi / 100];
    e[2] = 0;                               /* flag: step >= 51 (unused) */
    e[3] = 0;
    if (m711_write(d, a, e, 4) || m711_commit(d, 2)) return -1;
    if (m711_get_dpi(d, p, lvl) != dpi) { errno = EIO; return -1; }
    return 0;
}

#include <poll.h>
#include <time.h>

/* raw value = USB interval in ms (1,2,4,8). Unverified beyond 1 and 2. */
int m711_polling_hz(int raw) {
    switch (raw) { case 1: return 1000; case 2: return 500; case 4: return 250; case 8: return 125; }
    return 0;
}

int m711_get_polling_raw(m711 *d, int p) {
    uint8_t v;
    if (p < 0 || p >= M711_PROFILES) { errno = EINVAL; return -1; }
    return m711_read(d, 0x32 + 2 * p, &v, 1) ? -1 : v;
}

int m711_set_polling_raw(m711 *d, int p, int raw) {
    uint8_t v[2] = {raw, 0};
    if (p < 0 || p >= M711_PROFILES || raw < 1 || raw > 255) { errno = EINVAL; return -1; }
    if (m711_write(d, 0x32 + 2 * p, v, 2) || m711_commit(d, 8)) return -1;
    if (m711_get_polling_raw(d, p) != raw) { errno = EIO; return -1; }
    return 0;
}

int m711_set_polling(m711 *d, int p, int hz) {
    for (int raw = 1; raw <= 8; raw++)
        if (m711_polling_hz(raw) == hz) return m711_set_polling_raw(d, p, raw);
    errno = EINVAL;
    return -1;
}

static int cmp_dbl(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static int find_iface(int iface, char *out, size_t n) {
    DIR *d = opendir("/sys/class/hidraw");
    struct dirent *e;
    if (!d) return -1;
    while ((e = readdir(d))) {
        char dev[512], real[PATH_MAX];
        if (strncmp(e->d_name, "hidraw", 6)) continue;
        snprintf(dev, sizeof dev, "/sys/class/hidraw/%s/device", e->d_name);
        if (!realpath(dev, real)) continue;
        if (sysfs_hex(real, "../", "bInterfaceNumber") == iface &&
            sysfs_hex(real, "../../", "idVendor") == M711_VID &&
            sysfs_hex(real, "../../", "idProduct") == M711_PID) {
            snprintf(out, n, "/dev/%s", e->d_name);
            closedir(d);
            return 0;
        }
    }
    closedir(d);
    return -1;
}

int m711_measure_rate(double seconds, double *median_us, int *samples) {
    char path[300];
    if (find_iface(0, path, sizeof path)) { errno = ENODEV; return -1; }
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) return -1;
    size_t cap = 1 << 16, n = 0;
    double *dt = malloc(cap * sizeof *dt), last = 0;
    struct timespec t0, t;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (;;) {
        clock_gettime(CLOCK_MONOTONIC, &t);
        double el = (t.tv_sec - t0.tv_sec) + (t.tv_nsec - t0.tv_nsec) / 1e9;
        if (el >= seconds) break;
        struct pollfd pf = {fd, POLLIN, 0};
        if (poll(&pf, 1, 50) <= 0) { last = 0; continue; }
        uint8_t buf[64];
        while (read(fd, buf, sizeof buf) > 0) {
            clock_gettime(CLOCK_MONOTONIC, &t);
            double now = t.tv_sec * 1e6 + t.tv_nsec / 1e3;
            if (last > 0 && now - last < 20000 && n < cap) dt[n++] = now - last;
            last = now;
        }
    }
    close(fd);
    *samples = n;
    if (n < 20) { free(dt); errno = ENODATA; return -1; }
    qsort(dt, n, sizeof *dt, cmp_dbl);
    *median_us = dt[n / 2];
    free(dt);
    return 0;
}

#include <ctype.h>
#include <strings.h>

static int session(m711 *d, uint8_t state) {
    uint8_t req[16] = {2, 0xf5, state};
    if (set_feature(d, req, sizeof req)) return -1;
    if (d->ops.settle_us) usleep(d->ops.settle_us);
    return 0;
}

static const struct { const char *name; uint8_t e[4]; } fixed[] = {
    {"left", {0x81, 0, 0, 0}},     {"right", {0x82, 0, 0, 0}},   {"middle", {0x83, 0, 0, 0}},
    {"button4", {0x84, 0, 0, 0}},  {"back", {0x84, 0, 0, 0}},
    {"button5", {0x85, 0, 0, 0}},  {"forward", {0x85, 0, 0, 0}},
    {"none", {0, 0, 0, 0}},
    /* device functions, named from the vendor UI's default button layout */
    {"dpi+", {0x8a, 0, 0, 0}},     {"dpi-", {0x89, 0, 0, 0}},    {"ledmode", {0x9b, 4, 0, 0}},
    {"scrollup", {0x8b, 0, 0, 0}}, {"scrolldown", {0x8c, 0, 0, 0}},
    {"stop", {0x8e, 1, 0xb7, 0}},  {"playpause", {0x8e, 1, 0xcd, 0}},
    {"prev", {0x8e, 1, 0xb6, 0}},  {"next", {0x8e, 1, 0xb5, 0}},
    {"volup", {0x8e, 1, 0xe9, 0}}, {"voldown", {0x8e, 1, 0xea, 0}},
    {"mute", {0x8e, 1, 0xe2, 0}},
};

static const struct { const char *name; uint8_t usage; } keys[] = {
    {"enter", 40}, {"esc", 41}, {"backspace", 42}, {"tab", 43}, {"space", 44},
    {"minus", 45}, {"equal", 46}, {"lbracket", 47}, {"rbracket", 48}, {"backslash", 49},
    {"semicolon", 51}, {"quote", 52}, {"grave", 53}, {"comma", 54}, {"dot", 55}, {"slash", 56},
    {"capslock", 57}, {"printscreen", 70}, {"scrolllock", 71}, {"pause", 72},
    {"insert", 73}, {"home", 74}, {"pageup", 75}, {"delete", 76}, {"end", 77}, {"pagedown", 78},
    {"right", 79}, {"left", 80}, {"down", 81}, {"up", 82},
};

static const struct { const char *name; uint8_t bit; } mods[] = {
    {"ctrl", 1}, {"shift", 2}, {"alt", 4}, {"win", 8},
};

static int key_usage(const char *s) {
    if (strlen(s) == 1 && isalpha((unsigned char)*s)) return 4 + tolower(*s) - 'a';
    if (strlen(s) == 1 && isdigit((unsigned char)*s)) return *s == '0' ? 39 : 30 + *s - '1';
    if ((*s == 'f' || *s == 'F') && isdigit((unsigned char)s[1])) {
        int n = atoi(s + 1);
        if (n >= 1 && n <= 12) return 57 + n;
    }
    for (size_t i = 0; i < sizeof keys / sizeof *keys; i++)
        if (!strcasecmp(s, keys[i].name)) return keys[i].usage;
    return -1;
}

int m711_action_parse(const char *spec, uint8_t out[4]) {
    for (size_t i = 0; i < sizeof fixed / sizeof *fixed; i++)
        if (!strcasecmp(spec, fixed[i].name)) { memcpy(out, fixed[i].e, 4); return 0; }
    if (!strncasecmp(spec, "raw:", 4)) {
        if (strlen(spec + 4) != 8) return -1;
        for (int i = 0; i < 4; i++) {
            char h[3] = {spec[4 + 2 * i], spec[5 + 2 * i], 0}, *end;
            out[i] = strtoul(h, &end, 16);
            if (*end) return -1;
        }
        return 0;
    }
    if (!strncasecmp(spec, "key:", 4)) {
        char tmp[64], *tok[8], *save;
        int n = 0;
        uint8_t mask = 0;
        if (strlen(spec + 4) >= sizeof tmp) return -1;
        strcpy(tmp, spec + 4);
        if (tmp[0] == '+' || strstr(tmp, "++") || (tmp[0] && tmp[strlen(tmp) - 1] == '+' && strlen(tmp) > 1))
            return -1;
        for (char *t = strtok_r(tmp, "+", &save); t; t = strtok_r(NULL, "+", &save)) {
            if (n == 8) return -1;
            tok[n++] = t;
        }
        if (!n) return -1;
        for (int i = 0; i < n - 1; i++) {      /* all but last must be modifiers */
            int found = 0;
            for (size_t m = 0; m < sizeof mods / sizeof *mods; m++)
                if (!strcasecmp(tok[i], mods[m].name)) { mask |= mods[m].bit; found = 1; }
            if (!found) return -1;
        }
        int usage = key_usage(tok[n - 1]);
        if (usage < 0) {                        /* lone modifier as the key itself */
            if (mask) return -1;
            for (size_t m = 0; m < sizeof mods / sizeof *mods; m++)
                if (!strcasecmp(tok[n - 1], mods[m].name)) {
                    out[0] = 0x90; out[1] = 0; out[2] = 0xe0 + m; out[3] = 0;
                    return 0;
                }
            return -1;
        }
        if (mask) { out[0] = 0x8f; out[1] = mask; out[2] = usage; out[3] = 0; }
        else      { out[0] = 0x90; out[1] = 0; out[2] = usage; out[3] = 0; }
        return 0;
    }
    return -1;
}

void m711_action_name(const uint8_t e[4], char *buf, size_t n) {
    for (size_t i = 0; i < sizeof fixed / sizeof *fixed; i++)
        if (!memcmp(e, fixed[i].e, 4) && strcmp(fixed[i].name, "back") && strcmp(fixed[i].name, "forward")) {
            snprintf(buf, n, "%s", fixed[i].name);
            return;
        }
    if (e[0] == 0x90 && !e[1] && !e[3]) {
        if (e[2] >= 0xe0 && e[2] <= 0xe7) {
            static const char *m[] = {"ctrl", "shift", "alt", "win", "rctrl", "rshift", "ralt", "rwin"};
            snprintf(buf, n, "key:%s", m[e[2] - 0xe0]);
            return;
        }
        for (char ch = 'a'; ch <= 'z'; ch++)
            if (e[2] == 4 + ch - 'a') { snprintf(buf, n, "key:%c", ch); return; }
        for (int f = 1; f <= 12; f++)
            if (e[2] == 57 + f) { snprintf(buf, n, "key:f%d", f); return; }
        for (size_t i = 0; i < sizeof keys / sizeof *keys; i++)
            if (e[2] == keys[i].usage) { snprintf(buf, n, "key:%s", keys[i].name); return; }
    }
    if (e[0] == 0x8f && !e[3]) {
        size_t l = 0;
        l += snprintf(buf + l, n - l, "key:");
        for (size_t m = 0; m < sizeof mods / sizeof *mods; m++)
            if (e[1] & mods[m].bit) l += snprintf(buf + l, n - l, "%s+", mods[m].name);
        uint8_t k[4] = {0x90, 0, e[2], 0};
        char nm[32];
        m711_action_name(k, nm, sizeof nm);
        if (!strncmp(nm, "key:", 4)) { snprintf(buf + l, n - l, "%s", nm + 4); return; }
    }
    snprintf(buf, n, "raw:%02x%02x%02x%02x", e[0], e[1], e[2], e[3]);
}

static int btn_addr(int p, int b, uint16_t *a) {
    if (p < 0 || p >= M711_PROFILES || b < 0 || b >= M711_BUTTONS) { errno = EINVAL; return -1; }
    *a = base[p] + 0x42 + 4 * b;
    return 0;
}

int m711_get_button(m711 *d, int p, int b, uint8_t out[4]) {
    uint16_t a;
    return btn_addr(p, b, &a) ? -1 : m711_read(d, a, out, 4);
}

int m711_set_button(m711 *d, int p, int b, const uint8_t e[4]) {
    uint16_t a;
    uint8_t chk[4];
    if (btn_addr(p, b, &a)) return -1;
    if (session(d, 0)) return -1;
    int rc = m711_write(d, a, e, 4) || m711_commit(d, 4);
    if (session(d, 1) || rc) return -1;
    if (m711_read(d, a, chk, 4)) return -1;
    if (memcmp(chk, e, 4)) { errno = EIO; return -1; }
    return 0;
}

static const uint8_t led_modes[8][2] = {{1, 4}, {1, 8}, {1, 2}, {2, 0}, {6, 0}, {7, 0}, {1, 0x10}, {0, 0}};

int m711_led_mode_to_type(int mode, uint8_t *type, uint8_t *sub) {
    if (mode < 0 || mode > 7) return -1;
    *type = led_modes[mode][0];
    *sub = led_modes[mode][1];
    return 0;
}

int m711_led_mode_of(const m711_led *l) {
    for (int m = 0; m < 8; m++)
        if (l->type == led_modes[m][0] && l->sub == led_modes[m][1]) return m;
    return -1;
}

int m711_get_led(m711 *d, int p, m711_led *l) {
    uint8_t b[7];
    if (p < 0 || p >= M711_PROFILES) { errno = EINVAL; return -1; }
    if (m711_read(d, 0x449 + 8 * p, b, 7)) return -1;
    *l = (m711_led){b[0], b[1], b[2], b[3], b[4], b[5], b[6]};
    return 0;
}

int m711_set_led(m711 *d, int p, const m711_led *l) {
    uint8_t b[7] = {l->r, l->g, l->b, l->type, l->value, l->sub, l->level}, chk[7];
    uint16_t a = 0x449 + 8 * p;
    static const uint8_t flags[] = {0x10, 4, 1, 2, 8};
    if (p < 0 || p >= M711_PROFILES) { errno = EINVAL; return -1; }
    if (session(d, 0)) return -1;
    int rc = m711_write(d, a, b, 6) || m711_write(d, a + 6, b + 6, 1);
    for (size_t i = 0; !rc && i < sizeof flags; i++) rc = m711_commit(d, flags[i]);
    if (session(d, 1) || rc) return -1;
    if (m711_read(d, a, chk, 7)) return -1;
    if (memcmp(chk, b, 7)) { errno = EIO; return -1; }
    return 0;
}

int m711_get_dpi_stage(m711 *d, int p) {
    uint8_t v;
    if (p < 0 || p >= M711_PROFILES) { errno = EINVAL; return -1; }
    return m711_read(d, base[p] + 2, &v, 1) ? -1 : v;
}

int m711_open_input(void) {
    char path[300];
    if (find_iface(0, path, sizeof path)) { errno = ENODEV; return -1; }
    return open(path, O_RDONLY | O_NONBLOCK);
}

/* Report layout from the interface-0 descriptor: 16 button bits, X and Y (16 bit each), wheel, pan. */
int m711_parse_mouse_report(const uint8_t *buf, size_t len, m711_mouse_state *st) {
    if (len < 8) { errno = EINVAL; return -1; }
    st->buttons = buf[0] | (buf[1] << 8);
    st->wheel = (int8_t)buf[6];
    return 0;
}
