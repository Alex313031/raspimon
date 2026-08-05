// Copyright (c) 2026 Alex313031.

// Provides Linux GTK3 helper functions, and UI utility functions for
// raspimon-gui

#include "gui_utils.h"

namespace {
  // Makes one top-level menu ("File") on `bar` and returns its (empty)
  // submenu for items to be appended to
  GtkWidget* AddMenu(GtkWidget* bar, const char* title) {
    GtkWidget* menu = gtk_menu_new();
    GtkWidget* item = gtk_menu_item_new_with_label(title);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(bar), item);
    return menu;
  }
} // namespace

GtkWidget* BuildMenuBar(GtkAccelGroup* accel_group,
                        GCallback on_exit,
                        GCallback on_fahrenheit,
                        GCallback on_about,
                        gpointer user_data) {
  GtkWidget* bar = gtk_menu_bar_new();

  // File > Exit (Ctrl+Q). gtk_widget_add_accelerator() both routes the
  // keystroke to the item's "activate" signal and renders the shortcut
  // right-aligned in the menu (GTK_ACCEL_VISIBLE), Win32 style
  GtkWidget* file_menu = AddMenu(bar, "File");
  GtkWidget* exit_item = gtk_menu_item_new_with_label("Exit");
  gtk_widget_add_accelerator(exit_item, "activate", accel_group, GDK_KEY_q, GDK_CONTROL_MASK,
                             GTK_ACCEL_VISIBLE);
  g_signal_connect(exit_item, "activate", on_exit, user_data);
  gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), exit_item);

  // Options > Fahrenheit: a check item, GTK's menu checkbox - it tracks
  // its own checked state and emits "toggled" on every flip
  GtkWidget* options_menu    = AddMenu(bar, "Options");
  GtkWidget* fahrenheit_item = gtk_check_menu_item_new_with_label("Fahrenheit");
  g_signal_connect(fahrenheit_item, "toggled", on_fahrenheit, user_data);
  gtk_menu_shell_append(GTK_MENU_SHELL(options_menu), fahrenheit_item);

  // About > About (F1) - function keys take no modifier, hence the 0,
  // which C++ makes us spell as a cast into the enum type
  GtkWidget* about_menu = AddMenu(bar, "About");
  GtkWidget* about_item = gtk_menu_item_new_with_label("About");
  gtk_widget_add_accelerator(about_item, "activate", accel_group, GDK_KEY_F1,
                             static_cast<GdkModifierType>(0), GTK_ACCEL_VISIBLE);
  g_signal_connect(about_item, "activate", on_about, user_data);
  gtk_menu_shell_append(GTK_MENU_SHELL(about_menu), about_item);

  return bar;
}

void CreateDashboardTags(GtkTextBuffer* buffer) {
  // GtkTextTags are the buffer-side analog of ANSI colors: a named bundle
  // of styling applied to ranges of text. PANGO_WEIGHT_BOLD is GTK's
  // ESC[1m. Unlike the terminal palette these tint against an unknown
  // theme background, so the colors are darker cousins of the console's
  gtk_text_buffer_create_tag(buffer, "header", "foreground", "forestgreen", "weight",
                             PANGO_WEIGHT_BOLD, nullptr);
  gtk_text_buffer_create_tag(buffer, "label", "weight", PANGO_WEIGHT_BOLD, nullptr);
  gtk_text_buffer_create_tag(buffer, "value", "foreground", "darkcyan", nullptr);
  gtk_text_buffer_create_tag(buffer, "warn", "foreground", "darkorange", nullptr);
  gtk_text_buffer_create_tag(buffer, "alert", "foreground", "red", "weight", PANGO_WEIGHT_BOLD,
                             nullptr);
}

void AppendTagged(GtkTextBuffer* buffer, const std::string& text, const char* tag) {
  GtkTextIter end;
  gtk_text_buffer_get_end_iter(buffer, &end);
  if (tag != nullptr) {
    gtk_text_buffer_insert_with_tags_by_name(buffer, &end, text.c_str(), -1, tag, nullptr);
  } else {
    gtk_text_buffer_insert(buffer, &end, text.c_str(), -1);
  }
}
