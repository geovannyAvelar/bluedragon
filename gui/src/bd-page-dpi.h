/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* DPI tab: current stage and five DPI stage columns. */
#define BD_TYPE_PAGE_DPI (bd_page_dpi_get_type())
G_DECLARE_FINAL_TYPE(BdPageDpi, bd_page_dpi, BD, PAGE_DPI, GtkBox)

G_END_DECLS
