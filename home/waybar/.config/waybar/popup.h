#ifndef POPUP_H
#define POPUP_H

#include <gtk-layer-shell.h>
#include <gtk/gtk.h>
#include <stdio.h>

/* The bar is a vertical island on the left edge; popups sit beside it. */
#define POPUP_LEFT 76
#define POPUP_EDGE 12
#define POPUP_ANIM_US 180000
#define POPUP_ANIM_RISE 12

typedef struct {
    gint64 start;
} PopupAnim;

static gboolean popup_anim_tick(GtkWidget *panel, GdkFrameClock *clock, gpointer data) {
    PopupAnim *anim = data;
    gint64 now = gdk_frame_clock_get_frame_time(clock);
    double t;

    if (anim->start == 0) anim->start = now;
    t = (double)(now - anim->start) / POPUP_ANIM_US;
    if (t > 1.0) t = 1.0;
    t = 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);
    gtk_widget_set_opacity(panel, t);
    gtk_widget_set_margin_start(panel, POPUP_LEFT - (int)((1.0 - t) * POPUP_ANIM_RISE));
    return t < 1.0 ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
}

/* Fade the panel in while it slides out of the bar. */
static void popup_animate(GtkWidget *panel) {
    gtk_widget_set_opacity(panel, 0.0);
    gtk_widget_set_margin_start(panel, POPUP_LEFT - POPUP_ANIM_RISE);
    gtk_widget_add_tick_callback(panel, popup_anim_tick, g_new0(PopupAnim, 1), g_free);
}

static int popup_cursor(int *x, int *y) {
    gchar *out = NULL;
    int ok = 0;

    if (g_spawn_command_line_sync("hyprctl cursorpos", &out, NULL, NULL, NULL) && out != NULL)
        ok = sscanf(out, "%d, %d", x, y) == 2 || sscanf(out, "%d,%d", x, y) == 2;
    g_free(out);
    return ok;
}

/* Put the panel beside the bar on the monitor under the cursor, growing away from it:
 * downward from the cursor in the upper half, upward in the lower half. */
static void popup_place(GtkWindow *window, GtkWidget *panel) {
    GdkDisplay *display = gdk_display_get_default();
    int x = 0;
    int y = 0;

    gtk_layer_set_namespace(window, "genos-popup");
    gtk_widget_set_halign(panel, GTK_ALIGN_START);
    gtk_widget_set_valign(panel, GTK_ALIGN_START);
    gtk_widget_set_margin_start(panel, POPUP_LEFT);
    gtk_widget_set_margin_top(panel, POPUP_EDGE);
    if (display == NULL || !popup_cursor(&x, &y)) return;

    GdkMonitor *monitor = gdk_display_get_monitor_at_point(display, x, y);
    GdkRectangle geom;
    int rel;

    if (monitor == NULL) return;
    gtk_layer_set_monitor(window, monitor);
    gdk_monitor_get_geometry(monitor, &geom);
    rel = y - geom.y;
    if (rel < geom.height / 2) {
        gtk_widget_set_margin_top(panel, rel > POPUP_EDGE + 24 ? rel - 24 : POPUP_EDGE);
    } else {
        int below = geom.height - rel - 24;
        gtk_widget_set_valign(panel, GTK_ALIGN_END);
        gtk_widget_set_margin_top(panel, 0);
        gtk_widget_set_margin_bottom(panel, below > POPUP_EDGE ? below : POPUP_EDGE);
    }
}

/* Rows start hidden and fade in one after another (CSS `row.in` shows them). */
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

static inline void popup_cascade(GtkWidget *list) {
    GList *rows = gtk_container_get_children(GTK_CONTAINER(list));
    guint delay = 20;

    for (GList *item = rows; item != NULL; item = item->next, delay += 40) {
        GWeakRef *ref = g_new0(GWeakRef, 1);
        g_weak_ref_init(ref, item->data);
        g_timeout_add_full(G_PRIORITY_DEFAULT, delay, reveal_row, ref, free_weak);
    }
    g_list_free(rows);
}

#endif
