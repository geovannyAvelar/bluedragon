/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* INFO tab: device id, active profile and a report-rate measurement. */
#define BD_TYPE_PAGE_INFO (bd_page_info_get_type())
G_DECLARE_FINAL_TYPE(BdPageInfo, bd_page_info, BD, PAGE_INFO, GtkBox)

G_END_DECLS
