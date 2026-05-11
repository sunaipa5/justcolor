#include <dbus/dbus.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  GtkWidget *color_area;
  GtkWidget *hex_entry;
  GtkWidget *rgb_entry;
  GtkWidget *status_label;
  GtkWidget *window;
  gboolean updating;
} AppWidgets;

static gboolean clear_status_bar(AppWidgets *widgets) {
  gtk_label_set_text(GTK_LABEL(widgets->status_label), "");
  return FALSE;
}

static void set_preview_color(GtkWidget *area, int r, int g, int b) {
  char css[512];
  snprintf(css, sizeof(css),
           "#preview-box { background-color: rgb(%d,%d,%d); border-radius: "
           "12px; border: 2px solid #444; }",
           r, g, b);

  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_string(provider, css);
  gtk_style_context_add_provider_for_display(
      gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);
}

static void update_all_from_rgb(AppWidgets *widgets, int r, int g, int b) {
  widgets->updating = TRUE;
  set_preview_color(widgets->color_area, r, g, b);

  char h_str[16], r_str[32];
  sprintf(h_str, "#%02X%02X%02X", r, g, b);
  sprintf(r_str, "%d, %d, %d", r, g, b);

  if (strcmp(gtk_editable_get_text(GTK_EDITABLE(widgets->hex_entry)), h_str) !=
      0)
    gtk_editable_set_text(GTK_EDITABLE(widgets->hex_entry), h_str);

  if (strcmp(gtk_editable_get_text(GTK_EDITABLE(widgets->rgb_entry)), r_str) !=
      0)
    gtk_editable_set_text(GTK_EDITABLE(widgets->rgb_entry), r_str);

  widgets->updating = FALSE;
}

static void on_hex_changed(GtkEditable *editable, AppWidgets *widgets) {
  if (widgets->updating)
    return;
  const char *text = gtk_editable_get_text(editable);
  if (text[0] == '#' && strlen(text) == 7) {
    int r, g, b;
    if (sscanf(text, "#%02x%02x%02x", &r, &g, &b) == 3) {
      update_all_from_rgb(widgets, r, g, b);
    }
  }
}

static void on_rgb_changed(GtkEditable *editable, AppWidgets *widgets) {
  if (widgets->updating)
    return;
  const char *text = gtk_editable_get_text(editable);
  int r, g, b;
  if (sscanf(text, "%d, %d, %d", &r, &g, &b) == 3) {
    if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
      update_all_from_rgb(widgets, r, g, b);
    }
  }
}

static void pick_color_dbus(AppWidgets *widgets) {
  DBusError err;
  dbus_error_init(&err);
  DBusConnection *conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
  if (!conn)
    return;

  DBusMessage *msg = dbus_message_new_method_call(
      "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
      "org.freedesktop.portal.Screenshot", "PickColor");
  const char *parent = "";
  DBusMessageIter iter, opt;
  dbus_message_iter_init_append(msg, &iter);
  dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &parent);
  dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &opt);
  dbus_message_iter_close_container(&iter, &opt);

  DBusMessage *reply =
      dbus_connection_send_with_reply_and_block(conn, msg, -1, &err);
  if (!reply)
    return;

  const char *handle;
  if (dbus_message_get_args(reply, &err, DBUS_TYPE_OBJECT_PATH, &handle,
                            DBUS_TYPE_INVALID)) {
    char match[256];
    snprintf(match, 256,
             "type='signal',path='%s',interface='org.freedesktop.portal."
             "Request',member='Response'",
             handle);
    dbus_bus_add_match(conn, match, NULL);
    while (dbus_connection_read_write_dispatch(conn, -1)) {
      DBusMessage *sig = dbus_connection_pop_message(conn);
      if (!sig)
        continue;
      if (dbus_message_is_signal(sig, "org.freedesktop.portal.Request",
                                 "Response")) {
        DBusMessageIter s_iter, d_iter, e_iter, v_iter, a_iter;
        dbus_uint32_t code;
        dbus_message_iter_init(sig, &s_iter);
        dbus_message_iter_get_basic(&s_iter, &code);
        if (code == 0 && dbus_message_iter_next(&s_iter)) {
          dbus_message_iter_recurse(&s_iter, &d_iter);
          while (dbus_message_iter_get_arg_type(&d_iter) ==
                 DBUS_TYPE_DICT_ENTRY) {
            dbus_message_iter_recurse(&d_iter, &e_iter);
            const char *key;
            dbus_message_iter_get_basic(&e_iter, &key);
            if (strcmp(key, "color") == 0) {
              dbus_message_iter_next(&e_iter);
              dbus_message_iter_recurse(&e_iter, &v_iter);
              dbus_message_iter_recurse(&v_iter, &a_iter);
              double rgb_vals[3];
              for (int i = 0; i < 3; i++) {
                dbus_message_iter_get_basic(&a_iter, &rgb_vals[i]);
                dbus_message_iter_next(&a_iter);
              }
              update_all_from_rgb(widgets, (int)(rgb_vals[0] * 255),
                                  (int)(rgb_vals[1] * 255),
                                  (int)(rgb_vals[2] * 255));
              break;
            }
            dbus_message_iter_next(&d_iter);
          }
        }
        dbus_message_unref(sig);
        break;
      }
      dbus_message_unref(sig);
    }
  }
  dbus_message_unref(reply);
}

static void on_copy_hex(GtkButton *btn, AppWidgets *widgets) {
  GdkClipboard *clipboard =
      gdk_display_get_clipboard(gdk_display_get_default());
  gdk_clipboard_set_text(
      clipboard, gtk_editable_get_text(GTK_EDITABLE(widgets->hex_entry)));
  gtk_label_set_text(GTK_LABEL(widgets->status_label),
                     "HEX copied to clipboard!");
  g_timeout_add_seconds(2, (GSourceFunc)clear_status_bar, widgets);
}

static void on_copy_rgb(GtkButton *btn, AppWidgets *widgets) {
  GdkClipboard *clipboard =
      gdk_display_get_clipboard(gdk_display_get_default());
  gdk_clipboard_set_text(
      clipboard, gtk_editable_get_text(GTK_EDITABLE(widgets->rgb_entry)));
  gtk_label_set_text(GTK_LABEL(widgets->status_label),
                     "RGB copied to clipboard!");
  g_timeout_add_seconds(2, (GSourceFunc)clear_status_bar, widgets);
}

static void activate(GtkApplication *app, gpointer user_data) {
  AppWidgets *widgets = g_new0(AppWidgets, 1);
  widgets->window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(widgets->window), "Justcolor");
  gtk_window_set_resizable(GTK_WINDOW(widgets->window), FALSE);

  GtkWidget *outer_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child(GTK_WINDOW(widgets->window), outer_vbox);

  GtkWidget *main_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 25);
  gtk_widget_set_margin_start(main_hbox, 25);
  gtk_widget_set_margin_end(main_hbox, 25);
  gtk_widget_set_margin_top(main_hbox, 25);
  gtk_widget_set_margin_bottom(main_hbox, 10);
  gtk_box_append(GTK_BOX(outer_vbox), main_hbox);

  GtkWidget *vbox_left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
  gtk_box_append(GTK_BOX(main_hbox), vbox_left);

  // HEX Section
  GtkWidget *hex_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_box_append(GTK_BOX(hex_vbox), gtk_label_new("HEX CODE"));
  GtkWidget *hex_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  widgets->hex_entry = gtk_entry_new();
  gtk_editable_set_text(GTK_EDITABLE(widgets->hex_entry), "#2D2D30");
  g_signal_connect(widgets->hex_entry, "changed", G_CALLBACK(on_hex_changed),
                   widgets);
  GtkWidget *btn_copy_hex = gtk_button_new_from_icon_name("edit-copy-symbolic");
  g_signal_connect(btn_copy_hex, "clicked", G_CALLBACK(on_copy_hex), widgets);

  gtk_box_append(GTK_BOX(hex_hbox), widgets->hex_entry);
  gtk_box_append(GTK_BOX(hex_hbox), btn_copy_hex);
  gtk_box_append(GTK_BOX(hex_vbox), hex_hbox);
  gtk_box_append(GTK_BOX(vbox_left), hex_vbox);

  // RGB Section
  GtkWidget *rgb_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_box_append(GTK_BOX(rgb_vbox), gtk_label_new("RGB VALUES"));
  GtkWidget *rgb_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  widgets->rgb_entry = gtk_entry_new();
  gtk_editable_set_text(GTK_EDITABLE(widgets->rgb_entry), "45, 45, 48");
  g_signal_connect(widgets->rgb_entry, "changed", G_CALLBACK(on_rgb_changed),
                   widgets);
  GtkWidget *btn_copy_rgb = gtk_button_new_from_icon_name("edit-copy-symbolic");
  g_signal_connect(btn_copy_rgb, "clicked", G_CALLBACK(on_copy_rgb), widgets);

  gtk_box_append(GTK_BOX(rgb_hbox), widgets->rgb_entry);
  gtk_box_append(GTK_BOX(rgb_hbox), btn_copy_rgb);
  gtk_box_append(GTK_BOX(rgb_vbox), rgb_hbox);
  gtk_box_append(GTK_BOX(vbox_left), rgb_vbox);

  // Right Panel
  GtkWidget *vbox_right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
  gtk_box_append(GTK_BOX(main_hbox), vbox_right);
  widgets->color_area = gtk_frame_new(NULL);
  gtk_widget_set_name(widgets->color_area, "preview-box");
  gtk_widget_set_size_request(widgets->color_area, 130, 130);
  set_preview_color(widgets->color_area, 45, 45, 48);
  GtkWidget *btn_pick = gtk_button_new_with_label("PICK COLOR");
  g_signal_connect_swapped(btn_pick, "clicked", G_CALLBACK(pick_color_dbus),
                           widgets);
  gtk_box_append(GTK_BOX(vbox_right), widgets->color_area);
  gtk_box_append(GTK_BOX(vbox_right), btn_pick);

  // Status Bar
  GtkWidget *status_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_margin_start(status_box, 10);
  gtk_widget_set_margin_end(status_box, 10);
  gtk_widget_set_margin_bottom(status_box, 5);
  widgets->status_label = gtk_label_new("");
  gtk_widget_set_halign(widgets->status_label, GTK_ALIGN_END);
  gtk_widget_set_hexpand(widgets->status_label, TRUE);
  gtk_widget_set_name(widgets->status_label, "status-text");

  GtkCssProvider *status_provider = gtk_css_provider_new();
  gtk_css_provider_load_from_string(
      status_provider, "#status-text { font-size: 11px; opacity: 0.6; }");
  gtk_style_context_add_provider_for_display(
      gdk_display_get_default(), GTK_STYLE_PROVIDER(status_provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  gtk_box_append(GTK_BOX(status_box), widgets->status_label);
  gtk_box_append(GTK_BOX(outer_vbox), status_box);
  gtk_window_present(GTK_WINDOW(widgets->window));
}

int main(int argc, char **argv) {
  GtkApplication *app =
      gtk_application_new("org.sunaipa.justcolor", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
