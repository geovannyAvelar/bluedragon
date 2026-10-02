/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#include "m711.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void hexdump(uint16_t a, const uint8_t *b, size_t n) {
    for (size_t i = 0; i < n; i += 16) {
        printf("%04zx ", a + i);
        for (size_t j = i; j < i + 16 && j < n; j++) printf(" %02x", b[j]);
        putchar('\n');
    }
}

static int dump(m711 *d) {
    uint8_t b[6];
    if (m711_read(d, 0x2c, b, 1)) return -1;
    printf("active profile: %d\n", b[0] + 1);
    for (int p = 0; p < M711_PROFILES; p++) {
        int raw = m711_get_polling_raw(d, p);
        printf("polling profile %d: raw %d (%d Hz)\n", p + 1, raw, m711_polling_hz(raw));
    }
    for (int p = 0; p < M711_PROFILES; p++) {
        uint16_t base = m711_profile_base(p);
        printf("-- profile %d @0x%03x\n", p + 1, base);
        m711_led led;
        if (!m711_get_led(d, p, &led))
            printf("   led   #%02x%02x%02x type %d sub %d value %d level %d  (mode %d)\n",
                   led.r, led.g, led.b, led.type, led.sub, led.value, led.level, m711_led_mode_of(&led));
        for (int l = 0; l < M711_DPI_LEVELS; l++) {
            if (m711_read(d, base + 4 + 6 * l, b, 6)) return -1;
            printf("   dpi%d  %02x %02x %02x %02x %02x %02x  %d dpi\n", l + 1,
                   b[0], b[1], b[2], b[3], b[4], b[5], m711_get_dpi(d, p, l));
        }
        for (int k = 0; k < M711_BUTTONS; k++) {
            if (m711_read(d, base + 0x42 + 4 * k, b, 4)) return -1;
            char nm[32];
            m711_action_name(b, nm, sizeof nm);
            printf("   btn%-2d %02x %02x %02x %02x  %s\n", k, b[0], b[1], b[2], b[3], nm);
        }
    }
    return 0;
}

static void usage(void) {
    fputs("usage: m711ctl [-d /dev/hidrawN] <cmd>\n"
          "  dump                       show profiles, DPI levels, buttons\n"
          "  read  <addr> <len>         hex dump device memory\n"
          "  write <addr> <hex bytes..> raw write (no commit)\n"
          "  commit <flag>              send apply command (1=profile 2=dpi)\n"
          "  profile <1-5>              select active profile\n"
          "  polling <profile 1-5> <125|250|500|1000>   (125/250 mapping unverified)\n"
          "  polling-raw <profile 1-5> <1-255>   raw value, for experiments\n"
          "  measure [seconds]          real report rate: keep moving the mouse\n"
          "  button <profile 1-5> <btn 0-11> <action>   e.g. left | key:ctrl+c | volup | raw:8a000000\n"
          "  led <profile 1-5> color <RRGGBB>\n"
          "  led <profile 1-5> mode <0-7> [value]   hid.exe mode number (3,4,6 uncertain)\n"
          "  led <profile 1-5> value <0-255> | level <0-255>\n"
          "  led <profile 1-5> raw <type> <sub> <value> <level>\n"
          "  dpi <profile 1-5> <level 1-5> <100-5000, step 100>\n", stderr);
    exit(2);
}

int main(int argc, char **argv) {
    const char *path = NULL;
    if (argc > 2 && !strcmp(argv[1], "-d")) { path = argv[2]; argc -= 2; argv += 2; }
    if (argc < 2) usage();
    m711 *d = m711_open(path);
    if (!d) { perror("open"); return 1; }
    const char *c = argv[1];
    int rc = 0;
    if (!strcmp(c, "dump")) rc = dump(d);
    else if (!strcmp(c, "read") && argc == 4) {
        uint16_t a = strtoul(argv[2], NULL, 0);
        size_t n = strtoul(argv[3], NULL, 0);
        uint8_t *b = malloc(n);
        rc = b ? m711_read(d, a, b, n) : -1;
        if (!rc) hexdump(a, b, n);
        free(b);
    } else if (!strcmp(c, "write") && argc > 3) {
        uint8_t b[256];
        size_t n = argc - 3;
        if (n > sizeof b) usage();
        for (size_t i = 0; i < n; i++) b[i] = strtoul(argv[3 + i], NULL, 16);
        rc = m711_write(d, strtoul(argv[2], NULL, 0), b, n);
    } else if (!strcmp(c, "commit") && argc == 3) rc = m711_commit(d, strtoul(argv[2], NULL, 0));
    else if (!strcmp(c, "profile") && argc == 3) rc = m711_set_profile(d, atoi(argv[2]) - 1);
    else if (!strcmp(c, "polling") && argc == 4)
        rc = m711_set_polling(d, atoi(argv[2]) - 1, atoi(argv[3]));
    else if (!strcmp(c, "polling-raw") && argc == 4)
        rc = m711_set_polling_raw(d, atoi(argv[2]) - 1, atoi(argv[3]));
    else if (!strcmp(c, "measure")) {
        double us; int n;
        rc = m711_measure_rate(argc > 2 ? atof(argv[2]) : 3.0, &us, &n);
        if (!rc) printf("%d reports, median interval %.0f us = ~%.0f Hz\n", n, us, 1e6 / us);
    } else if (!strcmp(c, "button") && argc == 5) {
        uint8_t e[4];
        if (m711_action_parse(argv[4], e)) {
            fprintf(stderr, "bad action '%s'\n", argv[4]);
            errno = EINVAL;
            rc = -1;
        } else rc = m711_set_button(d, atoi(argv[2]) - 1, atoi(argv[3]), e);
    } else if (!strcmp(c, "led") && argc >= 4) {
        int p = atoi(argv[2]) - 1;
        m711_led l;
        const char *sub = argv[3];
        rc = m711_get_led(d, p, &l);
        if (!rc) {
            if (!strcmp(sub, "color") && argc == 5 && strlen(argv[4]) == 6) {
                unsigned v = strtoul(argv[4], NULL, 16);
                l.r = v >> 16; l.g = v >> 8; l.b = v;
            } else if (!strcmp(sub, "mode") && argc >= 5) {
                if (m711_led_mode_to_type(atoi(argv[4]), &l.type, &l.sub)) { errno = EINVAL; rc = -1; }
                if (argc == 6) l.value = atoi(argv[5]);
            } else if (!strcmp(sub, "value") && argc == 5) l.value = atoi(argv[4]);
            else if (!strcmp(sub, "level") && argc == 5) l.level = atoi(argv[4]);
            else if (!strcmp(sub, "raw") && argc == 8) {
                l.type = atoi(argv[4]); l.sub = atoi(argv[5]); l.value = atoi(argv[6]); l.level = atoi(argv[7]);
            } else usage();
            if (!rc) rc = m711_set_led(d, p, &l);
        }
    } else if (!strcmp(c, "dpi") && argc == 5)
        rc = m711_set_dpi(d, atoi(argv[2]) - 1, atoi(argv[3]) - 1, atoi(argv[4]));
    else usage();
    if (rc) perror(c);
    m711_close(d);
    return rc ? 1 : 0;
}
