/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#ifndef M711_H
#define M711_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define M711_API __attribute__((visibility("default")))

#define M711_VID 0x04d9
#define M711_PID 0xfc30
#define M711_IFACE 2          /* USB interface carrying the config feature reports */
#define M711_PROFILES 5
#define M711_DPI_LEVELS 5
#define M711_BUTTONS 12

/* All profile, level and button indexes are 0-based. Functions return 0 on
 * success and -1 with errno set on failure, unless stated otherwise. */

typedef struct m711 m711;

/* ---- transport --------------------------------------------------------- */

/* Pluggable transport (HID feature reports). The report id is byte 0 of the
 * buffer. Return 0 on success, -1 on error. settle_us = pause after the
 * begin/end programming markers (hid.exe uses 100000). Lets you drive a
 * simulated device. */
typedef struct {
    int (*set_feature)(void *ctx, const uint8_t *buf, size_t len);
    int (*get_feature)(void *ctx, uint8_t *buf, size_t len);
    unsigned settle_us;
} m711_ops;

M711_API m711 *m711_open_custom(const m711_ops *ops, void *ctx);
M711_API m711 *m711_open(const char *hidraw_path);   /* NULL path = autodetect */
M711_API void m711_close(m711 *d);

/* ---- raw memory access ------------------------------------------------- */

M711_API int m711_read(m711 *d, uint16_t addr, void *buf, size_t len);
M711_API int m711_write(m711 *d, uint16_t addr, const void *buf, size_t len);
/* Apply: 1=profile 2=dpi 4=buttons 8=polling 0x10=? */
M711_API int m711_commit(m711 *d, uint8_t flag);
M711_API uint16_t m711_profile_base(int profile);

/* ---- profile ----------------------------------------------------------- */

M711_API int m711_set_profile(m711 *d, int profile);

/* ---- DPI --------------------------------------------------------------- */

/* DPI = step * 100 (steps 1..50 => 100..5000). Set verifies by read-back.
 * m711_get_dpi returns the DPI, or -1 on error. */
M711_API int m711_set_dpi(m711 *d, int profile, int level, int dpi);
M711_API int m711_get_dpi(m711 *d, int profile, int level);
M711_API extern const uint8_t m711_dpi_code[51];
/* Current DPI stage of a profile (0-based), stored at profile_base + 2. -1 on error. */
M711_API int m711_get_dpi_stage(m711 *d, int profile);

/* ---- polling rate ------------------------------------------------------ */

/* Per-profile byte at 0x32 + 2*profile, commit flag 8.
 * Raw value 1 == 1000 Hz (confirmed: active profile reads 1, USB endpoint is
 * 1 ms). Mapping of the other raw values is a guess: check with
 * m711_measure_rate. get_polling_raw returns the raw value, or -1 on error. */
M711_API int m711_get_polling_raw(m711 *d, int profile);
M711_API int m711_set_polling_raw(m711 *d, int profile, int raw);
M711_API int m711_set_polling(m711 *d, int profile, int hz);   /* 125/250/500/1000 */
M711_API int m711_polling_hz(int raw);                         /* 0 if unknown */
/* Measure the real report rate of the mouse interface (move the mouse!). */
M711_API int m711_measure_rate(double seconds, double *median_us, int *samples);

/* ---- buttons ----------------------------------------------------------- */

/* 12 entries x 4 bytes at profile_base + 0x42 + 4*n (n = 0..11). hid.exe only
 * edits n = 0..7, 10, 11. Writes follow hid.exe's SendAll order:
 * [02 f5 00] begin, write entry, apply flag 4, [02 f5 01] end.
 *
 * Action spec strings:
 *   left right middle button4 button5 back forward none
 *   dpi+ dpi- ledmode scrollup scrolldown   (names inferred from the vendor UI layout)
 *   stop playpause prev next volup voldown mute
 *   key:<combo>   e.g. key:f5  key:enter  key:ctrl+shift+a  (modifier-only ok: key:ctrl)
 *   raw:AABBCCDD  4 raw bytes, hex
 * Combos with modifiers use the 0x8f action: unverified. */
M711_API int m711_action_parse(const char *spec, uint8_t out[4]);
M711_API void m711_action_name(const uint8_t e[4], char *buf, size_t n);
M711_API int m711_get_button(m711 *d, int profile, int btn, uint8_t out[4]);
M711_API int m711_set_button(m711 *d, int profile, int btn, const uint8_t e[4]);

/* ---- LED --------------------------------------------------------------- */

/* One 7-byte record per profile at 0x449 + 8*profile:
 *   [R G B type value sub level]   (+ commit sequence 0x10,4,1,2,8 like hid.exe SendAll)
 * "mode" 0..7 is hid.exe's numbering, mapped to (type,sub):
 *   0=(1,4) 1=(1,8) 2=(1,2) 3=(2,0) 4=(6,0) 5=(7,0) 6=(1,0x10) 7=(0,0)
 * Modes 0,1,2,5,7 agree with hid.exe's read path; 3,4,6 are uncertain.
 * Effect names are unknown. */
typedef struct { uint8_t r, g, b, type, value, sub, level; } m711_led;

M711_API int m711_get_led(m711 *d, int profile, m711_led *led);
M711_API int m711_set_led(m711 *d, int profile, const m711_led *led);
M711_API int m711_led_mode_to_type(int mode, uint8_t *type, uint8_t *sub);
M711_API int m711_led_mode_of(const m711_led *led);   /* -1 if no mode matches */

#ifdef __cplusplus
}
#endif

#endif
