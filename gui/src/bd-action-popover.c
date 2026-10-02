/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#include "bd-action-popover.h"

#include "bd-actions.h"
#include "bd-config.h"
#include <bluedragon.h>

struct _BdActionPopover {
    GtkPopover parent;
    GtkBox *presets;
    GtkEntry *custom;
};

G_DEFINE_FINAL_TYPE(BdActionPopover, bd_action_popover, GTK_TYPE_POPOVER)

enum { SIGNAL_PICKED, N_SIGNALS };
static guint signals[N_SIGNALS];

static void pick(BdActionPopover *self, const char *spec) {
    g_signal_emit(self, signals[SIGNAL_PICKED], 0, spec);
    gtk_popover_popdown(GTK_POPOVER(self));
}

static void on_preset_clicked(GtkButton *button, BdActionPopover *self) {
    pick(self, g_object_get_data(G_OBJECT(button), "spec"));
}

/* template callback: custom entry activated or its button clicked */
static void on_custom(BdActionPopover *self) {
    const char *text = gtk_editable_get_text(GTK_EDITABLE(self->custom));
    uint8_t tmp[4];
    if (m711_action_parse(text, tmp)) {
        gtk_widget_add_css_class(GTK_WIDGET(self->custom), "error");
        return;
    }
    gtk_widget_remove_css_class(GTK_WIDGET(self->custom), "error");
    pick(self, text);
}

static void bd_action_popover_class_init(BdActionPopoverClass *klass) {
    GtkWidgetClass *wc = GTK_WIDGET_CLASS(klass);
    signals[SIGNAL_PICKED] = g_signal_new("picked", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0, NULL,
                                          NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);
    gtk_widget_class_set_template_from_resource(wc, BD_RESOURCE_PREFIX "/ui/action-popover.ui");
    gtk_widget_class_bind_template_child(wc, BdActionPopover, presets);
    gtk_widget_class_bind_template_child(wc, BdActionPopover, custom);
    gtk_widget_class_bind_template_callback(wc, on_custom);
}

static void bd_action_popover_init(BdActionPopover *self) {
    gtk_widget_init_template(GTK_WIDGET(self));
    gsize n;
    const BdActionPreset *p = bd_action_presets(&n);
    for (gsize i = 0; i < n; i++) {
        GtkWidget *b = gtk_button_new_with_label(p[i].label);
        gtk_button_set_has_frame(GTK_BUTTON(b), FALSE);
        gtk_widget_set_halign(gtk_button_get_child(GTK_BUTTON(b)), GTK_ALIGN_START);
        g_object_set_data(G_OBJECT(b), "spec", (gpointer)p[i].spec);
        g_signal_connect(b, "clicked", G_CALLBACK(on_preset_clicked), self);
        gtk_box_append(self->presets, b);
    }
}
