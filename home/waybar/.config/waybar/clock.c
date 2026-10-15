#include <errno.h>
#include <gtk-layer-shell.h>
#include <gtk/gtk.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <time.h>
#include <unistd.h>

static int lock_fd = -1;

static void quit(void) { gtk_main_quit(); }

static gboolean on_key(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    (void)widget;
    (void)data;
    if (event->keyval == GDK_KEY_Escape) {
        quit();
        return TRUE;
    }
    return FALSE;
}

static gboolean on_outside(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    (void)widget;
    (void)event;
    (void)data;
    quit();
    return TRUE;
}

static void hold_lock(void) {
    char path[PATH_MAX];
    const char *cache = g_get_user_cache_dir();

    g_mkdir_with_parents(cache, 0700);
    snprintf(path, sizeof path, "%s/waybar-clock.lock", cache);
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

static char *zone_name(void) {
    const char *prefix = "/usr/share/zoneinfo/";
    char *path = realpath("/etc/localtime", NULL);
    if (path == NULL) return g_strdup("local");
    if (strncmp(path, prefix, strlen(prefix)) == 0) {
        char *name = g_strdup(path + strlen(prefix));
        free(path);
        return name;
    }
    return path;
}

static void format_offset(const char *raw, char *out, size_t out_len) {
    if (strlen(raw) == 5)
        snprintf(out, out_len, "%c%c%c:%c%c", raw[0], raw[1], raw[2], raw[3], raw[4]);
    else
        snprintf(out, out_len, "%s", raw);
}

static gboolean refresh(gpointer data) {
    GtkLabel **labels = data;
    time_t now = time(NULL);
    struct tm local_tm;
    struct tm utc_tm;
    char clock_s[16], date_s[64], zone_s[32], raw_off[16], offset[16], utc_s[16];
    char week[8], yday[8], meta[256];

    localtime_r(&now, &local_tm);
    gmtime_r(&now, &utc_tm);
    strftime(clock_s, sizeof clock_s, "%H:%M:%S", &local_tm);
    strftime(date_s, sizeof date_s, "%A, %d %B %Y", &local_tm);
    strftime(zone_s, sizeof zone_s, "%Z", &local_tm);
    strftime(raw_off, sizeof raw_off, "%z", &local_tm);
    strftime(utc_s, sizeof utc_s, "%H:%M:%S", &utc_tm);
    strftime(week, sizeof week, "%V", &local_tm);
    strftime(yday, sizeof yday, "%-j", &local_tm);
    format_offset(raw_off, offset, sizeof offset);
    snprintf(meta, sizeof meta, "%s\n%s  %s\nUTC  %s\nWeek %s   Day %s",
             (const char *)g_object_get_data(G_OBJECT(labels[0]), "iana"), zone_s, offset,
             utc_s, week, yday);
    gtk_label_set_text(labels[0], clock_s);
    gtk_label_set_text(labels[1], date_s);
    gtk_label_set_text(labels[2], meta);
    return G_SOURCE_CONTINUE;
}

static void load_theme(GtkCssProvider *provider, const char *name, const char *fallback) {
    char *path = g_build_filename(g_get_home_dir(), ".config", "waybar", name, NULL);
    if (!gtk_css_provider_load_from_path(provider, path, NULL))
        gtk_css_provider_load_from_data(provider, fallback, -1, NULL);
    g_free(path);
}

int main(void) {
    static const char css[] =
        "window { background-color: rgba(0, 0, 0, 0); }"
        "#panel { background-color: #1e1e2e; border-radius: 8px; padding: 12px; }"
        "#time { color: #ffffff; font-size: 28px; font-weight: bold; }"
        "#date { color: #ffffff; font-size: 14px; }"
        "#meta { color: #c8c8d0; font-size: 13px; }"
        "calendar { color: #ffffff; background-color: #1e1e2e; }"
        "calendar:selected { color: #ffffff; background-color: #64727D; }";
    GtkWidget *window, *backdrop, *panel, *box, *calendar;
    GtkLabel *labels[3];
    GtkCssProvider *provider;
    GdkScreen *screen;
    GdkVisual *visual;
    const char *names[] = {"time", "date", "meta"};
    char *iana;

    hold_lock();
    gtk_init(NULL, NULL);

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
    gtk_widget_set_halign(panel, GTK_ALIGN_START);
    gtk_widget_set_valign(panel, GTK_ALIGN_START);
    gtk_widget_set_margin_top(panel, 36);
    gtk_widget_set_margin_start(panel, 12);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    iana = zone_name();
    for (int i = 0; i < 3; i++) {
        labels[i] = GTK_LABEL(gtk_label_new(""));
        gtk_widget_set_name(GTK_WIDGET(labels[i]), names[i]);
        gtk_label_set_xalign(labels[i], 0);
        gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(labels[i]), FALSE, FALSE, 0);
    }
    g_object_set_data_full(G_OBJECT(labels[0]), "iana", iana, g_free);

    calendar = gtk_calendar_new();
    gtk_calendar_set_display_options(
        GTK_CALENDAR(calendar),
        GTK_CALENDAR_SHOW_HEADING | GTK_CALENDAR_SHOW_DAY_NAMES | GTK_CALENDAR_SHOW_WEEK_NUMBERS);
    gtk_box_pack_start(GTK_BOX(box), calendar, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(panel), box);
    gtk_container_add(GTK_CONTAINER(backdrop), panel);
    gtk_container_add(GTK_CONTAINER(window), backdrop);

    provider = gtk_css_provider_new();
    load_theme(provider, "popup-clock.css", css);
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    refresh(labels);
    g_timeout_add_seconds(1, refresh, labels);
    g_signal_connect(window, "destroy", G_CALLBACK(quit), NULL);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key), NULL);
    g_signal_connect(backdrop, "button-press-event", G_CALLBACK(on_outside), NULL);
    gtk_widget_show_all(window);
    gtk_main();
    if (lock_fd >= 0) close(lock_fd);
    return 0;
}
