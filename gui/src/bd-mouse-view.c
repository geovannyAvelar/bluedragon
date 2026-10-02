/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#include "bd-mouse-view.h"

#include <cairo.h>
#include <math.h>
#include <string.h>

struct _BdMouseView {
    GtkDrawingArea parent;
};

G_DEFINE_FINAL_TYPE(BdMouseView, bd_mouse_view, GTK_TYPE_DRAWING_AREA)

static void marker(cairo_t *cr, double x, double y, const char *t) {
    cairo_set_source_rgb(cr, 0.08, 0.08, 0.08);
    cairo_new_sub_path(cr);
    cairo_arc(cr, x, y, 10, 0, 2 * M_PI);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.85, 0.85, 0.85);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);
    if (!strcmp(t, "up") || !strcmp(t, "down")) {              /* arrows are drawn, no font dependence */
        double d = t[0] == 'u' ? -1 : 1;
        cairo_new_path(cr);
        cairo_move_to(cr, x, y + 5 * d);
        cairo_line_to(cr, x - 5, y - 4 * d);
        cairo_line_to(cr, x + 5, y - 4 * d);
        cairo_close_path(cr);
        cairo_fill(cr);
        return;
    }
    cairo_text_extents_t ex;
    cairo_set_font_size(cr, 11);
    cairo_text_extents(cr, t, &ex);
    cairo_move_to(cr, x - ex.width / 2 - ex.x_bearing, y - ex.height / 2 - ex.y_bearing);
    cairo_show_text(cr, t);
    cairo_new_path(cr);
}

static void draw(GtkDrawingArea *area, cairo_t *cr, int w, int h, gpointer data) {
    (void)area; (void)data;
    double cx = w / 2.0;
    cairo_pattern_t *g = cairo_pattern_create_linear(0, 0, w, 0);
    cairo_pattern_add_color_stop_rgb(g, 0, 0.10, 0.10, 0.10);
    cairo_pattern_add_color_stop_rgb(g, 0.5, 0.22, 0.22, 0.22);
    cairo_pattern_add_color_stop_rgb(g, 1, 0.10, 0.10, 0.10);
    cairo_set_source(cr, g);
    cairo_move_to(cr, cx - 62, 70);
    cairo_curve_to(cr, cx - 80, 100, cx - 85, 200, cx - 70, 290);
    cairo_curve_to(cr, cx - 62, 345, cx - 40, h - 20, cx, h - 20);
    cairo_curve_to(cr, cx + 40, h - 20, cx + 62, 345, cx + 70, 290);
    cairo_curve_to(cr, cx + 85, 200, cx + 80, 100, cx + 62, 70);
    cairo_curve_to(cr, cx + 40, 55, cx - 40, 55, cx - 62, 70);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
    cairo_set_source_rgb(cr, 0.1, 0.4, 0.85);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
    cairo_set_source_rgba(cr, 0.1, 0.4, 0.85, 0.8);            /* button split + wheel */
    cairo_move_to(cr, cx, 62);
    cairo_line_to(cr, cx, 150);
    cairo_stroke(cr);
    cairo_set_source_rgb(cr, 0.1, 0.45, 0.95);
    cairo_rectangle(cr, cx - 9, 78, 18, 50);
    cairo_fill(cr);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    marker(cr, cx - 35, 95, "1");
    marker(cr, cx + 35, 95, "2");
    marker(cr, cx, 103, "3");
    marker(cr, cx, 70, "up");
    marker(cr, cx, 136, "down");
    marker(cr, cx - 62, 175, "4");
    marker(cr, cx - 62, 215, "5");
    marker(cr, cx, 190, "6");
    marker(cr, cx, 222, "7");
    marker(cr, cx, 254, "8");
}

static void bd_mouse_view_class_init(BdMouseViewClass *klass) { (void)klass; }

static void bd_mouse_view_init(BdMouseView *self) {
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(self), draw, NULL, NULL);
}
