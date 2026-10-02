/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
    const char *spec;     /* libbluedragon action spec, e.g. "dpi+" */
    const char *label;    /* text shown in the UI */
} BdActionPreset;

/* The built-in action choices offered in the action menu. */
const BdActionPreset *bd_action_presets(gsize *n);

/* Display text for an action spec: preset label, "KEY: CTRL + C" for key:ctrl+c, else upper-cased. */
char *bd_action_label(const char *spec);

G_END_DECLS
