/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#include "bd-page-general.h"

#include "bd-action-row.h"
#include "bd-config.h"
#include "bd-mouse-view.h"
#include "bd-page.h"
#include <errno.h>
#include <string.h>

#define N_POLL 4
static const int poll_hz[N_POLL] = {125, 250, 500, 1000};

struct _BdPageGeneral {
    GtkBox parent;
    GtkBox *rows;
    GtkCheckButton *poll[N_POLL];
};

static void bd_page_iface_init(BdPageInterface *iface);
G_DEFINE_FINAL_TYPE_WITH_CODE(BdPageGeneral, bd_page_general, GTK_TYPE_BOX,
                              G_IMPLEMENT_INTERFACE(BD_TYPE_PAGE, bd_page_iface_init))

/* calls fn for every BdActionRow in the rows box */
static BdActionRow *row_at(BdPageGeneral *self, int i) {
    GtkWidget *w = gtk_widget_get_first_child(GTK_WIDGET(self->rows));
    while (w && i--) w = gtk_widget_get_next_sibling(w);
    return BD_ACTION_ROW(w);
}

#define FOR_EACH_ROW(self, r) \
    for (GtkWidget *r_ = gtk_widget_get_first_child(GTK_WIDGET((self)->rows)); r_; r_ = gtk_widget_get_next_sibling(r_)) \
        for (BdActionRow *r = BD_ACTION_ROW(r_), *once_ = r; once_; once_ = NULL)

static void load(BdPage *page, m711 *dev, int profile) {
    BdPageGeneral *self = BD_PAGE_GENERAL(page);
    FOR_EACH_ROW(self, row) {
        uint8_t e[4];
        char name[64] = "?";
        if (m711_get_button(dev, profile, bd_action_row_get_slot(row), e) == 0) m711_action_name(e, name, sizeof name);
        bd_action_row_set_spec(row, name);
    }
    int raw = m711_get_polling_raw(dev, profile);
    for (int i = 0; i < N_POLL; i++) gtk_check_button_set_active(self->poll[i], m711_polling_hz(raw) == poll_hz[i]);
}

static gboolean apply(BdPage *page, m711 *dev, int profile, GString *what, GError **error) {
    BdPageGeneral *self = BD_PAGE_GENERAL(page);
    uint8_t want[16][4];
    int n = 0;
    FOR_EACH_ROW(self, row) {                                  /* validate everything first */
        if (m711_action_parse(bd_action_row_get_spec(row), want[n++])) {
            g_set_error(error, BD_ERROR, EINVAL, "Button %s: invalid action '%s'.", bd_action_row_get_caption(row),
                        bd_action_row_get_spec(row));
            return FALSE;
        }
    }
    n = 0;
    gboolean wrote = FALSE;
    FOR_EACH_ROW(self, row) {
        uint8_t cur[4];
        int slot = bd_action_row_get_slot(row), i = n++;
        if (m711_get_button(dev, profile, slot, cur) == 0 && !memcmp(cur, want[i], 4)) continue;
        if (m711_set_button(dev, profile, slot, want[i])) {
            bd_set_errno_error(error, "Setting button failed");
            return FALSE;
        }
        wrote = TRUE;
    }
    if (wrote) g_string_append(what, "buttons ");

    for (int i = 0; i < N_POLL; i++) {
        if (!gtk_check_button_get_active(self->poll[i])) continue;
        if (m711_polling_hz(m711_get_polling_raw(dev, profile)) == poll_hz[i]) break;
        if (m711_set_polling(dev, profile, poll_hz[i])) {
            bd_set_errno_error(error, "Setting polling rate failed");
            return FALSE;
        }
        g_string_append(what, "polling ");
        break;
    }
    return TRUE;
}

static void bd_page_iface_init(BdPageInterface *iface) {
    iface->load = load;
    iface->apply = apply;
}

GtkWidget *bd_page_general_popup_first(BdPageGeneral *self) {
    GtkMenuButton *b = bd_action_row_get_menu_button(row_at(self, 0));
    gtk_menu_button_popup(b);
    return GTK_WIDGET(gtk_menu_button_get_popover(b));
}

static void bd_page_general_class_init(BdPageGeneralClass *klass) {
    GtkWidgetClass *wc = GTK_WIDGET_CLASS(klass);
    g_type_ensure(BD_TYPE_MOUSE_VIEW);
    g_type_ensure(BD_TYPE_ACTION_ROW);
    gtk_widget_class_set_template_from_resource(wc, BD_RESOURCE_PREFIX "/ui/page-general.ui");
    gtk_widget_class_bind_template_child(wc, BdPageGeneral, rows);
    gtk_widget_class_bind_template_child_full(wc, "poll_125", FALSE, G_STRUCT_OFFSET(BdPageGeneral, poll[0]));
    gtk_widget_class_bind_template_child_full(wc, "poll_250", FALSE, G_STRUCT_OFFSET(BdPageGeneral, poll[1]));
    gtk_widget_class_bind_template_child_full(wc, "poll_500", FALSE, G_STRUCT_OFFSET(BdPageGeneral, poll[2]));
    gtk_widget_class_bind_template_child_full(wc, "poll_1000", FALSE, G_STRUCT_OFFSET(BdPageGeneral, poll[3]));
}

static void bd_page_general_init(BdPageGeneral *self) { gtk_widget_init_template(GTK_WIDGET(self)); }
