/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#pragma once

#include <bluedragon.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BD_ERROR (bd_error_quark())
GQuark bd_error_quark(void);

/* A settings page: loads its widgets from the device and writes edits back. */
#define BD_TYPE_PAGE (bd_page_get_type())
G_DECLARE_INTERFACE(BdPage, bd_page, BD, PAGE, GtkWidget)

struct _BdPageInterface {
    GTypeInterface parent_iface;

    /* Fill the widgets from the device (profile is 0-based). */
    void (*load)(BdPage *self, m711 *dev, int profile);
    /* Write changed values; append a word per written group to `what`. FALSE + error on failure. */
    gboolean (*apply)(BdPage *self, m711 *dev, int profile, GString *what, GError **error);
};

void bd_page_load(BdPage *self, m711 *dev, int profile);
gboolean bd_page_apply(BdPage *self, m711 *dev, int profile, GString *what, GError **error);

/* Sets `error` to "<what>: <strerror(errno)>". */
void bd_set_errno_error(GError **error, const char *what);

G_END_DECLS
