/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#include "bd-window.h"

#include "bd-config.h"
#include "bd-page-dpi.h"
#include "bd-page-general.h"
#include "bd-page-info.h"
#include "bd-page-light.h"
#include "bd-page.h"
#include <bluedragon.h>
#include <errno.h>

struct _BdWindow {
    GtkApplicationWindow parent;
    GtkStack *stack;
    GtkLabel *status;
    GtkWidget *page_general, *page_dpi, *page_light, *page_info;
    m711 *dev;
    int profile;             /* 0-based profile being edited */
    char *page;              /* visible stack page name */
};

G_DEFINE_FINAL_TYPE(BdWindow, bd_window, GTK_TYPE_APPLICATION_WINDOW)

enum { PROP_PROFILE = 1, PROP_PAGE, N_PROPS };
static GParamSpec *props[N_PROPS];

G_GNUC_PRINTF(2, 3)
static void say(BdWindow *self, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *s = g_strdup_vprintf(fmt, ap);
    va_end(ap);
    gtk_label_set_text(self->status, s);
    g_free(s);
}

static GtkWidget **pages(BdWindow *self, GtkWidget *out[4]) {
    out[0] = self->page_general;
    out[1] = self->page_dpi;
    out[2] = self->page_light;
    out[3] = self->page_info;
    return out;
}

static gboolean ensure_device(BdWindow *self) {
    if (self->dev) return TRUE;
    self->dev = m711_open(NULL);
    if (!self->dev) say(self, "No device (%s). Plug the mouse in, check hidraw permissions, press RESTORE.", g_strerror(errno));
    return self->dev != NULL;
}

void bd_window_reload(BdWindow *self) {
    if (!ensure_device(self)) return;
    GtkWidget *p[4];
    pages(self, p);
    for (int i = 0; i < 4; i++) bd_page_load(BD_PAGE(p[i]), self->dev, self->profile);
    say(self, "Loaded profile %d.", self->profile + 1);
}

/* ---- template callbacks (buttons in window.ui) ---- */

static void bd_window_on_restore(BdWindow *self) { bd_window_reload(self); }

static void bd_window_on_set_active(BdWindow *self) {
    if (!ensure_device(self)) return;
    if (m711_set_profile(self->dev, self->profile)) {
        say(self, "Set active failed: %s", g_strerror(errno));
        return;
    }
    bd_window_reload(self);
    say(self, "Profile %d is now the active profile.", self->profile + 1);
}

static void bd_window_on_apply(BdWindow *self) {
    if (!ensure_device(self)) return;
    GString *what = g_string_new("");
    GError *error = NULL;
    GtkWidget *p[4];
    pages(self, p);
    gboolean ok = TRUE;
    for (int i = 0; i < 4 && ok; i++) ok = bd_page_apply(BD_PAGE(p[i]), self->dev, self->profile, what, &error);
    if (!ok) {
        say(self, "%s", error->message);
        g_clear_error(&error);
    } else if (!what->len) {
        say(self, "Nothing changed.");
    } else {
        char *done = g_strdup_printf("Applied to profile %d: %s", self->profile + 1, g_strstrip(what->str));
        bd_window_reload(self);
        say(self, "%s", done);
        g_free(done);
    }
    g_string_free(what, TRUE);
}

/* ---- properties (driven by the bd.page / bd.profile property actions) ---- */

static void set_property(GObject *o, guint id, const GValue *v, GParamSpec *ps) {
    BdWindow *self = BD_WINDOW(o);
    switch (id) {
    case PROP_PROFILE:
        self->profile = g_value_get_int(v);
        break;
    case PROP_PAGE:
        g_free(self->page);
        self->page = g_value_dup_string(v);
        break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID(o, id, ps);
    }
}

static void get_property(GObject *o, guint id, GValue *v, GParamSpec *ps) {
    BdWindow *self = BD_WINDOW(o);
    switch (id) {
    case PROP_PROFILE: g_value_set_int(v, self->profile); break;
    case PROP_PAGE: g_value_set_string(v, self->page); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID(o, id, ps);
    }
}

static void on_profile_changed(BdWindow *self) { bd_window_reload(self); }

static void finalize(GObject *o) {
    BdWindow *self = BD_WINDOW(o);
    m711_close(self->dev);
    g_free(self->page);
    G_OBJECT_CLASS(bd_window_parent_class)->finalize(o);
}

static void bd_window_class_init(BdWindowClass *klass) {
    GObjectClass *oc = G_OBJECT_CLASS(klass);
    GtkWidgetClass *wc = GTK_WIDGET_CLASS(klass);
    oc->set_property = set_property;
    oc->get_property = get_property;
    oc->finalize = finalize;
    props[PROP_PROFILE] = g_param_spec_int("profile", NULL, NULL, 0, M711_PROFILES - 1, 0, G_PARAM_READWRITE);
    props[PROP_PAGE] = g_param_spec_string("page", NULL, NULL, "general", G_PARAM_READWRITE);
    g_object_class_install_properties(oc, N_PROPS, props);
    gtk_widget_class_install_property_action(wc, "bd.profile", "profile");
    gtk_widget_class_install_property_action(wc, "bd.page", "page");

    g_type_ensure(BD_TYPE_PAGE_GENERAL);
    g_type_ensure(BD_TYPE_PAGE_DPI);
    g_type_ensure(BD_TYPE_PAGE_LIGHT);
    g_type_ensure(BD_TYPE_PAGE_INFO);
    gtk_widget_class_set_template_from_resource(wc, BD_RESOURCE_PREFIX "/ui/window.ui");
    gtk_widget_class_bind_template_child(wc, BdWindow, stack);
    gtk_widget_class_bind_template_child(wc, BdWindow, status);
    gtk_widget_class_bind_template_child(wc, BdWindow, page_general);
    gtk_widget_class_bind_template_child(wc, BdWindow, page_dpi);
    gtk_widget_class_bind_template_child(wc, BdWindow, page_light);
    gtk_widget_class_bind_template_child(wc, BdWindow, page_info);
    gtk_widget_class_bind_template_callback(wc, bd_window_on_restore);
    gtk_widget_class_bind_template_callback(wc, bd_window_on_set_active);
    gtk_widget_class_bind_template_callback(wc, bd_window_on_apply);
}

static void bd_window_init(BdWindow *self) {
    self->page = g_strdup("general");
    gtk_widget_init_template(GTK_WIDGET(self));
    g_object_bind_property(self, "page", self->stack, "visible-child-name", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
    g_signal_connect(self, "notify::profile", G_CALLBACK(on_profile_changed), NULL);
}

BdWindow *bd_window_new(GtkApplication *app) {
    BdWindow *w = g_object_new(BD_TYPE_WINDOW, "application", app, NULL);
    bd_window_reload(w);
    return w;
}

void bd_window_show_page(BdWindow *self, const char *name) { g_object_set(self, "page", name, NULL); }

GtkWidget *bd_window_popup_first_action(BdWindow *self) {
    return bd_page_general_popup_first(BD_PAGE_GENERAL(self->page_general));
}

void bd_window_debug_press(BdWindow *self, guint mask) {
    bd_page_general_debug_press(BD_PAGE_GENERAL(self->page_general), mask);
}
