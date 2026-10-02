/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#include "bd-page-info.h"

#include "bd-config.h"
#include "bd-page.h"
#include <errno.h>

struct _BdPageInfo {
    GtkBox parent;
    GtkLabel *device, *active, *rate;
};

static void bd_page_iface_init(BdPageInterface *iface);
G_DEFINE_FINAL_TYPE_WITH_CODE(BdPageInfo, bd_page_info, GTK_TYPE_BOX,
                              G_IMPLEMENT_INTERFACE(BD_TYPE_PAGE, bd_page_iface_init))

static void load(BdPage *page, m711 *dev, int profile) {
    (void)profile;
    BdPageInfo *self = BD_PAGE_INFO(page);
    uint8_t act;
    if (m711_read(dev, 0x2c, &act, 1) == 0) {
        char *t = g_strdup_printf("Active profile: %d", act + 1);
        gtk_label_set_text(self->active, t);
        g_free(t);
    }
}

static void bd_page_iface_init(BdPageInterface *iface) { iface->load = load; }

/* ---- report-rate measurement, run in a worker thread ---- */

typedef struct { double us; int n, rc, err; } Measurement;

static void measure_thread(GTask *task, gpointer src, gpointer data, GCancellable *c) {
    (void)src; (void)data; (void)c;
    Measurement *m = g_new0(Measurement, 1);
    m->rc = m711_measure_rate(4.0, &m->us, &m->n);
    m->err = errno;
    g_task_return_pointer(task, m, g_free);
}

static void measure_done(GObject *src, GAsyncResult *res, gpointer data) {
    (void)src; (void)data;
    GTask *task = G_TASK(res);
    BdPageInfo *self = g_task_get_source_object(task);
    Measurement *m = g_task_propagate_pointer(task, NULL);
    char *t;
    if (m->rc) t = g_strdup_printf("No result: %s (keep moving the mouse during the test)", g_strerror(m->err));
    else t = g_strdup_printf("%d reports, median interval %.0f \xc2\xb5s = ~%.0f Hz", m->n, m->us, 1e6 / m->us);
    gtk_label_set_text(self->rate, t);
    g_free(t);
    g_free(m);
}

/* template callback */
static void on_measure(BdPageInfo *self) {
    gtk_label_set_text(self->rate, "Measuring for 4 s: move the mouse continuously...");
    GTask *task = g_task_new(self, NULL, measure_done, NULL);
    g_task_run_in_thread(task, measure_thread);
    g_object_unref(task);
}

static void bd_page_info_class_init(BdPageInfoClass *klass) {
    GtkWidgetClass *wc = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_template_from_resource(wc, BD_RESOURCE_PREFIX "/ui/page-info.ui");
    gtk_widget_class_bind_template_child(wc, BdPageInfo, device);
    gtk_widget_class_bind_template_child(wc, BdPageInfo, active);
    gtk_widget_class_bind_template_child(wc, BdPageInfo, rate);
    gtk_widget_class_bind_template_callback(wc, on_measure);
}

static void bd_page_info_init(BdPageInfo *self) {
    gtk_widget_init_template(GTK_WIDGET(self));
    char *t = g_strdup_printf("USB device %04x:%04x, config interface %d", M711_VID, M711_PID, M711_IFACE);
    gtk_label_set_text(self->device, t);
    g_free(t);
}
