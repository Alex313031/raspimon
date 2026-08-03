// Copyright (c) 2026 Alex313031.

// Provides Linux GTK3 helper functions, and UI utility functions for
// raspimon-gui

#include "gui_utils.h"

GtkWidget* BuildMenuBar() {
  GtkWidget* bar = gtk_menu_bar_new();
  constexpr std::array<const char*, 3> kMenus{"File", "Options", "About"};
  for (const char* name : kMenus) {
    GtkWidget* item = gtk_menu_item_new_with_label(name);
    // Attach an empty submenu so each title drops down (to nothing, for
    // now); menu entries come later
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), gtk_menu_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(bar), item);
  }
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
