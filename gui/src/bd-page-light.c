/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#include "bd-page-light.h"

#include "bd-config.h"
#include "bd-page.h"
#include <string.h>

struct _BdPageLight {
    GtkBox parent;
    GtkColorDialogButton *color;
    m711_led led;            /* what the widgets edit */
    m711_led led_dev;        /* last value loaded from / written to the device */
};

static void bd_page_iface_init(BdPageInterface *iface);
G_DEFINE_FINAL_TYPE_WITH_CODE(BdPageLight, bd_page_light, GTK_TYPE_BOX,
                              G_IMPLEMENT_INTERFACE(BD_TYPE_PAGE, bd_page_iface_init))

/* The widgets are bound to these properties: mode/brightness/speed through property actions
 * (see page-light.ui), color through a GBinding to the colour button. */
enum { PROP_MODE = 1, PROP_BRIGHTNESS, PROP_SPEED, PROP_COLOR, N_PROPS };
static GParamSpec *props[N_PROPS];

static void set_property(GObject *o, guint id, const GValue *v, GParamSpec *ps) {
    BdPageLight *self = BD_PAGE_LIGHT(o);
    switch (id) {
    case PROP_MODE: {
        int m = g_value_get_int(v);
        if (m >= 0) m711_led_mode_to_type(m, &self->led.type, &self->led.sub);
        break;
    }
    case PROP_BRIGHTNESS: self->led.value = g_value_get_int(v); break;
    case PROP_SPEED: self->led.level = g_value_get_int(v); break;
    case PROP_COLOR: {
        const GdkRGBA *c = g_value_get_boxed(v);
        if (!c) break;
        self->led.r = (uint8_t)(c->red * 255 + 0.5);
        self->led.g = (uint8_t)(c->green * 255 + 0.5);
        self->led.b = (uint8_t)(c->blue * 255 + 0.5);
        break;
    }
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID(o, id, ps);
    }
}

static void get_property(GObject *o, guint id, GValue *v, GParamSpec *ps) {
    BdPageLight *self = BD_PAGE_LIGHT(o);
    switch (id) {
    case PROP_MODE: g_value_set_int(v, m711_led_mode_of(&self->led)); break;
    case PROP_BRIGHTNESS: g_value_set_int(v, self->led.value); break;
    case PROP_SPEED: g_value_set_int(v, self->led.level); break;
    case PROP_COLOR: {
        GdkRGBA c = {self->led.r / 255.0f, self->led.g / 255.0f, self->led.b / 255.0f, 1.0f};
        g_value_set_boxed(v, &c);
        break;
    }
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID(o, id, ps);
    }
}

static void notify_all(BdPageLight *self) {
    g_object_freeze_notify(G_OBJECT(self));
    for (int i = PROP_MODE; i < N_PROPS; i++) g_object_notify_by_pspec(G_OBJECT(self), props[i]);
    g_object_thaw_notify(G_OBJECT(self));
}

/* action light.swatch ("s"): colour given as a hex string */
static void swatch_action(GtkWidget *w, const char *name, GVariant *param) {
    (void)name;
    BdPageLight *self = BD_PAGE_LIGHT(w);
    unsigned v = (unsigned)g_ascii_strtoull(g_variant_get_string(param, NULL), NULL, 16);
    self->led.r = v >> 16;
    self->led.g = v >> 8;
    self->led.b = v;
    g_object_notify_by_pspec(G_OBJECT(self), props[PROP_COLOR]);
}

static void load(BdPage *page, m711 *dev, int profile) {
    BdPageLight *self = BD_PAGE_LIGHT(page);
    if (m711_get_led(dev, profile, &self->led) == 0) self->led_dev = self->led;
    notify_all(self);
}

static gboolean apply(BdPage *page, m711 *dev, int profile, GString *what, GError **error) {
    BdPageLight *self = BD_PAGE_LIGHT(page);
    if (!memcmp(&self->led, &self->led_dev, sizeof self->led)) return TRUE;
    if (m711_set_led(dev, profile, &self->led)) {
        bd_set_errno_error(error, "Setting light failed");
        return FALSE;
    }
    self->led_dev = self->led;
    g_string_append(what, "light ");
    return TRUE;
}

static void bd_page_iface_init(BdPageInterface *iface) {
    iface->load = load;
    iface->apply = apply;
}

static void bd_page_light_class_init(BdPageLightClass *klass) {
    GObjectClass *oc = G_OBJECT_CLASS(klass);
    GtkWidgetClass *wc = GTK_WIDGET_CLASS(klass);
    oc->set_property = set_property;
    oc->get_property = get_property;
    props[PROP_MODE] = g_param_spec_int("mode", NULL, NULL, -1, 7, -1, G_PARAM_READWRITE);
    props[PROP_BRIGHTNESS] = g_param_spec_int("brightness", NULL, NULL, 0, 255, 0, G_PARAM_READWRITE);
    props[PROP_SPEED] = g_param_spec_int("speed", NULL, NULL, 0, 255, 0, G_PARAM_READWRITE);
    props[PROP_COLOR] = g_param_spec_boxed("color", NULL, NULL, GDK_TYPE_RGBA, G_PARAM_READWRITE);
    g_object_class_install_properties(oc, N_PROPS, props);

    gtk_widget_class_install_property_action(wc, "light.mode", "mode");
    gtk_widget_class_install_property_action(wc, "light.brightness", "brightness");
    gtk_widget_class_install_property_action(wc, "light.speed", "speed");
    gtk_widget_class_install_action(wc, "light.swatch", "s", swatch_action);

    gtk_widget_class_set_template_from_resource(wc, BD_RESOURCE_PREFIX "/ui/page-light.ui");
    gtk_widget_class_bind_template_child(wc, BdPageLight, color);
}

static void bd_page_light_init(BdPageLight *self) {
    gtk_widget_init_template(GTK_WIDGET(self));
    self->led.type = 1;                                      /* defined state before the first load */
    self->led.sub = 2;
    g_object_bind_property(self, "color", self->color, "rgba", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
}
