/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Line-art mouse with numbered button markers (GtkDrawingArea subclass). */
#define BD_TYPE_MOUSE_VIEW (bd_mouse_view_get_type())
G_DECLARE_FINAL_TYPE(BdMouseView, bd_mouse_view, BD, MOUSE_VIEW, GtkDrawingArea)

/* Markers, as bits of the "pressed" mask: BD_MARKER(1)..BD_MARKER(8) are the numbered buttons. */
#define BD_MARKER(n) (1u << ((n) - 1))
#define BD_MARKER_UP (1u << 8)
#define BD_MARKER_DOWN (1u << 9)

/* Highlight (yellow) the markers whose bit is set, e.g. while the physical button is held down. */
void bd_mouse_view_set_pressed(BdMouseView *self, guint mask);

G_END_DECLS
