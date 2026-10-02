/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* One mouse button: numbered caption + menu button choosing its action.
 * Properties: caption (shown in the circle), slot (device button slot), spec (action spec). */
#define BD_TYPE_ACTION_ROW (bd_action_row_get_type())
G_DECLARE_FINAL_TYPE(BdActionRow, bd_action_row, BD, ACTION_ROW, GtkBox)

int bd_action_row_get_slot(BdActionRow *self);
const char *bd_action_row_get_spec(BdActionRow *self);
void bd_action_row_set_spec(BdActionRow *self, const char *spec);
const char *bd_action_row_get_caption(BdActionRow *self);
GtkMenuButton *bd_action_row_get_menu_button(BdActionRow *self);

G_END_DECLS
