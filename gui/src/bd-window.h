/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Main window: tab bar, pages, profile row and the RESTORE / SET ACTIVE / APPLY bar.
 * Owns the device handle and drives every BdPage through the BdPage interface. */
#define BD_TYPE_WINDOW (bd_window_get_type())
G_DECLARE_FINAL_TYPE(BdWindow, bd_window, BD, WINDOW, GtkApplicationWindow)

BdWindow *bd_window_new(GtkApplication *app);

/* Re-read every page from the device (opens the device if needed). */
void bd_window_reload(BdWindow *self);

/* Developer aids used by the snapshot mode. */
void bd_window_show_page(BdWindow *self, const char *name);
GtkWidget *bd_window_popup_first_action(BdWindow *self);
void bd_window_debug_press(BdWindow *self, guint mask);

G_END_DECLS
