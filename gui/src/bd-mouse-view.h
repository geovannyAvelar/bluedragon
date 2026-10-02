/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Line-art mouse with numbered button markers (GtkDrawingArea subclass). */
#define BD_TYPE_MOUSE_VIEW (bd_mouse_view_get_type())
G_DECLARE_FINAL_TYPE(BdMouseView, bd_mouse_view, BD, MOUSE_VIEW, GtkDrawingArea)

G_END_DECLS
