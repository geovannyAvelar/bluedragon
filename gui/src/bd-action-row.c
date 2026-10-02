/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#include "bd-action-row.h"

#include "bd-action-popover.h"
#include "bd-actions.h"
#include "bd-config.h"

struct _BdActionRow {
    GtkBox parent;
    GtkLabel *number;
    GtkMenuButton *button;
    char *caption;
    char *spec;
    int slot;
};

G_DEFINE_FINAL_TYPE(BdActionRow, bd_action_row, GTK_TYPE_BOX)

enum { PROP_CAPTION = 1, PROP_SLOT, PROP_SPEC, N_PROPS };
static GParamSpec *props[N_PROPS];

int bd_action_row_get_slot(BdActionRow *self) { return self->slot; }
const char *bd_action_row_get_spec(BdActionRow *self) { return self->spec; }
const char *bd_action_row_get_caption(BdActionRow *self) { return self->caption; }
GtkMenuButton *bd_action_row_get_menu_button(BdActionRow *self) { return self->button; }

void bd_action_row_set_spec(BdActionRow *self, const char *spec) {
    if (!g_strcmp0(self->spec, spec)) return;
    g_free(self->spec);
    self->spec = g_strdup(spec);
    char *label = bd_action_label(spec ? spec : "");
    gtk_menu_button_set_label(self->button, label);
    g_free(label);
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_SPEC]);
}

/* template callback: the popover's "picked" signal */
static void on_picked(BdActionRow *self, const char *spec) { bd_action_row_set_spec(self, spec); }

static void set_property(GObject *o, guint id, const GValue *v, GParamSpec *ps) {
    BdActionRow *self = BD_ACTION_ROW(o);
    switch (id) {
    case PROP_CAPTION:
        g_free(self->caption);
        self->caption = g_value_dup_string(v);
        gtk_label_set_text(self->number, self->caption);
        break;
    case PROP_SLOT: self->slot = g_value_get_int(v); break;
    case PROP_SPEC: bd_action_row_set_spec(self, g_value_get_string(v)); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID(o, id, ps);
    }
}

static void get_property(GObject *o, guint id, GValue *v, GParamSpec *ps) {
    BdActionRow *self = BD_ACTION_ROW(o);
    switch (id) {
    case PROP_CAPTION: g_value_set_string(v, self->caption); break;
    case PROP_SLOT: g_value_set_int(v, self->slot); break;
    case PROP_SPEC: g_value_set_string(v, self->spec); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID(o, id, ps);
    }
}

static void finalize(GObject *o) {
    BdActionRow *self = BD_ACTION_ROW(o);
    g_free(self->caption);
    g_free(self->spec);
    G_OBJECT_CLASS(bd_action_row_parent_class)->finalize(o);
}

static void bd_action_row_class_init(BdActionRowClass *klass) {
    GObjectClass *oc = G_OBJECT_CLASS(klass);
    GtkWidgetClass *wc = GTK_WIDGET_CLASS(klass);
    oc->set_property = set_property;
    oc->get_property = get_property;
    oc->finalize = finalize;
    props[PROP_CAPTION] = g_param_spec_string("caption", NULL, NULL, "", G_PARAM_READWRITE);
    props[PROP_SLOT] = g_param_spec_int("slot", NULL, NULL, 0, 15, 0, G_PARAM_READWRITE);
    props[PROP_SPEC] = g_param_spec_string("spec", NULL, NULL, "", G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
    g_object_class_install_properties(oc, N_PROPS, props);

    g_type_ensure(BD_TYPE_ACTION_POPOVER);
    gtk_widget_class_set_template_from_resource(wc, BD_RESOURCE_PREFIX "/ui/action-row.ui");
    gtk_widget_class_bind_template_child(wc, BdActionRow, number);
    gtk_widget_class_bind_template_child(wc, BdActionRow, button);
    gtk_widget_class_bind_template_callback(wc, on_picked);
}

static void bd_action_row_init(BdActionRow *self) { gtk_widget_init_template(GTK_WIDGET(self)); }
