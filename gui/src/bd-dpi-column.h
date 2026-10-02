/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* One DPI stage: header, link checkbox, X/Y sliders and a readout. Properties: title, dpi. */
#define BD_TYPE_DPI_COLUMN (bd_dpi_column_get_type())
G_DECLARE_FINAL_TYPE(BdDpiColumn, bd_dpi_column, BD, DPI_COLUMN, GtkBox)

int bd_dpi_column_get_dpi(BdDpiColumn *self);
void bd_dpi_column_set_dpi(BdDpiColumn *self, int dpi);

G_END_DECLS
