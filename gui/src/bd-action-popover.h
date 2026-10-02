/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Menu of action presets plus a custom-action entry. Emits "picked" (const char *spec). */
#define BD_TYPE_ACTION_POPOVER (bd_action_popover_get_type())
G_DECLARE_FINAL_TYPE(BdActionPopover, bd_action_popover, BD, ACTION_POPOVER, GtkPopover)

G_END_DECLS
