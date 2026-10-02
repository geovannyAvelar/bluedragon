/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
/* m711-gui: GTK4 front end for libm711. Layout follows the vendor tool (GENERAL / DPI / LIGHT /
 * INFO tabs, profile row, APPLY bar) but uses original artwork. Device calls are synchronous
 * (each takes < 0.5 s); the report-rate measurement runs in a worker thread. */
#include <errno.h>
#include <gtk/gtk.h>
#include <m711.h>
#include <math.h>
#include <string.h>

#define N_ROWS 10
#define N_MODES 8
#define N_SWATCHES 7

/* GENERAL rows: UI row -> device button slot (hid.exe edits slots 0-7, 10, 11). */
static const int row_slot[N_ROWS] = {0, 1, 2, 3, 4, 5, 6, 7, 10, 11};
static const char *row_caption[N_ROWS] = {"1", "2", "3", "4", "5", "6", "7", "8", "\xe2\x96\xb2", "\xe2\x96\xbc"};
static const int poll_hz[] = {125, 250, 500, 1000};

/* name shown for an action spec; unknown specs are shown upper-cased */
static const struct { const char *spec, *label; } friendly[] = {
    {"left", "LEFT CLICK"},       {"right", "RIGHT CLICK"},  {"middle", "MIDDLE BUTTON"},
    {"button5", "FORWARD"},       {"button4", "BACKWARD"},   {"dpi+", "DPI +"},
    {"dpi-", "DPI -"},            {"ledmode", "LED MODE SWITCH"}, {"scrollup", "SCROLL UP"},
    {"scrolldown", "SCROLL DOWN"}, {"playpause", "PLAY / PAUSE"}, {"next", "NEXT TRACK"},
    {"prev", "PREVIOUS TRACK"},   {"stop", "STOP"},          {"volup", "VOLUME +"},
    {"voldown", "VOLUME -"},      {"mute", "MUTE"},          {"none", "DISABLED"},
};

/* LED mode names: 2 = Breathing and 7 = Off are confirmed; the rest are not yet identified. */
static const char *mode_name[N_MODES] = {"MODE 0", "MODE 1", "BREATHING", "MODE 3", "MODE 4", "MODE 5", "MODE 6", "OFF"};
static const char *swatch_rgb[N_SWATCHES] = {"ff0000", "00ff00", "0000ff", "00ffff", "ffff00", "aa00cc", "ffffff"};

typedef struct App App;
typedef struct { App *a; int row; } RowCtx;

struct App {
    m711 *dev;
    GtkWidget *win, *stack;
    GtkToggleButton *tab[4], *prof[M711_PROFILES];
    GtkLabel *status, *stage, *info_active, *rate;
    /* general */
    GtkMenuButton *row_btn[N_ROWS];
    char spec[N_ROWS][64];
    RowCtx ctx[N_ROWS];
    GtkWidget *mouse;
    GtkCheckButton *poll[4];
    /* dpi */
    GtkRange *dpi_x[M711_DPI_LEVELS], *dpi_y[M711_DPI_LEVELS];
    GtkLabel *dpi_lbl[M711_DPI_LEVELS];
    /* light */
    GtkToggleButton *mode_btn[N_MODES];
    GtkCheckButton *bright[3], *speed[3];
    GtkColorDialogButton *color;
    m711_led led, led_dev;
    gboolean loading;
};

/* ---- helpers ---- */

static void say(App *a, const char *fmt, ...) G_GNUC_PRINTF(2, 3);
static void say(App *a, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *s = g_strdup_vprintf(fmt, ap);
    va_end(ap);
    gtk_label_set_text(a->status, s);
    g_free(s);
}

static int cur_profile(App *a) {
    for (int i = 0; i < M711_PROFILES; i++)
        if (gtk_toggle_button_get_active(a->prof[i])) return i;
    return 0;
}

static gboolean need_dev(App *a) {
    if (a->dev) return TRUE;
    say(a, "No device. Plug the mouse in (check hidraw permissions) and press RESTORE.");
    return FALSE;
}

static char *label_for(const char *spec) {
    for (guint i = 0; i < G_N_ELEMENTS(friendly); i++)
        if (!strcmp(spec, friendly[i].spec)) return g_strdup(friendly[i].label);
    char *u = g_ascii_strup(spec, -1);
    if (g_str_has_prefix(u, "KEY:")) {                           /* KEY:CTRL+C -> KEY: CTRL + C */
        char **parts = g_strsplit(u + 4, "+", -1);
        char *j = g_strjoinv(" + ", parts);
        char *r = g_strconcat("KEY: ", j, NULL);
        g_strfreev(parts);
        g_free(j);
        g_free(u);
        return r;
    }
    return u;
}

static void set_row(App *a, int row, const char *spec) {
    g_strlcpy(a->spec[row], spec, sizeof a->spec[row]);
    char *l = label_for(spec);
    gtk_menu_button_set_label(a->row_btn[row], l);
    g_free(l);
    gtk_widget_queue_draw(a->mouse);
}

static void set_color_widget(App *a) {
    GdkRGBA c = {a->led.r / 255.0f, a->led.g / 255.0f, a->led.b / 255.0f, 1.0f};
    gtk_color_dialog_button_set_rgba(a->color, &c);
}

static void sync_led_widgets(App *a) {
    int mode = m711_led_mode_of(&a->led);
    for (int i = 0; i < N_MODES; i++) gtk_toggle_button_set_active(a->mode_btn[i], i == mode);
    for (int i = 0; i < 3; i++) {
        gtk_check_button_set_active(a->bright[i], a->led.value == i + 1);
        gtk_check_button_set_active(a->speed[i], a->led.level == 3 - i);
    }
    set_color_widget(a);
}

/* ---- device -> widgets ---- */

static void refresh(App *a) {
    if (!a->dev && !(a->dev = m711_open(NULL))) {
        say(a, "Cannot open device: %s", g_strerror(errno));
        return;
    }
    int p = cur_profile(a);
    uint8_t act = 0;
    a->loading = TRUE;
    if (m711_read(a->dev, 0x2c, &act, 1) == 0) {
        char *t = g_strdup_printf("Active profile: %d", act + 1);
        gtk_label_set_text(a->info_active, t);
        g_free(t);
    }
    int stage = m711_get_dpi_stage(a->dev, p);
    char *st = g_strdup_printf("Current Stage : %d", stage + 1);
    gtk_label_set_text(a->stage, st);
    g_free(st);
    for (int l = 0; l < M711_DPI_LEVELS; l++) {
        int dpi = m711_get_dpi(a->dev, p, l);
        if (dpi > 0) {
            gtk_range_set_value(a->dpi_x[l], dpi);
            gtk_range_set_value(a->dpi_y[l], dpi);
            char *t = g_strdup_printf("X %d   Y %d", dpi, dpi);
            gtk_label_set_text(a->dpi_lbl[l], t);
            g_free(t);
        }
    }
    for (int r = 0; r < N_ROWS; r++) {
        uint8_t e[4];
        char name[64] = "?";
        if (m711_get_button(a->dev, p, row_slot[r], e) == 0) m711_action_name(e, name, sizeof name);
        set_row(a, r, name);
    }
    if (m711_get_led(a->dev, p, &a->led) == 0) {
        a->led_dev = a->led;
        sync_led_widgets(a);
    }
    int raw = m711_get_polling_raw(a->dev, p);
    for (int i = 0; i < 4; i++) gtk_check_button_set_active(a->poll[i], m711_polling_hz(raw) == poll_hz[i]);
    a->loading = FALSE;
    say(a, "Loaded profile %d.", p + 1);
}

/* ---- widgets -> device ---- */

static void on_apply(GtkButton *w, App *a) {
    (void)w;
    if (!need_dev(a)) return;
    int p = cur_profile(a), rc = 0;
    GString *what = g_string_new("");

    for (int l = 0; l < M711_DPI_LEVELS && !rc; l++) {
        int dpi = (int)(gtk_range_get_value(a->dpi_x[l]) / 100 + 0.5) * 100;
        if (dpi != m711_get_dpi(a->dev, p, l)) {
            rc = m711_set_dpi(a->dev, p, l, dpi);
            if (!rc && !strstr(what->str, "DPI")) g_string_append(what, "DPI ");
        }
    }
    uint8_t want[N_ROWS][4];
    for (int r = 0; r < N_ROWS; r++) {
        if (m711_action_parse(a->spec[r], want[r])) {
            say(a, "Button %s: invalid action '%s'.", row_caption[r], a->spec[r]);
            g_string_free(what, TRUE);
            return;
        }
    }
    for (int r = 0; r < N_ROWS && !rc; r++) {
        uint8_t cur[4];
        if (m711_get_button(a->dev, p, row_slot[r], cur) == 0 && !memcmp(cur, want[r], 4)) continue;
        rc = m711_set_button(a->dev, p, row_slot[r], want[r]);
        if (!rc && !strstr(what->str, "buttons")) g_string_append(what, "buttons ");
    }
    if (!rc && memcmp(&a->led, &a->led_dev, sizeof a->led)) {
        rc = m711_set_led(a->dev, p, &a->led);
        if (!rc) g_string_append(what, "light ");
    }
    for (int i = 0; i < 4 && !rc; i++) {
        if (!gtk_check_button_get_active(a->poll[i])) continue;
        if (m711_polling_hz(m711_get_polling_raw(a->dev, p)) != poll_hz[i]) {
            rc = m711_set_polling(a->dev, p, poll_hz[i]);
            if (!rc) g_string_append(what, "polling ");
        }
    }
    if (rc) say(a, "Apply failed: %s", g_strerror(errno));
    else if (!what->len) say(a, "Nothing changed.");
    else say(a, "Applied to profile %d: %s", p + 1, g_strstrip(what->str));
    g_string_free(what, TRUE);
    if (!rc) refresh(a);
}

static void on_set_active(GtkButton *w, App *a) {
    (void)w;
    if (!need_dev(a)) return;
    if (m711_set_profile(a->dev, cur_profile(a)) == 0) {
        refresh(a);
        say(a, "Profile %d is now the active profile.", cur_profile(a) + 1);
    } else say(a, "Set active failed: %s", g_strerror(errno));
}

static void on_restore(GtkButton *w, App *a) { (void)w; refresh(a); }

static void on_profile(GtkToggleButton *b, App *a) {
    if (gtk_toggle_button_get_active(b)) refresh(a);
}

static void on_tab(GtkToggleButton *b, App *a) {
    if (gtk_toggle_button_get_active(b))
        gtk_stack_set_visible_child_name(GTK_STACK(a->stack), g_object_get_data(G_OBJECT(b), "page"));
}

/* ---- GENERAL ---- */

static void on_pick(GtkButton *b, RowCtx *c) {
    set_row(c->a, c->row, g_object_get_data(G_OBJECT(b), "spec"));
    gtk_popover_popdown(GTK_POPOVER(gtk_widget_get_ancestor(GTK_WIDGET(b), GTK_TYPE_POPOVER)));
}

static void on_custom(GtkWidget *w, RowCtx *c) {
    GtkEntry *e = GTK_ENTRY(g_object_get_data(G_OBJECT(w), "entry"));
    const char *t = gtk_editable_get_text(GTK_EDITABLE(e));
    uint8_t tmp[4];
    if (m711_action_parse(t, tmp)) {
        gtk_widget_add_css_class(GTK_WIDGET(e), "error");
        return;
    }
    gtk_widget_remove_css_class(GTK_WIDGET(e), "error");
    set_row(c->a, c->row, t);
    gtk_popover_popdown(GTK_POPOVER(gtk_widget_get_ancestor(w, GTK_TYPE_POPOVER)));
}

static GtkWidget *build_popover(RowCtx *c) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    for (guint i = 0; i < G_N_ELEMENTS(friendly); i++) {
        GtkWidget *b = gtk_button_new_with_label(friendly[i].label);
        gtk_widget_add_css_class(b, "flat");
        gtk_button_set_has_frame(GTK_BUTTON(b), FALSE);
        g_object_set_data(G_OBJECT(b), "spec", (gpointer)friendly[i].spec);
        g_signal_connect(b, "clicked", G_CALLBACK(on_pick), c);
        gtk_box_append(GTK_BOX(box), b);
    }
    GtkWidget *e = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(e), "key:ctrl+c   key:f5   raw:8d000000");
    GtkWidget *set = gtk_button_new_with_label("Use custom action");
    g_object_set_data(G_OBJECT(set), "entry", e);
    g_signal_connect(set, "clicked", G_CALLBACK(on_custom), c);
    g_signal_connect_swapped(e, "activate", G_CALLBACK(gtk_widget_activate), set);
    gtk_box_append(GTK_BOX(box), e);
    gtk_box_append(GTK_BOX(box), set);
    GtkWidget *sw = gtk_scrolled_window_new();
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(sw), TRUE);
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(sw), 360);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), box);
    GtkWidget *pop = gtk_popover_new();
    gtk_popover_set_child(GTK_POPOVER(pop), sw);
    return pop;
}

/* original line-art mouse with numbered button markers */
static void marker(cairo_t *cr, double x, double y, const char *t) {
    cairo_set_source_rgb(cr, 0.08, 0.08, 0.08);
    cairo_new_sub_path(cr);
    cairo_arc(cr, x, y, 10, 0, 2 * M_PI);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.85, 0.85, 0.85);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);
    if (!strcmp(t, "up") || !strcmp(t, "down")) {              /* arrows drawn, no font dependence */
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

static void draw_mouse(GtkDrawingArea *area, cairo_t *cr, int w, int h, gpointer data) {
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
    cairo_set_source_rgb(cr, 0.7, 0.1, 0.1);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
    cairo_set_source_rgba(cr, 0.7, 0.1, 0.1, 0.8);                    /* button split + wheel */
    cairo_move_to(cr, cx, 62); cairo_line_to(cr, cx, 150); cairo_stroke(cr);
    cairo_set_source_rgb(cr, 0.75, 0.1, 0.1);
    cairo_rectangle(cr, cx - 9, 78, 18, 50); cairo_fill(cr);
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

static GtkWidget *build_general(App *a) {
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 24);
    gtk_widget_add_css_class(root, "page");

    a->mouse = gtk_drawing_area_new();
    gtk_widget_set_size_request(a->mouse, 230, 380);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(a->mouse), draw_mouse, a, NULL);
    gtk_box_append(GTK_BOX(root), a->mouse);

    GtkWidget *col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    for (int r = 0; r < N_ROWS; r++) {
        GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget *n = gtk_label_new(row_caption[r]);
        gtk_widget_add_css_class(n, "num");
        a->row_btn[r] = GTK_MENU_BUTTON(gtk_menu_button_new());
        gtk_widget_add_css_class(GTK_WIDGET(a->row_btn[r]), "action");
        gtk_widget_set_hexpand(GTK_WIDGET(a->row_btn[r]), TRUE);
        gtk_widget_set_size_request(GTK_WIDGET(a->row_btn[r]), 220, -1);
        a->ctx[r] = (RowCtx){a, r};
        gtk_menu_button_set_popover(a->row_btn[r], build_popover(&a->ctx[r]));
        gtk_box_append(GTK_BOX(h), n);
        gtk_box_append(GTK_BOX(h), GTK_WIDGET(a->row_btn[r]));
        gtk_box_append(GTK_BOX(col), h);
    }
    gtk_box_append(GTK_BOX(root), col);

    GtkWidget *right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_valign(right, GTK_ALIGN_START);
    GtkWidget *t = gtk_label_new("POLLING RATE");
    gtk_widget_add_css_class(t, "section");
    gtk_label_set_xalign(GTK_LABEL(t), 0);
    gtk_box_append(GTK_BOX(right), t);
    for (int i = 0; i < 4; i++) {
        char l[16];
        snprintf(l, sizeof l, "%d HZ", poll_hz[i]);
        a->poll[i] = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(l));
        if (i) gtk_check_button_set_group(a->poll[i], a->poll[0]);
        gtk_box_append(GTK_BOX(right), GTK_WIDGET(a->poll[i]));
    }
    GtkWidget *n = gtk_label_new("Click an action to change it, then press APPLY.\n250/125 Hz are unverified.");
    gtk_widget_add_css_class(n, "hint");
    gtk_label_set_wrap(GTK_LABEL(n), TRUE);
    gtk_label_set_xalign(GTK_LABEL(n), 0);
    gtk_widget_set_size_request(n, 200, -1);
    gtk_box_append(GTK_BOX(right), n);
    gtk_box_append(GTK_BOX(root), right);
    return root;
}

/* ---- DPI ---- */

typedef struct { App *a; int level; } DpiCtx;
static DpiCtx dpi_ctx[M711_DPI_LEVELS];

static void on_dpi_changed(GtkRange *r, DpiCtx *c) {
    App *a = c->a;
    int dpi = (int)(gtk_range_get_value(r) / 100 + 0.5) * 100;
    if (a->loading) return;
    gtk_range_set_value(a->dpi_y[c->level], dpi);
    char *t = g_strdup_printf("X %d   Y %d", dpi, dpi);
    gtk_label_set_text(a->dpi_lbl[c->level], t);
    g_free(t);
}

static GtkWidget *build_dpi(App *a) {
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(root, "page");
    a->stage = GTK_LABEL(gtk_label_new("Current Stage : ?"));
    gtk_widget_add_css_class(GTK_WIDGET(a->stage), "section");
    gtk_label_set_xalign(a->stage, 0);
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(a->stage));

    GtkWidget *cols = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_set_homogeneous(GTK_BOX(cols), TRUE);
    gtk_widget_set_vexpand(cols, TRUE);
    for (int l = 0; l < M711_DPI_LEVELS; l++) {
        char t[8];
        GtkWidget *col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        snprintf(t, sizeof t, "DPI%d", l + 1);
        GtkWidget *head = gtk_label_new(t);
        gtk_widget_add_css_class(head, "dpihead");
        GtkWidget *link = gtk_check_button_new_with_label("LINK XY");
        gtk_check_button_set_active(GTK_CHECK_BUTTON(link), TRUE);
        gtk_widget_set_sensitive(link, FALSE);
        gtk_widget_set_tooltip_text(link, "Separate X/Y DPI is not supported yet");
        GtkWidget *sl = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
        gtk_widget_set_halign(sl, GTK_ALIGN_CENTER);
        gtk_widget_set_vexpand(sl, TRUE);
        for (int axis = 0; axis < 2; axis++) {
            GtkRange *r = GTK_RANGE(gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, 100, 5000, 100));
            gtk_range_set_inverted(r, TRUE);
            gtk_scale_set_draw_value(GTK_SCALE(r), FALSE);
            gtk_widget_set_size_request(GTK_WIDGET(r), -1, 250);
            if (axis) { a->dpi_y[l] = r; gtk_widget_set_sensitive(GTK_WIDGET(r), FALSE); }
            else {
                a->dpi_x[l] = r;
                dpi_ctx[l] = (DpiCtx){a, l};
                g_signal_connect(r, "value-changed", G_CALLBACK(on_dpi_changed), &dpi_ctx[l]);
            }
            gtk_box_append(GTK_BOX(sl), GTK_WIDGET(r));
        }
        a->dpi_lbl[l] = GTK_LABEL(gtk_label_new("X -   Y -"));
        gtk_box_append(GTK_BOX(col), head);
        gtk_box_append(GTK_BOX(col), link);
        gtk_box_append(GTK_BOX(col), sl);
        gtk_box_append(GTK_BOX(col), GTK_WIDGET(a->dpi_lbl[l]));
        gtk_box_append(GTK_BOX(cols), col);
    }
    gtk_box_append(GTK_BOX(root), cols);
    return root;
}

/* ---- LIGHT ---- */

static void on_mode(GtkToggleButton *b, App *a) {
    if (a->loading || !gtk_toggle_button_get_active(b)) return;
    int m = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "mode"));
    m711_led_mode_to_type(m, &a->led.type, &a->led.sub);
}

static void on_bright(GtkCheckButton *b, App *a) {
    if (a->loading || !gtk_check_button_get_active(b)) return;
    a->led.value = 1 + GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "idx"));
}

static void on_speed(GtkCheckButton *b, App *a) {
    if (a->loading || !gtk_check_button_get_active(b)) return;
    a->led.level = 3 - GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "idx"));
}

static void on_swatch(GtkButton *b, App *a) {
    unsigned v = (unsigned)strtoul(g_object_get_data(G_OBJECT(b), "rgb"), NULL, 16);
    a->led.r = v >> 16; a->led.g = v >> 8; a->led.b = v;
    a->loading = TRUE;
    set_color_widget(a);
    a->loading = FALSE;
}

static void on_color(GObject *o, GParamSpec *ps, App *a) {
    (void)o; (void)ps;
    if (a->loading) return;
    const GdkRGBA *c = gtk_color_dialog_button_get_rgba(a->color);
    a->led.r = (uint8_t)(c->red * 255 + 0.5);
    a->led.g = (uint8_t)(c->green * 255 + 0.5);
    a->led.b = (uint8_t)(c->blue * 255 + 0.5);
}

static GtkWidget *radio_column(App *a, const char *title, const char *const *labels, GtkCheckButton **out, GCallback cb) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *t = gtk_label_new(title);
    gtk_widget_add_css_class(t, "section");
    gtk_label_set_xalign(GTK_LABEL(t), 0);
    gtk_box_append(GTK_BOX(box), t);
    for (int i = 0; i < 3; i++) {
        out[i] = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(labels[i]));
        if (i) gtk_check_button_set_group(out[i], out[0]);
        g_object_set_data(G_OBJECT(out[i]), "idx", GINT_TO_POINTER(i));
        g_signal_connect(out[i], "toggled", cb, a);
        gtk_box_append(GTK_BOX(box), GTK_WIDGET(out[i]));
    }
    return box;
}

static GtkWidget *build_light(App *a) {
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_add_css_class(root, "page");

    GtkWidget *modes = gtk_grid_new();
    gtk_grid_set_column_homogeneous(GTK_GRID(modes), TRUE);
    for (int i = 0; i < N_MODES; i++) {
        a->mode_btn[i] = GTK_TOGGLE_BUTTON(gtk_toggle_button_new_with_label(mode_name[i]));
        gtk_widget_add_css_class(GTK_WIDGET(a->mode_btn[i]), "modetab");
        if (i) gtk_toggle_button_set_group(a->mode_btn[i], a->mode_btn[0]);
        g_object_set_data(G_OBJECT(a->mode_btn[i]), "mode", GINT_TO_POINTER(i));
        g_signal_connect(a->mode_btn[i], "toggled", G_CALLBACK(on_mode), a);
        gtk_grid_attach(GTK_GRID(modes), GTK_WIDGET(a->mode_btn[i]), i % 4, i / 4, 1, 1);
    }
    gtk_box_append(GTK_BOX(root), modes);

    GtkWidget *body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 40);
    GtkWidget *left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *t = gtk_label_new("LIGHT COLOR");
    gtk_widget_add_css_class(t, "section");
    gtk_label_set_xalign(GTK_LABEL(t), 0);
    gtk_box_append(GTK_BOX(left), t);
    GtkWidget *sw = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    for (int i = 0; i < N_SWATCHES; i++) {
        GtkWidget *b = gtk_button_new();
        char cls[16];
        snprintf(cls, sizeof cls, "sw%d", i);
        gtk_widget_add_css_class(b, "swatch");
        gtk_widget_add_css_class(b, cls);
        gtk_widget_set_size_request(b, 32, 24);
        g_object_set_data(G_OBJECT(b), "rgb", (gpointer)swatch_rgb[i]);
        g_signal_connect(b, "clicked", G_CALLBACK(on_swatch), a);
        gtk_box_append(GTK_BOX(sw), b);
    }
    gtk_box_append(GTK_BOX(left), sw);
    GtkWidget *cl = gtk_label_new("Custom color");
    gtk_label_set_xalign(GTK_LABEL(cl), 0);
    a->color = GTK_COLOR_DIALOG_BUTTON(gtk_color_dialog_button_new(gtk_color_dialog_new()));
    g_signal_connect(a->color, "notify::rgba", G_CALLBACK(on_color), a);
    gtk_box_append(GTK_BOX(left), cl);
    gtk_box_append(GTK_BOX(left), GTK_WIDGET(a->color));
    gtk_box_append(GTK_BOX(body), left);

    static const char *lvl[3] = {"LOW", "MEDIUM", "HIGH"};
    GtkWidget *right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 22);
    gtk_box_append(GTK_BOX(right), radio_column(a, "BRIGHTNESS LEVEL", lvl, a->bright, G_CALLBACK(on_bright)));
    gtk_box_append(GTK_BOX(right), radio_column(a, "SPEED", lvl, a->speed, G_CALLBACK(on_speed)));
    gtk_box_append(GTK_BOX(body), right);
    gtk_box_append(GTK_BOX(root), body);

    GtkWidget *n = gtk_label_new("Only BREATHING and OFF are identified so far. Brightness and speed are "
                                 "known to apply to BREATHING; other modes may use these values differently.");
    gtk_widget_add_css_class(n, "hint");
    gtk_label_set_wrap(GTK_LABEL(n), TRUE);
    gtk_label_set_xalign(GTK_LABEL(n), 0);
    gtk_box_append(GTK_BOX(root), n);
    return root;
}

/* ---- INFO ---- */

typedef struct { double us; int n, rc, err; } Meas;

static void meas_thread(GTask *t, gpointer src, gpointer data, GCancellable *c) {
    (void)src; (void)data; (void)c;
    Meas *m = g_new0(Meas, 1);
    m->rc = m711_measure_rate(4.0, &m->us, &m->n);
    m->err = errno;
    g_task_return_pointer(t, m, g_free);
}

static void meas_done(GObject *src, GAsyncResult *res, gpointer data) {
    App *a = data;
    Meas *m = g_task_propagate_pointer(G_TASK(src), NULL);
    char *t;
    if (m->rc) t = g_strdup_printf("No result: %s (keep moving the mouse during the test)", g_strerror(m->err));
    else t = g_strdup_printf("%d reports, median interval %.0f \xc2\xb5s = ~%.0f Hz", m->n, m->us, 1e6 / m->us);
    gtk_label_set_text(a->rate, t);
    g_free(t);
    g_free(m);
    (void)res;
    g_object_unref(src);
}

static void on_measure(GtkButton *b, App *a) {
    gtk_label_set_text(a->rate, "Measuring for 4 s: move the mouse continuously...");
    GTask *t = g_task_new(NULL, NULL, meas_done, a);
    (void)b;
    g_task_run_in_thread(t, meas_thread);
}

static GtkWidget *build_info(App *a) {
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_add_css_class(root, "page");
    char *id = g_strdup_printf("USB device %04x:%04x, config interface %d", M711_VID, M711_PID, M711_IFACE);
    GtkWidget *l = gtk_label_new(id);
    g_free(id);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    a->info_active = GTK_LABEL(gtk_label_new("Active profile: ?"));
    gtk_label_set_xalign(a->info_active, 0);
    GtkWidget *b = gtk_button_new_with_label("MEASURE REPORT RATE");
    gtk_widget_set_halign(b, GTK_ALIGN_START);
    gtk_widget_add_css_class(b, "action");
    g_signal_connect(b, "clicked", G_CALLBACK(on_measure), a);
    a->rate = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(a->rate, 0);
    gtk_box_append(GTK_BOX(root), l);
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(a->info_active));
    gtk_box_append(GTK_BOX(root), b);
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(a->rate));
    return root;
}

/* ---- window ---- */

static const char *css =
    "window { background: #141414; color: #e8e8e8; }"
    ".title { color: #ff1a1a; font-size: 30px; font-weight: 900; }"
    ".subtitle { color: #e8e8e8; font-weight: bold; letter-spacing: 2px; }"
    ".tabbar button { background: #1a1a1a; color: #fff; border: 2px solid #b00000; border-radius: 0;"
    "  font-weight: bold; padding: 6px 0; }"
    ".tabbar button:checked { background: #ff0000; border-color: #ff0000; }"
    ".page { padding: 16px; }"
    ".section { font-weight: bold; color: #ffffff; }"
    ".hint { color: #9a9a9a; font-size: 11px; }"
    ".num { background: #1a1a1a; border: 1.5px solid #d8d8d8; border-radius: 50%;"
    "  min-width: 22px; min-height: 22px; font-size: 11px; font-weight: bold; }"
    "button.action { background: #5a0000; color: #fff; border: 1px solid #7a0000; border-radius: 0;"
    "  font-weight: bold; box-shadow: none; }"
    "button.action:hover { background: #8a0000; }"
    ".dpihead { background: #ff0000; font-weight: bold; padding: 3px 14px; }"
    ".modetab { background: #5a1414; color: #fff; border-radius: 0; border: 1px solid #333; font-weight: bold; }"
    ".modetab:checked { background: #1a1a1a; border-bottom-color: #ff0000; color: #fff; }"
    ".swatch { border-radius: 0; border: 1px solid #333; padding: 0; min-width: 32px; }"
    ".sw0 { background: #ff0000; } .sw1 { background: #00ff00; } .sw2 { background: #0000ff; }"
    ".sw3 { background: #00ffff; } .sw4 { background: #ffff00; } .sw5 { background: #aa00cc; }"
    ".sw6 { background: #ffffff; }"
    ".profiles button { background: #5a0000; color: #fff; border-radius: 0; border: none; font-weight: bold;"
    "  min-width: 100px; box-shadow: none; }"
    ".profiles button:checked { background: #ff0000; }"
    ".bar button { background: #3c3c3c; color: #fff; border-radius: 0; border: 1px solid #555; font-weight: bold;"
    "  min-width: 110px; box-shadow: none; }"
    ".bar button:hover { background: #505050; }"
    ".status { color: #ff6060; padding: 4px 16px; }"
    "scale trough { background: #8a0000; min-width: 8px; min-height: 8px; }"
    "scale slider { background: #222; border: 2px solid #ff0000; border-radius: 0; min-width: 14px; min-height: 14px; margin: 0; padding: 0; }"
    "entry.error { border-color: #ff0000; }";

static GtkWidget *hbox_centered(GtkWidget **kids, int n, const char *cls, int spacing) {
    GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, spacing);
    gtk_widget_add_css_class(h, cls);
    gtk_widget_set_halign(h, GTK_ALIGN_CENTER);
    for (int i = 0; i < n; i++) gtk_box_append(GTK_BOX(h), kids[i]);
    return h;
}

/* Developer aid: M711_GUI_SNAPSHOT_DIR=/dir renders every tab to /dir/<page>.png and exits. */
static gboolean snap_step(gpointer data) {
    App *a = data;
    static int step = 0;
    static const char *pages[4] = {"general", "dpi", "light", "info"};
    const char *dir = g_getenv("M711_GUI_SNAPSHOT_DIR");
    if (step < 4) {
        GdkPaintable *p = gtk_widget_paintable_new(a->win);
        int w = gtk_widget_get_width(a->win), h = gtk_widget_get_height(a->win);
        GtkSnapshot *snap = gtk_snapshot_new();
        gdk_paintable_snapshot(p, snap, w, h);
        GskRenderNode *node = gtk_snapshot_free_to_node(snap);
        GskRenderer *r = gtk_native_get_renderer(GTK_NATIVE(a->win));
        GdkTexture *tex = gsk_renderer_render_texture(r, node, &GRAPHENE_RECT_INIT(0, 0, w, h));
        char *f = g_strdup_printf("%s/%s.png", dir, pages[step]);
        gdk_texture_save_to_png(tex, f);
        g_free(f);
        g_object_unref(tex);
        gsk_render_node_unref(node);
        g_object_unref(p);
    }
    if (++step >= 4) { g_application_quit(g_application_get_default()); return G_SOURCE_REMOVE; }
    gtk_toggle_button_set_active(a->tab[step], TRUE);
    return G_SOURCE_CONTINUE;
}

static void activate(GtkApplication *app, gpointer data) {
    App *a = data;
    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", TRUE, NULL);
    GtkCssProvider *prov = gtk_css_provider_new();
    gtk_css_provider_load_from_string(prov, css);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(prov),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(prov);

    a->win = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(a->win), "M711 Mouse Settings");
    gtk_window_set_default_size(GTK_WINDOW(a->win), 820, 660);
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

    GtkWidget *head = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *title = gtk_label_new("M711");
    gtk_widget_add_css_class(title, "title");
    GtkWidget *sub = gtk_label_new("GAMING MOUSE");
    gtk_widget_add_css_class(sub, "subtitle");
    gtk_widget_set_margin_top(head, 12);
    gtk_box_append(GTK_BOX(head), title);
    gtk_box_append(GTK_BOX(head), sub);

    a->stack = gtk_stack_new();
    gtk_widget_set_vexpand(a->stack, TRUE);
    static const char *names[4] = {"GENERAL", "DPI", "LIGHT", "INFO"};
    static const char *pages[4] = {"general", "dpi", "light", "info"};
    gtk_stack_add_named(GTK_STACK(a->stack), build_general(a), pages[0]);
    gtk_stack_add_named(GTK_STACK(a->stack), build_dpi(a), pages[1]);
    gtk_stack_add_named(GTK_STACK(a->stack), build_light(a), pages[2]);
    gtk_stack_add_named(GTK_STACK(a->stack), build_info(a), pages[3]);

    GtkWidget *tabs = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(tabs, "tabbar");
    gtk_box_set_homogeneous(GTK_BOX(tabs), TRUE);
    gtk_widget_set_margin_start(tabs, 16);
    gtk_widget_set_margin_end(tabs, 16);
    for (int i = 0; i < 4; i++) {
        a->tab[i] = GTK_TOGGLE_BUTTON(gtk_toggle_button_new_with_label(names[i]));
        if (i) gtk_toggle_button_set_group(a->tab[i], a->tab[0]);
        g_object_set_data(G_OBJECT(a->tab[i]), "page", (gpointer)pages[i]);
        g_signal_connect(a->tab[i], "toggled", G_CALLBACK(on_tab), a);
        gtk_box_append(GTK_BOX(tabs), GTK_WIDGET(a->tab[i]));
    }
    gtk_toggle_button_set_active(a->tab[0], TRUE);

    GtkWidget *pk[M711_PROFILES];
    for (int i = 0; i < M711_PROFILES; i++) {
        char t[16];
        snprintf(t, sizeof t, "PROFILE%d", i + 1);
        a->prof[i] = GTK_TOGGLE_BUTTON(gtk_toggle_button_new_with_label(t));
        if (i) gtk_toggle_button_set_group(a->prof[i], a->prof[0]);
        pk[i] = GTK_WIDGET(a->prof[i]);
    }
    gtk_toggle_button_set_active(a->prof[0], TRUE);
    for (int i = 0; i < M711_PROFILES; i++) g_signal_connect(a->prof[i], "toggled", G_CALLBACK(on_profile), a);
    GtkWidget *profiles = hbox_centered(pk, M711_PROFILES, "profiles", 14);

    GtkWidget *restore = gtk_button_new_with_label("RESTORE");
    GtkWidget *active = gtk_button_new_with_label("SET ACTIVE");
    GtkWidget *apply = gtk_button_new_with_label("APPLY");
    gtk_widget_set_tooltip_text(restore, "Reload settings from the mouse, discarding edits");
    gtk_widget_set_tooltip_text(active, "Make the selected profile the one the mouse uses");
    g_signal_connect(restore, "clicked", G_CALLBACK(on_restore), a);
    g_signal_connect(active, "clicked", G_CALLBACK(on_set_active), a);
    g_signal_connect(apply, "clicked", G_CALLBACK(on_apply), a);
    GtkWidget *bk[3] = {restore, active, apply};
    GtkWidget *bar = hbox_centered(bk, 3, "bar", 10);

    a->status = GTK_LABEL(gtk_label_new(""));
    gtk_widget_add_css_class(GTK_WIDGET(a->status), "status");
    gtk_label_set_xalign(a->status, 0);
    gtk_widget_set_margin_bottom(bar, 6);

    gtk_box_append(GTK_BOX(root), head);
    gtk_box_append(GTK_BOX(root), tabs);
    gtk_box_append(GTK_BOX(root), a->stack);
    gtk_box_append(GTK_BOX(root), profiles);
    gtk_box_append(GTK_BOX(root), bar);
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(a->status));
    gtk_window_set_child(GTK_WINDOW(a->win), root);
    refresh(a);
    gtk_window_present(GTK_WINDOW(a->win));
    if (g_getenv("M711_GUI_SNAPSHOT_DIR")) g_timeout_add(900, snap_step, a);
}

int main(int argc, char **argv) {
    App a = {0};
    GtkApplication *app = gtk_application_new("org.m711.settings", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), &a);
    int rc = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    m711_close(a.dev);
    return rc;
}
