/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
/* bluedragon-gui: GTK4 front end for libbluedragon. Widgets are composite templates (gui/data/ui, compiled
 * into a GResource); one C class per widget in this directory. */
#include "bd-config.h"
#include "bd-snapshot.h"
#include "bd-window.h"
#include <gtk/gtk.h>

static void load_css(void) {
    GtkCssProvider *prov = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(prov, BD_RESOURCE_PREFIX "/style.css");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(prov),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(prov);
}

static void activate(GtkApplication *app, gpointer data) {
    (void)data;
    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", TRUE, NULL);
    load_css();
    BdWindow *win = bd_window_new(app);
    gtk_window_present(GTK_WINDOW(win));
    const char *snap = g_getenv("M711_GUI_SNAPSHOT_DIR");
    if (snap) bd_snapshot_start(win, snap);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("org.bluedragon.settings", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int rc = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return rc;
}
