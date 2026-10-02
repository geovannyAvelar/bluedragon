/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#include "bd-page-dpi.h"

#include "bd-config.h"
#include "bd-dpi-column.h"
#include "bd-page.h"

struct _BdPageDpi {
    GtkBox parent;
    GtkLabel *stage;
    GtkBox *columns;
};

static void bd_page_iface_init(BdPageInterface *iface);
G_DEFINE_FINAL_TYPE_WITH_CODE(BdPageDpi, bd_page_dpi, GTK_TYPE_BOX,
                              G_IMPLEMENT_INTERFACE(BD_TYPE_PAGE, bd_page_iface_init))

static void load(BdPage *page, m711 *dev, int profile) {
    BdPageDpi *self = BD_PAGE_DPI(page);
    char *t = g_strdup_printf("Current Stage : %d", m711_get_dpi_stage(dev, profile) + 1);
    gtk_label_set_text(self->stage, t);
    g_free(t);
    int level = 0;
    for (GtkWidget *c = gtk_widget_get_first_child(GTK_WIDGET(self->columns)); c && level < M711_DPI_LEVELS;
         c = gtk_widget_get_next_sibling(c), level++) {
        int dpi = m711_get_dpi(dev, profile, level);
        if (dpi > 0) bd_dpi_column_set_dpi(BD_DPI_COLUMN(c), dpi);
    }
}

static gboolean apply(BdPage *page, m711 *dev, int profile, GString *what, GError **error) {
    BdPageDpi *self = BD_PAGE_DPI(page);
    int level = 0;
    gboolean wrote = FALSE;
    for (GtkWidget *c = gtk_widget_get_first_child(GTK_WIDGET(self->columns)); c && level < M711_DPI_LEVELS;
         c = gtk_widget_get_next_sibling(c), level++) {
        int dpi = bd_dpi_column_get_dpi(BD_DPI_COLUMN(c));
        if (dpi == m711_get_dpi(dev, profile, level)) continue;
        if (m711_set_dpi(dev, profile, level, dpi)) {
            bd_set_errno_error(error, "Setting DPI failed");
            return FALSE;
        }
        wrote = TRUE;
    }
    if (wrote) g_string_append(what, "DPI ");
    return TRUE;
}

static void bd_page_iface_init(BdPageInterface *iface) {
    iface->load = load;
    iface->apply = apply;
}

static void bd_page_dpi_class_init(BdPageDpiClass *klass) {
    GtkWidgetClass *wc = GTK_WIDGET_CLASS(klass);
    g_type_ensure(BD_TYPE_DPI_COLUMN);
    gtk_widget_class_set_template_from_resource(wc, BD_RESOURCE_PREFIX "/ui/page-dpi.ui");
    gtk_widget_class_bind_template_child(wc, BdPageDpi, stage);
    gtk_widget_class_bind_template_child(wc, BdPageDpi, columns);
}

static void bd_page_dpi_init(BdPageDpi *self) { gtk_widget_init_template(GTK_WIDGET(self)); }
