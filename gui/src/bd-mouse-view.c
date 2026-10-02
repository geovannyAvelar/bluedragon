/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#include "bd-mouse-view.h"

#include "bd-config.h"
#include <cairo.h>
#include <math.h>
#include <string.h>

struct _BdMouseView {
    GtkDrawingArea parent;
    cairo_surface_t *body;   /* rendered artwork, cached per pixel width */
    int body_px_w;
    gboolean body_failed;
    guint pressed;           /* BD_MARKER_* bits to highlight */
};

G_DEFINE_FINAL_TYPE(BdMouseView, bd_mouse_view, GTK_TYPE_DRAWING_AREA)

/* Everything is laid out in a fixed 230x340 space and scaled uniformly to fit the widget, so the
 * markers stay on the buttons whatever size the window is. */
#define VIEW_W 230
#define VIEW_H 340

/* Artwork: CC0 top-down mouse (gui/data/images). Its viewBox is 170.55 x 506.5; the cable takes the
 * top 37%, which is cropped away so only the body is shown. */
#define IMAGE_RESOURCE BD_RESOURCE_PREFIX "/images/mouse-top.svg"
#define SVG_ASPECT (506.5 / 170.55)
#define BODY_TOP_FRAC 0.372
#define BODY_W 150.0
#define BODY_H (BODY_W * SVG_ASPECT * (1 - BODY_TOP_FRAC))
#define BODY_X ((VIEW_W - BODY_W) / 2)
#define BODY_Y ((VIEW_H - BODY_H) / 2)

/* a point given as fractions of the mouse body */
static double bx(double f) { return BODY_X + f * BODY_W; }
static double by(double f) { return BODY_Y + f * BODY_H; }

#define YELLOW 1.0, 0.85, 0.1

static void marker(cairo_t *cr, double x, double y, const char *t, gboolean pressed) {
    if (pressed) cairo_set_source_rgb(cr, YELLOW);
    else cairo_set_source_rgb(cr, 0.08, 0.08, 0.08);
    cairo_new_sub_path(cr);
    cairo_arc(cr, x, y, 10, 0, 2 * M_PI);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.85, 0.85, 0.85);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);
    if (pressed) cairo_set_source_rgb(cr, 0.08, 0.08, 0.08);      /* dark glyph on the yellow disc */
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

/* Copy a pixbuf into a cairo ARGB32 surface (pre-multiplied alpha). */
static cairo_surface_t *surface_from_pixbuf(const GdkPixbuf *pb) {
    int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
    int n = gdk_pixbuf_get_n_channels(pb), stride_in = gdk_pixbuf_get_rowstride(pb);
    const guchar *in = gdk_pixbuf_read_pixels(pb);
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    guchar *out = cairo_image_surface_get_data(surf);
    int stride_out = cairo_image_surface_get_stride(surf);
    cairo_surface_flush(surf);
    for (int y = 0; y < h; y++) {
        const guchar *src = in + (gsize)y * stride_in;
        guint32 *dst = (guint32 *)(out + (gsize)y * stride_out);
        for (int x = 0; x < w; x++, src += n) {
            guint a = n == 4 ? src[3] : 255;
            dst[x] = (a << 24) | ((src[0] * a / 255) << 16) | ((src[1] * a / 255) << 8) | (src[2] * a / 255);
        }
    }
    cairo_surface_mark_dirty(surf);
    return surf;
}

/* Draw the SVG body, cropped and tinted blue. k = pixbuf pixels per logical unit. Returns FALSE if the
 * image could not be loaded (the SVG pixbuf loader, librsvg2-common, may be missing). */
static gboolean draw_body_image(BdMouseView *self, cairo_t *cr, double k) {
    int px_w = (int)(BODY_W * k + 0.5);
    if (self->body_failed) return FALSE;
    if (!self->body || self->body_px_w != px_w) {
        GError *error = NULL;
        g_clear_pointer(&self->body, cairo_surface_destroy);
        GdkPixbuf *pb = gdk_pixbuf_new_from_resource_at_scale(IMAGE_RESOURCE, px_w, -1, TRUE, &error);
        self->body_px_w = px_w;
        if (pb) {
            self->body = surface_from_pixbuf(pb);
            g_object_unref(pb);
        } else {
            g_warning("mouse image not available, using the drawn mouse: %s", error->message);
            g_clear_error(&error);
            self->body_failed = TRUE;
            return FALSE;
        }
    }
    double crop = BODY_TOP_FRAC * cairo_image_surface_get_height(self->body);
    double body_h = cairo_image_surface_get_height(self->body) - crop;
    cairo_save(cr);
    cairo_translate(cr, BODY_X, BODY_Y);
    cairo_scale(cr, 1 / k, 1 / k);
    cairo_rectangle(cr, 0, 0, px_w, body_h);
    cairo_clip(cr);
    cairo_push_group(cr);
    cairo_set_source_surface(cr, self->body, 0, -crop);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_ATOP);               /* tint only where the image is */
    cairo_set_source_rgba(cr, 0.1, 0.4, 0.9, 0.3);
    cairo_paint(cr);
    /* pressed buttons of the picture itself: yellow over their area (still limited to the image) */
    static const struct { guint bit; double x0, y0, x1, y1; } zones[] = {
        {BD_MARKER(1), 0.00, 0.00, 0.495, 0.385},       /* left button */
        {BD_MARKER(2), 0.505, 0.00, 1.00, 0.385},       /* right button */
        {BD_MARKER(3), 0.40, 0.10, 0.60, 0.31},         /* wheel */
    };
    for (guint i = 0; i < G_N_ELEMENTS(zones); i++) {
        if (!(self->pressed & zones[i].bit)) continue;
        cairo_rectangle(cr, zones[i].x0 * px_w, zones[i].y0 * body_h, (zones[i].x1 - zones[i].x0) * px_w,
                        (zones[i].y1 - zones[i].y0) * body_h);
        cairo_set_source_rgba(cr, YELLOW, 0.65);
        cairo_fill(cr);
    }
    cairo_pop_group_to_source(cr);
    cairo_paint(cr);
    cairo_restore(cr);
    return TRUE;
}

/* fallback: simple vector mouse */
static void draw_body_vector(cairo_t *cr) {
    double cx = VIEW_W / 2.0, h = VIEW_H;
    cairo_pattern_t *g = cairo_pattern_create_linear(0, 0, VIEW_W, 0);
    cairo_pattern_add_color_stop_rgb(g, 0, 0.10, 0.10, 0.10);
    cairo_pattern_add_color_stop_rgb(g, 0.5, 0.22, 0.22, 0.22);
    cairo_pattern_add_color_stop_rgb(g, 1, 0.10, 0.10, 0.10);
    cairo_set_source(cr, g);
    cairo_move_to(cr, cx - 62, 50);
    cairo_curve_to(cr, cx - 80, 80, cx - 85, 150, cx - 72, 225);
    cairo_curve_to(cr, cx - 64, 270, cx - 40, h - 34, cx, h - 34);
    cairo_curve_to(cr, cx + 40, h - 34, cx + 64, 270, cx + 72, 225);
    cairo_curve_to(cr, cx + 85, 150, cx + 80, 80, cx + 62, 50);
    cairo_curve_to(cr, cx + 40, 35, cx - 40, 35, cx - 62, 50);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(g);
    cairo_set_source_rgb(cr, 0.1, 0.4, 0.85);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
    cairo_set_source_rgba(cr, 0.1, 0.4, 0.85, 0.8);            /* button split + wheel */
    cairo_move_to(cr, cx, 42);
    cairo_line_to(cr, cx, 130);
    cairo_stroke(cr);
    cairo_set_source_rgb(cr, 0.1, 0.45, 0.95);
    cairo_rectangle(cr, cx - 9, 58, 18, 50);
    cairo_fill(cr);
}

static void rounded_rect(cairo_t *cr, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
    cairo_close_path(cr);
}

static void button_shape(cairo_t *cr, double cx, double cy, double w, double h, gboolean pressed) {
    rounded_rect(cr, cx - w / 2, cy - h / 2, w, h, 3);
    if (pressed) cairo_set_source_rgb(cr, YELLOW);
    else cairo_set_source_rgb(cr, 0.07, 0.10, 0.20);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.10, 0.40, 0.90);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);
}

/* The picture has no extra buttons, so draw them: two side buttons on the left edge (4, 5) and the
 * DPI+, DPI- and LED-mode buttons stacked under the wheel (6, 7, 8). Side buttons get a leader
 * line to their marker, which sits outside the body. */
static void draw_extra_buttons(BdMouseView *self, cairo_t *cr) {
    static const double side_y[2] = {0.48, 0.60};
    for (int i = 0; i < 2; i++) {
        double y = by(side_y[i]);
        button_shape(cr, bx(0.0) + 1, y, 11, 24, self->pressed & BD_MARKER(4 + i));
        cairo_set_source_rgba(cr, 0.85, 0.85, 0.85, 0.7);
        cairo_set_line_width(cr, 1);
        cairo_move_to(cr, bx(-0.19) + 10, y);
        cairo_line_to(cr, bx(0.0) - 5, y);
        cairo_stroke(cr);
    }
    static const double palm_y[3] = {0.52, 0.64, 0.76};
    for (int i = 0; i < 3; i++) button_shape(cr, bx(0.5), by(palm_y[i]), 34, 20, self->pressed & BD_MARKER(6 + i));
}

static void draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data) {
    BdMouseView *self = data;
    double scale = fmin((double)width / VIEW_W, (double)height / VIEW_H);
    cairo_translate(cr, (width - VIEW_W * scale) / 2, (height - VIEW_H * scale) / 2);
    cairo_scale(cr, scale, scale);
    if (!draw_body_image(self, cr, scale * gtk_widget_get_scale_factor(GTK_WIDGET(area)))) draw_body_vector(cr);

    draw_extra_buttons(self, cr);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    marker(cr, bx(0.27), by(0.17), "1", self->pressed & BD_MARKER(1));
    marker(cr, bx(0.73), by(0.17), "2", self->pressed & BD_MARKER(2));
    marker(cr, bx(0.50), by(0.20), "3", self->pressed & BD_MARKER(3));
    marker(cr, bx(0.50), by(0.07), "up", self->pressed & BD_MARKER_UP);
    marker(cr, bx(0.50), by(0.33), "down", self->pressed & BD_MARKER_DOWN);
    marker(cr, bx(-0.19), by(0.48), "4", self->pressed & BD_MARKER(4));
    marker(cr, bx(-0.19), by(0.60), "5", self->pressed & BD_MARKER(5));
    marker(cr, bx(0.50), by(0.52), "6", self->pressed & BD_MARKER(6));
    marker(cr, bx(0.50), by(0.64), "7", self->pressed & BD_MARKER(7));
    marker(cr, bx(0.50), by(0.76), "8", self->pressed & BD_MARKER(8));
}

static void dispose(GObject *o) {
    g_clear_pointer(&BD_MOUSE_VIEW(o)->body, cairo_surface_destroy);
    G_OBJECT_CLASS(bd_mouse_view_parent_class)->dispose(o);
}

void bd_mouse_view_set_pressed(BdMouseView *self, guint mask) {
    if (self->pressed == mask) return;
    self->pressed = mask;
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

static void bd_mouse_view_class_init(BdMouseViewClass *klass) { G_OBJECT_CLASS(klass)->dispose = dispose; }

static void bd_mouse_view_init(BdMouseView *self) {
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(self), draw, self, NULL);
}
