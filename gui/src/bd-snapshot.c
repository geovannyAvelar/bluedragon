/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#include "bd-snapshot.h"

typedef struct {
    BdWindow *win;
    GtkWidget *popover;
    char *dir;
    int step;
} Snap;

static const char *page_names[4] = {"general", "dpi", "light", "info"};

static void save_widget(GtkWidget *wd, const char *path) {
    GdkPaintable *p = gtk_widget_paintable_new(wd);
    int w = gtk_widget_get_width(wd), h = gtk_widget_get_height(wd);
    GtkSnapshot *snap = gtk_snapshot_new();
    gdk_paintable_snapshot(p, snap, w, h);
    GskRenderNode *node = gtk_snapshot_free_to_node(snap);
    GskRenderer *r = gtk_native_get_renderer(gtk_widget_get_native(wd));
    GdkTexture *tex = gsk_renderer_render_texture(r, node, &GRAPHENE_RECT_INIT(0, 0, w, h));
    gdk_texture_save_to_png(tex, path);
    g_object_unref(tex);
    gsk_render_node_unref(node);
    g_object_unref(p);
}

/* One action per timer tick, so each state has time to render:
 * ticks 0-3 save a page, tick 3 also opens the action menu, tick 5 saves the menu and
 * switches to profile 2 through the bd.profile action, tick 7 saves that. */
static gboolean step(gpointer data) {
    Snap *s = data;
    if (s->step < 4) {
        char *path = g_strdup_printf("%s/%s.png", s->dir, page_names[s->step]);
        save_widget(GTK_WIDGET(s->win), path);
        g_free(path);
        if (s->step < 3) bd_window_show_page(s->win, page_names[s->step + 1]);
        else {
            bd_window_show_page(s->win, "general");
            s->popover = bd_window_popup_first_action(s->win);
        }
    } else if (s->step == 5) {
        char *path = g_strdup_printf("%s/popover.png", s->dir);
        save_widget(s->popover, path);
        g_free(path);
        gtk_popover_popdown(GTK_POPOVER(s->popover));
        gtk_widget_activate_action(GTK_WIDGET(s->win), "bd.profile", "i", 1);   /* PROFILE2 button */
    } else if (s->step == 7) {
        char *path = g_strdup_printf("%s/profile2.png", s->dir);
        save_widget(GTK_WIDGET(s->win), path);
        g_free(path);
        g_application_quit(g_application_get_default());
        g_free(s->dir);
        g_free(s);
        return G_SOURCE_REMOVE;
    }
    s->step++;
    return G_SOURCE_CONTINUE;
}

void bd_snapshot_start(BdWindow *win, const char *dir) {
    Snap *s = g_new0(Snap, 1);
    s->win = win;
    s->dir = g_strdup(dir);
    g_timeout_add(900, step, s);
}
