/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* LIGHT tab: mode, colour, brightness and speed of the LED record. */
#define BD_TYPE_PAGE_LIGHT (bd_page_light_get_type())
G_DECLARE_FINAL_TYPE(BdPageLight, bd_page_light, BD, PAGE_LIGHT, GtkBox)

G_END_DECLS
