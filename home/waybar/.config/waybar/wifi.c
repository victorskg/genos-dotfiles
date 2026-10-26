#define _GNU_SOURCE

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
#include "popup.h"

#define MAX_NETS 128
#define MAX_SAVED 64

enum { JOB_REFRESH = 1, JOB_CONNECT, JOB_DISCONNECT, JOB_RADIO };

typedef struct {
    char ssid[128];
    int signal;
    int secure;
    int connected;
    int saved;
} Net;

typedef struct {
    int kind;
    int rescan;
    int radio_on;
    char ssid[128];
    char password[128];
    char device[64];
} Job;

typedef struct {
    int kind;
    int ok;
    int wifi_on;
    char device[64];
    char ssid[128];
    char message[512];
    Net nets[MAX_NETS];
    int nnets;
} Result;

typedef struct {
    GtkWidget *list;
    GtkWidget *scroll;
    GtkWidget *status;
    GtkWidget *sw;
    GtkWidget *refresh;
    GtkWidget *spinner;
    GtkWidget *pass_box;
    GtkWidget *pass_label;
    GtkWidget *pass_entry;
    GtkWidget *error;
    char pending[128];
    char device[64];
    int alive;
    int busy;
} App;

static App app;
static int lock_fd = -1;

static void copy_str(char *dst, size_t n, const char *src) {
    snprintf(dst, n, "%s", src != NULL ? src : "");
}

static gboolean on_switch(GtkSwitch *sw, gboolean state, gpointer data);
static void quit_app(void);

static void trim_inplace(char *s) {
    char *start = s;
    char *end;
    while (*start == ' ' || *start == '\n' || *start == '\r' || *start == '\t') start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\n' || end[-1] == '\r' || end[-1] == '\t')) *--end = '\0';
}

static void set_message(char *dst, size_t n, const char *src) {
    size_t len;
    if (src == NULL) src = "";
    while (*src == ' ' || *src == '\n' || *src == '\r' || *src == '\t') src++;
    snprintf(dst, n, "%s", src);
    for (char *p = dst; *p != '\0'; p++) {
        if (*p == '\n' || *p == '\r') *p = ' ';
    }
    len = strlen(dst);
    while (len > 0 && (dst[len - 1] == ' ' || dst[len - 1] == '\t')) dst[--len] = '\0';
    if (strncmp(dst, "Error: ", 7) == 0) memmove(dst, dst + 7, strlen(dst + 7) + 1);
    else if (strncmp(dst, "Erro: ", 6) == 0) memmove(dst, dst + 6, strlen(dst + 6) + 1);
}

static int wants_secret(const char *msg) {
    if (msg == NULL) return 0;
    return strcasestr(msg, "secret") != NULL || strcasestr(msg, "password") != NULL ||
           strcasestr(msg, "senha") != NULL || strcasestr(msg, "psk") != NULL ||
           strcasestr(msg, "802-11-wireless-security") != NULL;
}

static int needs_password(const char *sec) {
    return strstr(sec, "WPA") != NULL || strstr(sec, "WEP") != NULL || strstr(sec, "SAE") != NULL;
}

static int cmp_net(const void *a, const void *b) {
    const Net *x = a;
    const Net *y = b;
    if (x->signal != y->signal) return y->signal - x->signal;
    return strcasecmp(x->ssid, y->ssid);
}

static void unescape_inplace(char *s) {
    char *w = s;
    for (char *r = s; *r != '\0'; r++) {
        if (*r == '\\' && r[1] != '\0') {
            *w++ = *++r;
            continue;
        }
        *w++ = *r;
    }
    *w = '\0';
}

static int split_fields(char *s, char **fields, int max) {
    int n = 1;
    char *w = s;
    fields[0] = s;
    for (char *r = s; *r != '\0'; r++) {
        if (*r == '\\' && r[1] != '\0') {
            *w++ = *++r;
            continue;
        }
        if (*r == ':' && n < max) {
            *w++ = '\0';
            fields[n++] = w;
            continue;
        }
        *w++ = *r;
    }
    *w = '\0';
    return n;
}

static int parse_networks(char *text, Net *nets, int max) {
    char *save = NULL;
    int n = 0;
    if (text == NULL) return 0;
    for (char *line = strtok_r(text, "\n", &save); line != NULL; line = strtok_r(NULL, "\n", &save)) {
        char *fields[4];
        int signal;
        int idx = -1;
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\r') line[--len] = '\0';
        if (split_fields(line, fields, 4) < 4 || fields[1][0] == '\0') continue;
        signal = atoi(fields[2]);
        if (signal < 0) signal = 0;
        if (signal > 100) signal = 100;
        for (int i = 0; i < n; i++) {
            if (strcmp(nets[i].ssid, fields[1]) == 0) {
                idx = i;
                break;
            }
        }
        if (idx < 0) {
            if (n >= max) continue;
            idx = n++;
            copy_str(nets[idx].ssid, sizeof nets[idx].ssid, fields[1]);
            nets[idx].signal = signal;
            nets[idx].secure = needs_password(fields[3]);
            nets[idx].connected = fields[0][0] == '*';
            nets[idx].saved = 0;
        } else {
            if (signal > nets[idx].signal) nets[idx].signal = signal;
            if (fields[0][0] == '*') nets[idx].connected = 1;
            if (needs_password(fields[3])) nets[idx].secure = 1;
        }
    }
    qsort(nets, (size_t)n, sizeof *nets, cmp_net);
    return n;
}

static int nm_exec(char **argv, char **out, char **err) {
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

static int load_saved(char saved[][128], int max) {
    char *argv[] = {"nmcli", "-c", "no", "-t", "-f", "NAME,TYPE", "connection", "show", NULL};
    char *out = NULL;
    char *err = NULL;
    char *save = NULL;
    int n = 0;
    if (nm_exec(argv, &out, &err) != 0) {
        g_free(out);
        g_free(err);
        return -1;
    }
    g_free(err);
    for (char *line = strtok_r(out, "\n", &save); line != NULL; line = strtok_r(NULL, "\n", &save)) {
        static const char suf[] = ":802-11-wireless";
        size_t len = strlen(line);
        char *ssid = NULL;
        char *ssid_err = NULL;
        if (len > 0 && line[len - 1] == '\r') line[--len] = '\0';
        if (len <= sizeof suf - 1 || strcmp(line + len - (sizeof suf - 1), suf) != 0 || n >= max) continue;
        line[len - (sizeof suf - 1)] = '\0';
        unescape_inplace(line);
        if (line[0] == '\0') continue;
        char *query[] = {"nmcli", "-c", "no", "-g", "802-11-wireless.ssid", "connection", "show", line, NULL};
        if (nm_exec(query, &ssid, &ssid_err) == 0 && ssid != NULL) {
            trim_inplace(ssid);
            if (ssid[0] != '\0') copy_str(saved[n++], 128, ssid);
        }
        g_free(ssid);
        g_free(ssid_err);
    }
    g_free(out);
    return n;
}

static void load_device(Result *r) {
    char *argv[] = {"nmcli", "-c", "no", "-t", "-f", "DEVICE,TYPE,STATE", "device", NULL};
    char *out = NULL;
    char *err = NULL;
    char *save = NULL;
    if (nm_exec(argv, &out, &err) != 0) {
        g_free(out);
        g_free(err);
        return;
    }
    g_free(err);
    for (char *line = strtok_r(out, "\n", &save); line != NULL; line = strtok_r(NULL, "\n", &save)) {
        char *fields[3];
        if (split_fields(line, fields, 3) < 3 || strcmp(fields[1], "wifi") != 0) continue;
        if (r->device[0] == '\0' || strncmp(fields[2], "connected", 9) == 0)
            copy_str(r->device, sizeof r->device, fields[0]);
    }
    g_free(out);
}

static void do_refresh(Job *job, Result *r) {
    char *argv[] = {"nmcli", "-c", "no", "-t", "-f", "WIFI", "radio", NULL};
    char *out = NULL;
    char *err = NULL;
    char saved[MAX_SAVED][128];
    int nsaved;
    int rc = nm_exec(argv, &out, &err);
    if (rc != 0 || out == NULL) {
        r->ok = 0;
        set_message(r->message, sizeof r->message, err != NULL ? err : "Couldn't read Wi-Fi state");
        g_free(out);
        g_free(err);
        return;
    }
    r->wifi_on = strncmp(out, "enabled", 7) == 0;
    g_free(out);
    g_free(err);
    r->ok = 1;
    if (!r->wifi_on) return;

    load_device(r);
    nsaved = load_saved(saved, MAX_SAVED);
    char *list_argv[] = {"nmcli", "-c", "no", "-t", "-f", "IN-USE,SSID,SIGNAL,SECURITY", "device", "wifi",
                         "list", "--rescan", job->rescan ? "yes" : "no", NULL};
    rc = nm_exec(list_argv, &out, &err);
    if (rc != 0) {
        r->ok = 0;
        set_message(r->message, sizeof r->message, err != NULL ? err : "Couldn't scan");
        g_free(out);
        g_free(err);
        return;
    }
    g_free(err);
    r->nnets = parse_networks(out, r->nets, MAX_NETS);
    g_free(out);
    for (int i = 0; i < r->nnets; i++) {
        if (nsaved < 0) {
            r->nets[i].saved = 1;
            continue;
        }
        for (int j = 0; j < nsaved; j++) {
            if (strcmp(saved[j], r->nets[i].ssid) == 0) {
                r->nets[i].saved = 1;
                break;
            }
        }
    }
}

static void do_connect(Job *job, Result *r) {
    char *argv[12];
    char *out = NULL;
    char *err = NULL;
    int i = 0;
    argv[i++] = "nmcli";
    argv[i++] = "-c";
    argv[i++] = "no";
    argv[i++] = "-w";
    argv[i++] = "20";
    argv[i++] = "device";
    argv[i++] = "wifi";
    argv[i++] = "connect";
    argv[i++] = job->ssid;
    if (job->password[0] != '\0') {
        argv[i++] = "password";
        argv[i++] = job->password;
    }
    argv[i] = NULL;
    copy_str(r->ssid, sizeof r->ssid, job->ssid);
    r->ok = nm_exec(argv, &out, &err) == 0;
    if (!r->ok) set_message(r->message, sizeof r->message, err != NULL && err[0] != '\0' ? err : out);
    memset(job->password, 0, sizeof job->password);
    g_free(out);
    g_free(err);
}

static void do_disconnect(Job *job, Result *r) {
    char *argv[] = {"nmcli", "-c", "no", "device", "disconnect", job->device, NULL};
    char *out = NULL;
    char *err = NULL;
    r->ok = nm_exec(argv, &out, &err) == 0;
    if (!r->ok) set_message(r->message, sizeof r->message, err != NULL && err[0] != '\0' ? err : out);
    g_free(out);
    g_free(err);
}

static void do_radio(Job *job, Result *r) {
    char *argv[] = {"nmcli", "-c", "no", "radio", "wifi", job->radio_on ? "on" : "off", NULL};
    char *out = NULL;
    char *err = NULL;
    if (nm_exec(argv, &out, &err) != 0) {
        r->ok = 0;
        r->wifi_on = !job->radio_on;
        set_message(r->message, sizeof r->message, err != NULL ? err : out);
        g_free(out);
        g_free(err);
        return;
    }
    g_free(out);
    g_free(err);
    if (!job->radio_on) {
        r->ok = 1;
        r->wifi_on = 0;
        return;
    }
    job->rescan = 1;
    do_refresh(job, r);
}

static void free_job(Job *job) {
    if (job == NULL) return;
    memset(job->password, 0, sizeof job->password);
    g_free(job);
}

static gboolean deliver(gpointer data);

static gpointer worker(gpointer data) {
    Job *job = data;
    Result *result = g_new0(Result, 1);
    result->kind = job->kind;
    switch (job->kind) {
    case JOB_REFRESH:
        do_refresh(job, result);
        break;
    case JOB_CONNECT:
        do_connect(job, result);
        break;
    case JOB_DISCONNECT:
        do_disconnect(job, result);
        break;
    case JOB_RADIO:
        do_radio(job, result);
        break;
    default:
        break;
    }
    free_job(job);
    g_idle_add(deliver, result);
    return NULL;
}

static void set_busy(int busy) {
    app.busy = busy;
    gtk_widget_set_sensitive(app.list, !busy);
    gtk_widget_set_sensitive(app.refresh, !busy);
    gtk_widget_set_sensitive(app.sw, !busy);
    gtk_widget_set_sensitive(app.pass_box, !busy);
    if (busy) {
        gtk_spinner_start(GTK_SPINNER(app.spinner));
        gtk_widget_show(app.spinner);
    } else {
        gtk_spinner_stop(GTK_SPINNER(app.spinner));
        gtk_widget_hide(app.spinner);
    }
}

static void show_error(const char *text) {
    gtk_label_set_text(GTK_LABEL(app.error), text != NULL && text[0] != '\0' ? text : "Something went wrong");
    gtk_widget_show(app.error);
}

static void hide_error(void) {
    gtk_label_set_text(GTK_LABEL(app.error), "");
    gtk_widget_hide(app.error);
}

static void hide_password(void) {
    gtk_entry_set_text(GTK_ENTRY(app.pass_entry), "");
    app.pending[0] = '\0';
    gtk_widget_hide(app.pass_box);
}

static void set_switch(int on) {
    g_signal_handlers_block_by_func(app.sw, G_CALLBACK(on_switch), NULL);
    gtk_switch_set_active(GTK_SWITCH(app.sw), on);
    g_signal_handlers_unblock_by_func(app.sw, G_CALLBACK(on_switch), NULL);
}

static void start_job(Job *job, const char *status) {
    GError *error = NULL;
    GThread *thread;
    if (!app.alive || app.busy) {
        free_job(job);
        return;
    }
    if (status != NULL) {
        gtk_label_set_text(GTK_LABEL(app.status), status);
    }
    set_busy(1);
    thread = g_thread_try_new("nm", worker, job, &error);
    if (thread == NULL) {
        set_busy(0);
        show_error(error->message);
        g_error_free(error);
        free_job(job);
        return;
    }
    g_thread_unref(thread);
}

static void start_refresh(int rescan) {
    Job *job = g_new0(Job, 1);
    job->kind = JOB_REFRESH;
    job->rescan = rescan;
    start_job(job, "Scanning...");
}

static void start_connect(const char *ssid, const char *password) {
    Job *job = g_new0(Job, 1);
    char status[192];
    job->kind = JOB_CONNECT;
    copy_str(job->ssid, sizeof job->ssid, ssid);
    copy_str(app.pending, sizeof app.pending, ssid);
    if (password != NULL) copy_str(job->password, sizeof job->password, password);
    snprintf(status, sizeof status, "Connecting to %s...", ssid);
    start_job(job, status);
}

static void ask_password(const char *ssid) {
    char text[192];
    copy_str(app.pending, sizeof app.pending, ssid);
    snprintf(text, sizeof text, "Password for %s", ssid);
    gtk_label_set_text(GTK_LABEL(app.pass_label), text);
    gtk_entry_set_text(GTK_ENTRY(app.pass_entry), "");
    gtk_widget_show_all(app.pass_box);
    gtk_widget_grab_focus(app.pass_entry);
}

static void clear_list(void) {
    GList *children = gtk_container_get_children(GTK_CONTAINER(app.list));
    for (GList *item = children; item != NULL; item = item->next) gtk_widget_destroy(GTK_WIDGET(item->data));
    g_list_free(children);
}

static const char *signal_glyph(int signal) {
    return signal >= 75 ? "\xF3\xB0\xA4\xA8" :
           signal >= 50 ? "\xF3\xB0\xA4\xA5" :
           signal >= 25 ? "\xF3\xB0\xA4\xA2" : "\xF3\xB0\xA4\x9F";
}

static GtkWidget *net_row(const Net *net) {
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *glyph = gtk_label_new(signal_glyph(net->signal));
    GtkWidget *ssid = gtk_label_new(net->ssid);
    GtkWidget *action = gtk_label_new(net->connected ? "\xF3\xB0\x8C\xB8" : "\xF3\xB0\x8C\xB7");
    char *tip = g_strdup_printf("%s\n%d%%", net->ssid, net->signal);

    gtk_widget_set_name(box, "item");
    gtk_widget_set_name(glyph, "glyph");
    gtk_widget_set_name(ssid, "ssid");
    gtk_label_set_xalign(GTK_LABEL(ssid), 0);
    gtk_label_set_ellipsize(GTK_LABEL(ssid), PANGO_ELLIPSIZE_END);
    gtk_widget_set_hexpand(ssid, TRUE);
    gtk_widget_set_name(action, "action");
    gtk_widget_set_valign(action, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(box), glyph, FALSE, FALSE, 0);
    if (net->secure) {
        GtkWidget *lock = gtk_label_new("\xF3\xB0\x8C\xBE");
        gtk_widget_set_name(lock, "lock");
        gtk_box_pack_start(GTK_BOX(box), lock, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(box), ssid, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(box), action, FALSE, FALSE, 0);
    if (net->connected) gtk_style_context_add_class(gtk_widget_get_style_context(row), "connected");
    gtk_container_add(GTK_CONTAINER(row), box);
    g_object_set_data_full(G_OBJECT(row), "ssid", g_strdup(net->ssid), g_free);
    g_object_set_data(G_OBJECT(row), "secure", GINT_TO_POINTER(net->secure));
    g_object_set_data(G_OBJECT(row), "connected", GINT_TO_POINTER(net->connected));
    g_object_set_data(G_OBJECT(row), "saved", GINT_TO_POINTER(net->saved));
    gtk_widget_set_tooltip_text(row, tip);
    g_free(tip);
    return row;
}

static gboolean reveal_row(gpointer data) {
    GtkWidget *row = g_weak_ref_get(data);

    if (row != NULL) {
        gtk_style_context_add_class(gtk_widget_get_style_context(row), "in");
        g_object_unref(row);
    }
    return G_SOURCE_REMOVE;
}

static void free_weak(gpointer data) {
    g_weak_ref_clear(data);
    g_free(data);
}

/* Rows start hidden and fade in one after another. */
static void cascade_rows(void) {
    GList *rows = gtk_container_get_children(GTK_CONTAINER(app.list));
    guint delay = 20;

    for (GList *item = rows; item != NULL; item = item->next, delay += 40) {
        GWeakRef *ref = g_new0(GWeakRef, 1);
        g_weak_ref_init(ref, item->data);
        g_timeout_add_full(G_PRIORITY_DEFAULT, delay, reveal_row, ref, free_weak);
    }
    g_list_free(rows);
}

static void set_caption(const char *text, int live) {
    GtkStyleContext *ctx = gtk_widget_get_style_context(app.status);

    gtk_label_set_text(GTK_LABEL(app.status), text);
    if (live) gtk_style_context_add_class(ctx, "live");
    else gtk_style_context_remove_class(ctx, "live");
}

static void fill_list(const Result *r) {
    char caption[64];

    clear_list();
    copy_str(app.device, sizeof app.device, r->device);
    set_switch(r->wifi_on);
    if (!r->wifi_on) {
        set_caption("Off", 0);
        gtk_widget_hide(app.scroll);
        return;
    }
    gtk_widget_show(app.scroll);
    for (int i = 0; i < r->nnets; i++) gtk_container_add(GTK_CONTAINER(app.list), net_row(&r->nets[i]));
    gtk_widget_show_all(app.list);
    cascade_rows();
    if (r->nnets == 0) {
        set_caption("No networks found", 0);
        return;
    }
    snprintf(caption, sizeof caption, "%d %s available", r->nnets, r->nnets == 1 ? "network" : "networks");
    set_caption(caption, 0);
}

static void apply(Result *r) {
    if (r->kind == JOB_CONNECT) {
        if (r->ok) {
            hide_error();
            hide_password();
            start_refresh(0);
            return;
        }
        show_error(r->message);
        if (wants_secret(r->message)) ask_password(r->ssid);
        return;
    }
    if (r->kind == JOB_DISCONNECT) {
        if (!r->ok) {
            show_error(r->message[0] != '\0' ? r->message : "Couldn't disconnect");
            return;
        }
        hide_error();
        start_refresh(0);
        return;
    }
    if (!r->ok) {
        if (r->kind == JOB_RADIO) set_switch(r->wifi_on);
        show_error(r->message[0] != '\0' ? r->message : "Couldn't scan");
        return;
    }
    if (r->kind == JOB_RADIO && !r->wifi_on) {
        quit_app();
        return;
    }
    hide_error();
    hide_password();
    fill_list(r);
}

static gboolean deliver(gpointer data) {
    Result *result = data;
    if (app.alive) {
        set_busy(0);
        apply(result);
    }
    g_free(result);
    return G_SOURCE_REMOVE;
}

static gboolean on_switch(GtkSwitch *sw, gboolean state, gpointer data) {
    Job *job = g_new0(Job, 1);
    (void)sw;
    (void)data;
    job->kind = JOB_RADIO;
    job->radio_on = state;
    start_job(job, state ? "Turning Wi-Fi on..." : "Turning Wi-Fi off...");
    return TRUE;
}

static void on_refresh(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    start_refresh(1);
}

static void on_disconnect(GtkWidget *widget, gpointer data) {
    Job *job;
    (void)widget;
    (void)data;
    if (app.device[0] == '\0') return;
    job = g_new0(Job, 1);
    job->kind = JOB_DISCONNECT;
    copy_str(job->device, sizeof job->device, app.device);
    start_job(job, "Disconnecting...");
}

static void on_row(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    const char *ssid;
    (void)box;
    (void)data;
    if (row == NULL) return;
    ssid = g_object_get_data(G_OBJECT(row), "ssid");
    if (ssid == NULL) return;
    if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "connected"))) {
        on_disconnect(NULL, NULL);
        return;
    }
    hide_error();
    if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "secure")) &&
        !GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "saved"))) {
        ask_password(ssid);
        return;
    }
    hide_password();
    start_connect(ssid, NULL);
}

static void on_submit(GtkWidget *widget, gpointer data) {
    const char *password;
    (void)widget;
    (void)data;
    if (app.pending[0] == '\0') return;
    password = gtk_entry_get_text(GTK_ENTRY(app.pass_entry));
    if (password[0] == '\0') return;
    start_connect(app.pending, password);
}

static void quit_app(void) { gtk_main_quit(); }

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
    app.alive = 0;
    quit_app();
}

static void hold_lock(void) {
    char path[PATH_MAX];
    const char *cache = g_get_user_cache_dir();

    g_mkdir_with_parents(cache, 0700);
    snprintf(path, sizeof path, "%s/waybar-wifi.lock", cache);
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

static GtkWidget *flat_button(const char *icon, const char *tip) {
    GtkWidget *button = gtk_button_new_with_label(icon);
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_widget_set_name(gtk_bin_get_child(GTK_BIN(button)), "glyph");
    gtk_widget_set_tooltip_text(button, tip);
    return button;
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
        "#panel { background-color: #1e1e2e; border-radius: 8px; padding: 10px; }"
        "#lock { font-family: \"Symbols Nerd Font\"; }"
        "#title { color: #ffffff; font-size: 14px; font-weight: bold; }"
        "#status, #meta, #pass-label, #lock { color: #c8c8d0; font-size: 12px; }"
        "#ssid { color: #ffffff; font-size: 14px; }"
        "#error { color: #f38ba8; font-size: 12px; }"
        "button { background: transparent; color: #ffffff; border: none; box-shadow: none; padding: 2px 8px; }"
        "button:hover { background-color: #313244; }"
        "button:disabled { color: #6c6c80; }"
        "list, row, scrolledwindow { background: transparent; border: none; }"
        "row { padding: 4px 6px; border-radius: 6px; }"
        "row:hover { background-color: #313244; }"
        "row.connected, row.connected:hover { background-color: #64727D; }"
        "entry { background-color: #313244; color: #ffffff; border: none; border-radius: 6px; padding: 4px 8px; caret-color: #ffffff; }";
    GtkWidget *window, *backdrop, *panel, *box, *header, *title, *caption_row, *pass_row, *connect;
    GtkCssProvider *provider;
    GdkScreen *screen;
    GdkVisual *visual;

    hold_lock();
    gtk_init(NULL, NULL);
    app.alive = 1;

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
    gtk_widget_set_size_request(panel, 340, -1);
    popup_place(GTK_WINDOW(window), panel);
    popup_animate(panel);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(box, "stack");
    header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_name(header, "header");
    title = gtk_label_new("Wi-Fi");
    gtk_widget_set_name(title, "title");
    gtk_label_set_xalign(GTK_LABEL(title), 0);
    gtk_widget_set_hexpand(title, TRUE);
    app.sw = gtk_switch_new();
    gtk_widget_set_valign(app.sw, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(header), app.sw, FALSE, FALSE, 0);

    caption_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_name(caption_row, "caption");
    app.status = gtk_label_new("Scanning...");
    gtk_widget_set_name(app.status, "status");
    gtk_label_set_xalign(GTK_LABEL(app.status), 0);
    gtk_label_set_ellipsize(GTK_LABEL(app.status), PANGO_ELLIPSIZE_END);
    app.spinner = gtk_spinner_new();
    gtk_box_pack_start(GTK_BOX(caption_row), app.status, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caption_row), app.spinner, FALSE, FALSE, 0);

    app.scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(app.scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(app.scroll), TRUE);
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(app.scroll), 360);
    gtk_widget_set_name(app.scroll, "list-wrap");
    app.list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(app.list), GTK_SELECTION_NONE);
    gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(app.list), TRUE);
    gtk_container_add(GTK_CONTAINER(app.scroll), app.list);

    app.refresh = gtk_button_new_with_label("\xF3\xB0\x91\x90  Rescan");
    gtk_widget_set_name(app.refresh, "rescan");

    app.pass_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(app.pass_box, "secret");
    app.pass_label = gtk_label_new("");
    gtk_widget_set_name(app.pass_label, "pass-label");
    gtk_label_set_xalign(GTK_LABEL(app.pass_label), 0);
    gtk_label_set_ellipsize(GTK_LABEL(app.pass_label), PANGO_ELLIPSIZE_END);
    pass_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_name(pass_row, "secret-row");
    app.pass_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(app.pass_entry), FALSE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(app.pass_entry), "Password");
    connect = flat_button("\xF3\xB0\x84\xAC", "Connect");
    gtk_box_pack_start(GTK_BOX(pass_row), app.pass_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(pass_row), connect, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(app.pass_box), app.pass_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(app.pass_box), pass_row, FALSE, FALSE, 0);

    app.error = gtk_label_new("");
    gtk_widget_set_name(app.error, "error");
    gtk_label_set_xalign(GTK_LABEL(app.error), 0);
    gtk_label_set_line_wrap(GTK_LABEL(app.error), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(app.error), 32);

    gtk_box_pack_start(GTK_BOX(box), header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), caption_row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), app.scroll, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), app.pass_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), app.error, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), app.refresh, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(panel), box);
    gtk_container_add(GTK_CONTAINER(backdrop), panel);
    gtk_container_add(GTK_CONTAINER(window), backdrop);

    provider = gtk_css_provider_new();
    load_theme(provider, "popup-wifi.css", css);
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_signal_connect(app.sw, "state-set", G_CALLBACK(on_switch), NULL);
    g_signal_connect(app.refresh, "clicked", G_CALLBACK(on_refresh), NULL);
    g_signal_connect(app.list, "row-activated", G_CALLBACK(on_row), NULL);
    g_signal_connect(connect, "clicked", G_CALLBACK(on_submit), NULL);
    g_signal_connect(app.pass_entry, "activate", G_CALLBACK(on_submit), NULL);
    g_signal_connect(window, "destroy", G_CALLBACK(on_destroy), NULL);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key), NULL);
    g_signal_connect(backdrop, "button-press-event", G_CALLBACK(on_outside), NULL);
    g_signal_connect(panel, "button-press-event", G_CALLBACK(swallow), NULL);

    gtk_widget_show_all(window);
    gtk_widget_hide(app.pass_box);
    gtk_widget_hide(app.error);
    gtk_widget_hide(app.spinner);
    start_refresh(0);
    gtk_main();
    if (lock_fd >= 0) close(lock_fd);
    return 0;
}
