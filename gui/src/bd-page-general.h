/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* GENERAL tab: the ten button actions and the polling rate. */
#define BD_TYPE_PAGE_GENERAL (bd_page_general_get_type())
G_DECLARE_FINAL_TYPE(BdPageGeneral, bd_page_general, BD, PAGE_GENERAL, GtkBox)

/* Developer aid: opens the first action menu and returns its popover. */
GtkWidget *bd_page_general_popup_first(BdPageGeneral *self);

G_END_DECLS
