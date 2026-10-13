#include <ctype.h>
#include <gtk-layer-shell.h>
#include <gtk/gtk.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_NODES 16

typedef struct {
    int id;
    int is_default;
    char name[192];
} Node;

typedef struct {
    const char *title;
    const char *target;
    const char *section;
    const char *until;
    GtkWidget *scale;
    GtkWidget *percent;
    GtkWidget *mute;
    GtkWidget *list;
    char signature[2048];
    int updating;
    int want;
    guint volume_id;
} Group;

static Group output = {
    .title = "Output",
    .target = "@DEFAULT_AUDIO_SINK@",
    .section = "Sinks:",
    .until = "Sources:",
};

static Group input = {
    .title = "Input",
    .target = "@DEFAULT_AUDIO_SOURCE@",
    .section = "Sources:",
    .until = "Filters:",
};

static GtkWidget *error_label;
static int alive;
static int lock_fd = -1;

static void quit_app(void) { gtk_main_quit(); }

static int wp_exec(char **argv, char **out, char **err) {
    GError *error = NULL;
    int status = 0;
    *out = NULL;
    *err = NULL;
    if (!g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, out, err, &status, &error)) {
        if (*err == NULL) *err = g_strdup(error->message);
        g_error_free(error);
        return -1;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return 0;
    return 1;
}

static char *skip_tree(char *s) {
    while (*s != '\0') {
        unsigned char c = (unsigned char)*s;
        if (c == ' ' || c == '\t') {
            s++;
            continue;
        }
        if (c < 128) break;
        if ((c & 0xE0) == 0xC0 && s[1] != '\0') s += 2;
        else if ((c & 0xF0) == 0xE0 && s[1] != '\0' && s[2] != '\0') s += 3;
        else if ((c & 0xF8) == 0xF0 && s[1] != '\0' && s[2] != '\0' && s[3] != '\0') s += 4;
        else break;
    }
    return s;
}

static int parse_nodes(char *text, const char *section, const char *until, Node *nodes, int max) {
    char *save = NULL;
    int in = 0;
    int n = 0;
    if (text == NULL) return 0;
    for (char *line = strtok_r(text, "\n", &save); line != NULL; line = strtok_r(NULL, "\n", &save)) {
        char *p;
        char *dot;
        char *bracket;
        int is_default = 0;
        if (!in) {
            if (strstr(line, section) != NULL) in = 1;
            continue;
        }
        if (strstr(line, until) != NULL) break;
        p = skip_tree(line);
        if (*p == '*') {
            is_default = 1;
            p++;
        }
        while (*p == ' ' || *p == '\t') p++;
        if (!isdigit((unsigned char)*p) || n >= max) continue;
        nodes[n].id = atoi(p);
        nodes[n].is_default = is_default;
        dot = strchr(p, '.');
        if (dot == NULL || dot[1] != ' ') continue;
        p = dot + 2;
        bracket = strstr(p, "[vol:");
        if (bracket != NULL) {
            while (bracket > p && bracket[-1] == ' ') bracket--;
            *bracket = '\0';
        }
        snprintf(nodes[n].name, sizeof nodes[n].name, "%s", p);
        n++;
    }
    return n;
}

static int parse_volume(const char *text, int *percent, int *muted) {
    double value = 0;
    if (text == NULL || sscanf(text, "Volume: %lf", &value) != 1) return 0;
    *percent = (int)(value * 100.0 + 0.5);
    if (*percent < 0) *percent = 0;
    if (*percent > 100) *percent = 100;
    *muted = strstr(text, "[MUTED]") != NULL;
    return 1;
}

static void show_error(const char *text) {
    gtk_label_set_text(GTK_LABEL(error_label), text != NULL && text[0] != '\0' ? text : "Something went wrong");
    gtk_widget_show(error_label);
}

static void hide_error(void) {
    gtk_label_set_text(GTK_LABEL(error_label), "");
    gtk_widget_hide(error_label);
}

static void set_percent(Group *group, int percent) {
    char buf[8];
    snprintf(buf, sizeof buf, "%d%%", percent);
    gtk_label_set_text(GTK_LABEL(group->percent), buf);
}

static void node_signature(const Node *nodes, int n, char *dst, size_t dst_n) {
    size_t used = 0;
    dst[0] = '\0';
    for (int i = 0; i < n; i++) {
        int wrote = snprintf(dst + used, dst_n - used, "%d:%d:%s;", nodes[i].id, nodes[i].is_default, nodes[i].name);
        if (wrote < 0 || (size_t)wrote >= dst_n - used) break;
        used += (size_t)wrote;
    }
}

static void clear_list(GtkWidget *list) {
    GList *children = gtk_container_get_children(GTK_CONTAINER(list));
    for (GList *item = children; item != NULL; item = item->next) gtk_widget_destroy(GTK_WIDGET(item->data));
    g_list_free(children);
}

static GtkWidget *node_row(const Node *node) {
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *label = gtk_label_new(node->name);
    gtk_widget_set_name(label, "ssid");
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 28);
    gtk_container_add(GTK_CONTAINER(row), label);
    if (node->is_default) gtk_style_context_add_class(gtk_widget_get_style_context(row), "current");
    g_object_set_data(G_OBJECT(row), "id", GINT_TO_POINTER(node->id));
    return row;
}

static void fill_nodes(Group *group, const Node *nodes, int n) {
    char next[sizeof group->signature];
    node_signature(nodes, n, next, sizeof next);
    if (strcmp(next, group->signature) == 0) return;
    snprintf(group->signature, sizeof group->signature, "%s", next);
    clear_list(group->list);
    for (int i = 0; i < n; i++) gtk_container_add(GTK_CONTAINER(group->list), node_row(&nodes[i]));
    gtk_widget_show_all(group->list);
}

static void refresh_group(Group *group, const char *status) {
    char *copy = g_strdup(status);
    char *volume_argv[] = {"wpctl", "get-volume", (char *)group->target, NULL};
    char *volume = NULL;
    char *err = NULL;
    Node nodes[MAX_NODES];
    int n = parse_nodes(copy, group->section, group->until, nodes, MAX_NODES);
    int percent = 0;
    int muted = 0;
    g_free(copy);
    fill_nodes(group, nodes, n);
    if (wp_exec(volume_argv, &volume, &err) != 0 || !parse_volume(volume, &percent, &muted)) {
        show_error(err != NULL && err[0] != '\0' ? err : "Couldn't read volume");
        g_free(volume);
        g_free(err);
        return;
    }
    g_free(err);
    g_free(volume);
    gtk_button_set_label(GTK_BUTTON(group->mute), muted ? "Unmute" : "Mute");
    if (group->volume_id == 0) {
        group->updating = 1;
        gtk_range_set_value(GTK_RANGE(group->scale), percent);
        group->updating = 0;
        set_percent(group, percent);
    }
}

static void refresh(void) {
    char *argv[] = {"wpctl", "status", NULL};
    char *status = NULL;
    char *err = NULL;
    if (!alive) return;
    if (wp_exec(argv, &status, &err) != 0) {
        show_error(err != NULL && err[0] != '\0' ? err : "Couldn't list devices");
        g_free(status);
        g_free(err);
        return;
    }
    g_free(err);
    hide_error();
    refresh_group(&output, status);
    refresh_group(&input, status);
    g_free(status);
}

static gboolean tick(gpointer data) {
    (void)data;
    if (!alive) return G_SOURCE_REMOVE;
    refresh();
    return G_SOURCE_CONTINUE;
}

static gboolean flush_volume(gpointer data) {
    Group *group = data;
    char vol[16];
    char *argv[] = {"wpctl", "set-volume", "-l", "1.0", (char *)group->target, vol, NULL};
    char *out = NULL;
    char *err = NULL;
    group->volume_id = 0;
    if (!alive) return G_SOURCE_REMOVE;
    snprintf(vol, sizeof vol, "%.2f", group->want / 100.0);
    if (wp_exec(argv, &out, &err) != 0) show_error(err != NULL && err[0] != '\0' ? err : "Couldn't set volume");
    else hide_error();
    g_free(out);
    g_free(err);
    return G_SOURCE_REMOVE;
}

static void on_scale(GtkRange *range, gpointer data) {
    Group *group = data;
    if (group->updating) return;
    group->want = (int)(gtk_range_get_value(range) + 0.5);
    set_percent(group, group->want);
    if (group->volume_id != 0) g_source_remove(group->volume_id);
    group->volume_id = g_timeout_add(60, flush_volume, group);
}

static void on_mute(GtkWidget *widget, gpointer data) {
    Group *group = data;
    char *argv[] = {"wpctl", "set-mute", (char *)group->target, "toggle", NULL};
    char *out = NULL;
    char *err = NULL;
    (void)widget;
    if (wp_exec(argv, &out, &err) != 0) show_error(err != NULL && err[0] != '\0' ? err : "Couldn't mute");
    g_free(out);
    g_free(err);
    refresh();
}

static void on_row(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    Group *group = data;
    char id[16];
    char *argv[] = {"wpctl", "set-default", id, NULL};
    char *out = NULL;
    char *err = NULL;
    (void)box;
    if (row == NULL) return;
    snprintf(id, sizeof id, "%d", GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "id")));
    if (wp_exec(argv, &out, &err) != 0) show_error(err != NULL && err[0] != '\0' ? err : "Couldn't switch device");
    g_free(out);
    g_free(err);
    group->signature[0] = '\0';
    refresh();
}

static gboolean on_key(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    (void)widget;
    (void)data;
    if (event->keyval == GDK_KEY_Escape) {
        quit_app();
        return TRUE;
    }
    return FALSE;
}

static gboolean on_outside(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    (void)widget;
    (void)event;
    (void)data;
    quit_app();
    return TRUE;
}

static gboolean swallow(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    (void)widget;
    (void)event;
    (void)data;
    return TRUE;
}

static void on_destroy(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    alive = 0;
    quit_app();
}

static int cursor_pos(int *x, int *y) {
    char *argv[] = {"hyprctl", "cursorpos", NULL};
    char *out = NULL;
    char *err = NULL;
    int ok = 0;
    if (wp_exec(argv, &out, &err) == 0 && out != NULL)
        ok = sscanf(out, "%d, %d", x, y) == 2 || sscanf(out, "%d,%d", x, y) == 2;
    g_free(out);
    g_free(err);
    return ok;
}

static void place_panel(GtkWidget *window, GtkWidget *panel) {
    GdkDisplay *display = gdk_display_get_default();
    int x = 0;
    int y = 0;
    int from_right = 12;

    if (display != NULL && cursor_pos(&x, &y)) {
        GdkMonitor *monitor = gdk_display_get_monitor_at_point(display, x, y);
        if (monitor != NULL) {
            GdkRectangle geom;
            int limit;
            gtk_layer_set_monitor(GTK_WINDOW(window), monitor);
            gdk_monitor_get_geometry(monitor, &geom);
            from_right = geom.x + geom.width - x;
            limit = geom.width - 320;
            if (limit < 8) limit = 8;
            if (from_right < 8) from_right = 8;
            if (from_right > limit) from_right = limit;
        }
    }
    gtk_widget_set_margin_end(panel, from_right);
}

static void hold_lock(void) {
    char path[PATH_MAX];
    const char *cache = g_get_user_cache_dir();

    g_mkdir_with_parents(cache, 0700);
    snprintf(path, sizeof path, "%s/waybar-audio.lock", cache);
    lock_fd = open(path, O_RDWR | O_CREAT, 0600);
    if (lock_fd < 0) return;
    if (flock(lock_fd, LOCK_EX | LOCK_NB) == 0) {
        char pid[32];
        snprintf(pid, sizeof pid, "%d\n", getpid());
        if (ftruncate(lock_fd, 0) == 0) {
            ssize_t written = write(lock_fd, pid, strlen(pid));
            (void)written;
        }
        return;
    }
    char buf[32] = {0};
    ssize_t n = read(lock_fd, buf, sizeof buf - 1);
    if (n > 0) {
        pid_t other = (pid_t)atoi(buf);
        if (other > 0) kill(other, SIGTERM);
    }
    _exit(0);
}

static GtkWidget *flat_button(const char *label) {
    GtkWidget *button = gtk_button_new_with_label(label);
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    return button;
}

static void build_group(GtkWidget *box, Group *group) {
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *title = gtk_label_new(group->title);
    GtkWidget *volume_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);

    gtk_widget_set_name(title, "title");
    gtk_label_set_xalign(GTK_LABEL(title), 0);
    group->mute = flat_button("Mute");
    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(header), group->mute, FALSE, FALSE, 0);

    group->scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_scale_set_draw_value(GTK_SCALE(group->scale), FALSE);
    gtk_range_set_increments(GTK_RANGE(group->scale), 1, 5);
    gtk_widget_set_hexpand(group->scale, TRUE);
    gtk_widget_set_valign(group->scale, GTK_ALIGN_CENTER);
    group->percent = gtk_label_new("0%");
    gtk_widget_set_name(group->percent, "percent");
    gtk_label_set_width_chars(GTK_LABEL(group->percent), 4);
    gtk_label_set_xalign(GTK_LABEL(group->percent), 1);
    gtk_box_pack_start(GTK_BOX(volume_row), group->scale, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(volume_row), group->percent, FALSE, FALSE, 0);

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scroll), TRUE);
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(scroll), 140);
    gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(scroll), 280);
    group->list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(group->list), GTK_SELECTION_NONE);
    gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(group->list), TRUE);
    gtk_container_add(GTK_CONTAINER(scroll), group->list);

    gtk_box_pack_start(GTK_BOX(box), header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), volume_row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), scroll, FALSE, FALSE, 0);

    g_signal_connect(group->scale, "value-changed", G_CALLBACK(on_scale), group);
    g_signal_connect(group->mute, "clicked", G_CALLBACK(on_mute), group);
    g_signal_connect(group->list, "row-activated", G_CALLBACK(on_row), group);
}

int main(void) {
    static const char css[] =
        "window { background-color: rgba(0, 0, 0, 0); }"
        "#panel { background-color: #1e1e2e; border-radius: 8px; padding: 10px; }"
        "#title { color: #ffffff; font-size: 14px; font-weight: bold; }"
        "#ssid { color: #ffffff; font-size: 14px; }"
        "#error { color: #f38ba8; font-size: 12px; }"
        "#percent { color: #ffffff; font-size: 14px; }"
        "button { background: transparent; color: #ffffff; border: none; box-shadow: none; padding: 2px 8px; }"
        "button:hover { background-color: #313244; }"
        "list, row, scrolledwindow { background: transparent; border: none; }"
        "row { padding: 4px 6px; border-radius: 6px; }"
        "row:hover { background-color: #313244; }"
        "row.current, row.current:hover { background-color: #64727D; }"
        "scale trough { background-color: #313244; min-height: 6px; border-radius: 3px; }"
        "scale highlight { background-color: #8da9c4; border-radius: 3px; }"
        "scale slider { background-color: #ffffff; min-width: 14px; min-height: 14px; }";
    GtkWidget *window, *backdrop, *panel, *box;
    GtkCssProvider *provider;
    GdkScreen *screen;
    GdkVisual *visual;

    hold_lock();
    gtk_init(NULL, NULL);
    alive = 1;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    gtk_widget_set_app_paintable(window, TRUE);
    screen = gdk_screen_get_default();
    visual = gdk_screen_get_rgba_visual(screen);
    if (visual != NULL) gtk_widget_set_visual(window, visual);

    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_exclusive_zone(GTK_WINDOW(window), -1);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);

    backdrop = gtk_event_box_new();
    panel = gtk_event_box_new();
    gtk_widget_set_name(panel, "panel");
    gtk_widget_set_halign(panel, GTK_ALIGN_END);
    gtk_widget_set_valign(panel, GTK_ALIGN_START);
    gtk_widget_set_margin_top(panel, 36);
    gtk_widget_set_size_request(panel, 300, -1);
    place_panel(window, panel);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    build_group(box, &output);
    build_group(box, &input);
    error_label = gtk_label_new("");
    gtk_widget_set_name(error_label, "error");
    gtk_label_set_xalign(GTK_LABEL(error_label), 0);
    gtk_label_set_line_wrap(GTK_LABEL(error_label), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(error_label), 32);
    gtk_box_pack_start(GTK_BOX(box), error_label, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(panel), box);
    gtk_container_add(GTK_CONTAINER(backdrop), panel);
    gtk_container_add(GTK_CONTAINER(window), backdrop);

    provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_signal_connect(window, "destroy", G_CALLBACK(on_destroy), NULL);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key), NULL);
    g_signal_connect(backdrop, "button-press-event", G_CALLBACK(on_outside), NULL);
    g_signal_connect(panel, "button-press-event", G_CALLBACK(swallow), NULL);

    gtk_widget_show_all(window);
    gtk_widget_hide(error_label);
    refresh();
    gtk_widget_grab_focus(output.scale);
    g_timeout_add_seconds(1, tick, NULL);
    gtk_main();
    if (lock_fd >= 0) close(lock_fd);
    return 0;
}
