/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#include "bd-actions.h"

#include <string.h>

static const BdActionPreset presets[] = {
    {"left", "LEFT CLICK"},        {"right", "RIGHT CLICK"},   {"middle", "MIDDLE BUTTON"},
    {"button5", "FORWARD"},        {"button4", "BACKWARD"},    {"dpi+", "DPI +"},
    {"dpi-", "DPI -"},             {"ledmode", "LED MODE SWITCH"}, {"scrollup", "SCROLL UP"},
    {"scrolldown", "SCROLL DOWN"}, {"playpause", "PLAY / PAUSE"}, {"next", "NEXT TRACK"},
    {"prev", "PREVIOUS TRACK"},    {"stop", "STOP"},           {"volup", "VOLUME +"},
    {"voldown", "VOLUME -"},       {"mute", "MUTE"},           {"none", "DISABLED"},
};

const BdActionPreset *bd_action_presets(gsize *n) {
    *n = G_N_ELEMENTS(presets);
    return presets;
}

char *bd_action_label(const char *spec) {
    for (guint i = 0; i < G_N_ELEMENTS(presets); i++)
        if (!strcmp(spec, presets[i].spec)) return g_strdup(presets[i].label);
    char *u = g_ascii_strup(spec, -1);
    if (g_str_has_prefix(u, "KEY:")) {
        char **parts = g_strsplit(u + 4, "+", -1);
        char *joined = g_strjoinv(" + ", parts);
        char *r = g_strconcat("KEY: ", joined, NULL);
        g_strfreev(parts);
        g_free(joined);
        g_free(u);
        return r;
    }
    return u;
}
