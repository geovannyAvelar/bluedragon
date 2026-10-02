/* m711-gui: GTK4 front end for libm711. Operations run synchronously (each takes < 0.5 s). */
#include <errno.h>
#include <gtk/gtk.h>
#include <m711.h>
#include <string.h>

#define N_MODES 8
static const int poll_hz[] = {1000, 500, 250, 125};

typedef struct {
    m711 *dev;
    GtkWidget *win;
    GtkDropDown *profile;
    GtkLabel *active, *status;
    GtkScale *dpi[M711_DPI_LEVELS];
    GtkEntry *btn[M711_BUTTONS];
    GtkColorDialogButton *color;
    GtkDropDown *mode, *poll;
    GtkSpinButton *value, *level;
    uint8_t led_type, led_sub;       /* kept when "custom" mode is selected */
    gboolean loading;                /* suppress change handlers while refreshing */
} App;

static void say(App *a, const char *fmt, ...) G_GNUC_PRINTF(2, 3);
static void say(App *a, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *s = g_strdup_vprintf(fmt, ap);
    va_end(ap);
    gtk_label_set_text(a->status, s);
    g_free(s);
}

static int cur_profile(App *a) { return gtk_drop_down_get_selected(a->profile); }

static gboolean need_dev(App *a) {
    if (a->dev) return TRUE;
    say(a, "No device. Plug the mouse in (check hidraw permissions) and press Reload.");
    return FALSE;
}

/* ---- device -> widgets ---- */

static void refresh(App *a) {
    if (!a->dev) {
        a->dev = m711_open(NULL);
        if (!a->dev) {
            say(a, "Cannot open device: %s", g_strerror(errno));
            return;
        }
    }
    int p = cur_profile(a);
    uint8_t act = 0;
    a->loading = TRUE;
    if (m711_read(a->dev, 0x2c, &act, 1) == 0) {
        char *t = g_strdup_printf("Active profile: %d", act + 1);
        gtk_label_set_text(a->active, t);
        g_free(t);
    }
    for (int l = 0; l < M711_DPI_LEVELS; l++) {
        int dpi = m711_get_dpi(a->dev, p, l);
        if (dpi > 0) gtk_range_set_value(GTK_RANGE(a->dpi[l]), dpi);
    }
    for (int b = 0; b < M711_BUTTONS; b++) {
        uint8_t e[4];
        char name[32] = "?";
        if (m711_get_button(a->dev, p, b, e) == 0) m711_action_name(e, name, sizeof name);
        gtk_editable_set_text(GTK_EDITABLE(a->btn[b]), name);
        gtk_widget_remove_css_class(GTK_WIDGET(a->btn[b]), "error");
    }
    m711_led led;
    if (m711_get_led(a->dev, p, &led) == 0) {
        GdkRGBA c = {led.r / 255.0f, led.g / 255.0f, led.b / 255.0f, 1.0f};
        gtk_color_dialog_button_set_rgba(a->color, &c);
        int mode = m711_led_mode_of(&led);
        gtk_drop_down_set_selected(a->mode, mode < 0 ? N_MODES : mode);
        gtk_spin_button_set_value(a->value, led.value);
        gtk_spin_button_set_value(a->level, led.level);
        a->led_type = led.type;
        a->led_sub = led.sub;
    }
    int raw = m711_get_polling_raw(a->dev, p), sel = GTK_INVALID_LIST_POSITION;
    for (guint i = 0; i < G_N_ELEMENTS(poll_hz); i++)
        if (m711_polling_hz(raw) == poll_hz[i]) sel = i;
    gtk_drop_down_set_selected(a->poll, sel);
    a->loading = FALSE;
    say(a, "Loaded profile %d.", p + 1);
}

/* ---- widgets -> device ---- */

static void report(App *a, const char *what, int rc) {
    if (rc == 0) say(a, "%s applied to profile %d.", what, cur_profile(a) + 1);
    else say(a, "%s failed: %s", what, g_strerror(errno));
}

static void on_apply_dpi(GtkButton *w, App *a) {
    (void)w;
    if (!need_dev(a)) return;
    int rc = 0;
    for (int l = 0; l < M711_DPI_LEVELS && !rc; l++) {
        int dpi = (int)(gtk_range_get_value(GTK_RANGE(a->dpi[l])) / 100 + 0.5) * 100;
        if (dpi != m711_get_dpi(a->dev, cur_profile(a), l))
            rc = m711_set_dpi(a->dev, cur_profile(a), l, dpi);
    }
    report(a, "DPI", rc);
}

static void on_button_edited(GtkEditable *e, App *a) {
    uint8_t out[4];
    const char *t = gtk_editable_get_text(e);
    if (a->loading) return;
    if (m711_action_parse(t, out) == 0) gtk_widget_remove_css_class(GTK_WIDGET(e), "error");
    else gtk_widget_add_css_class(GTK_WIDGET(e), "error");
}

static void on_apply_buttons(GtkButton *w, App *a) {
    (void)w;
    if (!need_dev(a)) return;
    int p = cur_profile(a), rc = 0, changed = 0;
    uint8_t want[M711_BUTTONS][4];
    for (int b = 0; b < M711_BUTTONS; b++) {         /* validate everything first */
        if (m711_action_parse(gtk_editable_get_text(GTK_EDITABLE(a->btn[b])), want[b])) {
            say(a, "Button %d: invalid action (see tooltip for syntax).", b);
            return;
        }
    }
    for (int b = 0; b < M711_BUTTONS && !rc; b++) {
        uint8_t cur[4];
        if (m711_get_button(a->dev, p, b, cur) == 0 && !memcmp(cur, want[b], 4)) continue;
        rc = m711_set_button(a->dev, p, b, want[b]);
        changed++;
    }
    if (!rc && !changed) say(a, "Buttons: nothing changed.");
    else report(a, "Buttons", rc);
    if (!rc) refresh(a);
}

static void on_apply_led(GtkButton *w, App *a) {
    (void)w;
    if (!need_dev(a)) return;
    GdkRGBA c = *gtk_color_dialog_button_get_rgba(a->color);
    m711_led led = {.r = (uint8_t)(c.red * 255 + 0.5), .g = (uint8_t)(c.green * 255 + 0.5),
                    .b = (uint8_t)(c.blue * 255 + 0.5),
                    .value = (uint8_t)gtk_spin_button_get_value_as_int(a->value),
                    .level = (uint8_t)gtk_spin_button_get_value_as_int(a->level)};
    guint mode = gtk_drop_down_get_selected(a->mode);
    if (mode < N_MODES) m711_led_mode_to_type(mode, &led.type, &led.sub);
    else { led.type = a->led_type; led.sub = a->led_sub; }
    report(a, "LED", m711_set_led(a->dev, cur_profile(a), &led));
}

static void on_apply_poll(GtkButton *w, App *a) {
    (void)w;
    if (!need_dev(a)) return;
    guint i = gtk_drop_down_get_selected(a->poll);
    if (i >= G_N_ELEMENTS(poll_hz)) { say(a, "Pick a polling rate first."); return; }
    report(a, "Polling rate", m711_set_polling(a->dev, cur_profile(a), poll_hz[i]));
}

static void on_set_active(GtkButton *w, App *a) {
    (void)w;
    if (!need_dev(a)) return;
    int rc = m711_set_profile(a->dev, cur_profile(a));
    if (rc == 0) {
        say(a, "Profile %d is now active.", cur_profile(a) + 1);
        refresh(a);
    } else say(a, "Set active failed: %s", g_strerror(errno));
}

static void on_reload(GtkButton *w, App *a) { (void)w; refresh(a); }
static void on_profile(GObject *o, GParamSpec *ps, App *a) { (void)o; (void)ps; refresh(a); }

/* ---- UI construction ---- */

static GtkWidget *page(GtkWidget **box_out) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    *box_out = box;
    return box;
}

static GtkWidget *row(const char *label, GtkWidget *w) {
    GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *l = gtk_label_new(label);
    gtk_widget_set_size_request(l, 110, -1);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_widget_set_hexpand(w, TRUE);
    gtk_box_append(GTK_BOX(h), l);
    gtk_box_append(GTK_BOX(h), w);
    return h;
}

static GtkWidget *apply_button(const char *label, GCallback cb, App *a) {
    GtkWidget *b = gtk_button_new_with_label(label);
    gtk_widget_add_css_class(b, "suggested-action");
    gtk_widget_set_halign(b, GTK_ALIGN_END);
    g_signal_connect(b, "clicked", cb, a);
    return b;
}

static GtkWidget *build_dpi(App *a) {
    GtkWidget *box, *p = page(&box);
    for (int l = 0; l < M711_DPI_LEVELS; l++) {
        char t[16];
        snprintf(t, sizeof t, "Level %d", l + 1);
        a->dpi[l] = GTK_SCALE(gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 100, 5000, 100));
        gtk_scale_set_draw_value(a->dpi[l], TRUE);
        gtk_scale_set_value_pos(a->dpi[l], GTK_POS_RIGHT);
        gtk_scale_set_digits(a->dpi[l], 0);
        gtk_box_append(GTK_BOX(box), row(t, GTK_WIDGET(a->dpi[l])));
    }
    gtk_box_append(GTK_BOX(box), apply_button("Apply DPI", G_CALLBACK(on_apply_dpi), a));
    return p;
}

static GtkWidget *build_buttons(App *a) {
    GtkWidget *box, *p = page(&box);
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    for (int b = 0; b < M711_BUTTONS; b++) {
        char t[16];
        snprintf(t, sizeof t, "Button %d", b);
        a->btn[b] = GTK_ENTRY(gtk_entry_new());
        gtk_widget_set_hexpand(GTK_WIDGET(a->btn[b]), TRUE);
        gtk_widget_set_tooltip_text(GTK_WIDGET(a->btn[b]),
            "left right middle button4 button5 none\n"
            "stop playpause prev next volup voldown mute\n"
            "key:f5  key:enter  key:ctrl+shift+a\n"
            "raw:AABBCCDD");
        g_signal_connect(a->btn[b], "changed", G_CALLBACK(on_button_edited), a);
        GtkWidget *l = gtk_label_new(t);
        gtk_label_set_xalign(GTK_LABEL(l), 0);
        gtk_grid_attach(GTK_GRID(grid), l, (b / 6) * 2, b % 6, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(a->btn[b]), (b / 6) * 2 + 1, b % 6, 1, 1);
    }
    gtk_box_append(GTK_BOX(box), grid);
    gtk_box_append(GTK_BOX(box), apply_button("Apply buttons", G_CALLBACK(on_apply_buttons), a));
    return p;
}

static GtkWidget *build_led(App *a) {
    GtkWidget *box, *p = page(&box);
    a->color = GTK_COLOR_DIALOG_BUTTON(gtk_color_dialog_button_new(gtk_color_dialog_new()));
    const char *modes[N_MODES + 2] = {"Mode 0", "Mode 1", "Mode 2", "Mode 3 (uncertain)",
        "Mode 4 (uncertain)", "Mode 5", "Mode 6 (uncertain)", "Mode 7", "Custom (keep current)", NULL};
    a->mode = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(modes));
    a->value = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0, 255, 1));
    a->level = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0, 255, 1));
    gtk_box_append(GTK_BOX(box), row("Color", GTK_WIDGET(a->color)));
    gtk_box_append(GTK_BOX(box), row("Mode", GTK_WIDGET(a->mode)));
    gtk_box_append(GTK_BOX(box), row("Value", GTK_WIDGET(a->value)));
    gtk_box_append(GTK_BOX(box), row("Level", GTK_WIDGET(a->level)));
    gtk_box_append(GTK_BOX(box), apply_button("Apply LED", G_CALLBACK(on_apply_led), a));
    return p;
}

static GtkWidget *build_poll(App *a) {
    GtkWidget *box, *p = page(&box);
    const char *rates[] = {"1000 Hz", "500 Hz", "250 Hz (unverified)", "125 Hz (unverified)", NULL};
    a->poll = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(rates));
    gtk_box_append(GTK_BOX(box), row("Polling rate", GTK_WIDGET(a->poll)));
    gtk_box_append(GTK_BOX(box), apply_button("Apply polling rate", G_CALLBACK(on_apply_poll), a));
    return p;
}

static void activate(GtkApplication *app, gpointer data) {
    App *a = data;
    a->win = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(a->win), "M711 Mouse Settings");
    gtk_window_set_default_size(GTK_WINDOW(a->win), 640, 520);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_top(top, 8);
    gtk_widget_set_margin_start(top, 12);
    gtk_widget_set_margin_end(top, 12);
    const char *profs[] = {"Profile 1", "Profile 2", "Profile 3", "Profile 4", "Profile 5", NULL};
    a->profile = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(profs));
    a->active = GTK_LABEL(gtk_label_new("Active profile: ?"));
    gtk_widget_set_hexpand(GTK_WIDGET(a->active), TRUE);
    gtk_label_set_xalign(a->active, 0);
    GtkWidget *set = gtk_button_new_with_label("Set active");
    GtkWidget *reload = gtk_button_new_with_label("Reload");
    gtk_widget_set_tooltip_text(set, "Make the selected profile the one the mouse uses");
    g_signal_connect(set, "clicked", G_CALLBACK(on_set_active), a);
    g_signal_connect(reload, "clicked", G_CALLBACK(on_reload), a);
    gtk_box_append(GTK_BOX(top), GTK_WIDGET(a->profile));
    gtk_box_append(GTK_BOX(top), set);
    gtk_box_append(GTK_BOX(top), GTK_WIDGET(a->active));
    gtk_box_append(GTK_BOX(top), reload);

    GtkWidget *stack = gtk_stack_new();
    gtk_stack_add_titled(GTK_STACK(stack), build_dpi(a), "dpi", "DPI");
    gtk_stack_add_titled(GTK_STACK(stack), build_buttons(a), "buttons", "Buttons");
    gtk_stack_add_titled(GTK_STACK(stack), build_led(a), "led", "LED");
    gtk_stack_add_titled(GTK_STACK(stack), build_poll(a), "poll", "Polling");
    GtkWidget *sw = gtk_stack_switcher_new();
    gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(sw), GTK_STACK(stack));
    gtk_widget_set_halign(sw, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(sw, 8);
    gtk_widget_set_vexpand(stack, TRUE);

    a->status = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(a->status, 0);
    gtk_widget_set_margin_start(GTK_WIDGET(a->status), 12);
    gtk_widget_set_margin_bottom(GTK_WIDGET(a->status), 8);

    gtk_box_append(GTK_BOX(root), top);
    gtk_box_append(GTK_BOX(root), sw);
    gtk_box_append(GTK_BOX(root), stack);
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(a->status));
    gtk_window_set_child(GTK_WINDOW(a->win), root);

    g_signal_connect(a->profile, "notify::selected", G_CALLBACK(on_profile), a);
    refresh(a);
    gtk_window_present(GTK_WINDOW(a->win));
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
