/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#include "bd-dpi-column.h"

#include "bd-config.h"

struct _BdDpiColumn {
    GtkBox parent;
    GtkLabel *head, *readout;
    GtkAdjustment *adjustment;
    char *title;
};

G_DEFINE_FINAL_TYPE(BdDpiColumn, bd_dpi_column, GTK_TYPE_BOX)

enum { PROP_TITLE = 1, PROP_DPI, N_PROPS };
static GParamSpec *props[N_PROPS];

int bd_dpi_column_get_dpi(BdDpiColumn *self) {
    return (int)(gtk_adjustment_get_value(self->adjustment) / 100 + 0.5) * 100;
}

static void update_readout(BdDpiColumn *self) {
    int dpi = bd_dpi_column_get_dpi(self);
    char *t = g_strdup_printf("X %d   Y %d", dpi, dpi);
    gtk_label_set_text(self->readout, t);
    g_free(t);
}

void bd_dpi_column_set_dpi(BdDpiColumn *self, int dpi) {
    gtk_adjustment_set_value(self->adjustment, dpi);
    update_readout(self);
}

/* template callback: adjustment "value-changed" */
static void on_value_changed(BdDpiColumn *self) {
    update_readout(self);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_DPI]);
}

static void set_property(GObject *o, guint id, const GValue *v, GParamSpec *ps) {
    BdDpiColumn *self = BD_DPI_COLUMN(o);
    switch (id) {
    case PROP_TITLE:
        g_free(self->title);
        self->title = g_value_dup_string(v);
        gtk_label_set_text(self->head, self->title);
        break;
    case PROP_DPI: bd_dpi_column_set_dpi(self, g_value_get_int(v)); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID(o, id, ps);
    }
}

static void get_property(GObject *o, guint id, GValue *v, GParamSpec *ps) {
    BdDpiColumn *self = BD_DPI_COLUMN(o);
    switch (id) {
    case PROP_TITLE: g_value_set_string(v, self->title); break;
    case PROP_DPI: g_value_set_int(v, bd_dpi_column_get_dpi(self)); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID(o, id, ps);
    }
}

static void finalize(GObject *o) {
    g_free(BD_DPI_COLUMN(o)->title);
    G_OBJECT_CLASS(bd_dpi_column_parent_class)->finalize(o);
}

static void bd_dpi_column_class_init(BdDpiColumnClass *klass) {
    GObjectClass *oc = G_OBJECT_CLASS(klass);
    GtkWidgetClass *wc = GTK_WIDGET_CLASS(klass);
    oc->set_property = set_property;
    oc->get_property = get_property;
    oc->finalize = finalize;
    props[PROP_TITLE] = g_param_spec_string("title", NULL, NULL, "", G_PARAM_READWRITE);
    props[PROP_DPI] = g_param_spec_int("dpi", NULL, NULL, 100, 5000, 100, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
    g_object_class_install_properties(oc, N_PROPS, props);

    gtk_widget_class_set_template_from_resource(wc, BD_RESOURCE_PREFIX "/ui/dpi-column.ui");
    gtk_widget_class_bind_template_child(wc, BdDpiColumn, head);
    gtk_widget_class_bind_template_child(wc, BdDpiColumn, readout);
    gtk_widget_class_bind_template_child(wc, BdDpiColumn, adjustment);
    gtk_widget_class_bind_template_callback(wc, on_value_changed);
}

static void bd_dpi_column_init(BdDpiColumn *self) {
    gtk_widget_init_template(GTK_WIDGET(self));
    update_readout(self);
}
