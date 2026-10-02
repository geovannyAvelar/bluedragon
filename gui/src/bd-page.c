/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#include "bd-page.h"

#include <errno.h>

G_DEFINE_INTERFACE(BdPage, bd_page, GTK_TYPE_WIDGET)

static void bd_page_default_init(BdPageInterface *iface) { (void)iface; }

GQuark bd_error_quark(void) { return g_quark_from_static_string("bd-error"); }

void bd_page_load(BdPage *self, m711 *dev, int profile) {
    BdPageInterface *iface = BD_PAGE_GET_IFACE(self);
    if (iface->load) iface->load(self, dev, profile);
}

gboolean bd_page_apply(BdPage *self, m711 *dev, int profile, GString *what, GError **error) {
    BdPageInterface *iface = BD_PAGE_GET_IFACE(self);
    return iface->apply ? iface->apply(self, dev, profile, what, error) : TRUE;
}

void bd_set_errno_error(GError **error, const char *what) {
    g_set_error(error, BD_ERROR, errno, "%s: %s", what, g_strerror(errno));
}
