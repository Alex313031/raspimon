#ifndef RASPIMON_GUI_UTILS_H_
#define RASPIMON_GUI_UTILS_H_

#include "pch.h"

// Builds the classic in-window ("Win32 style") menu bar:
//   File    > Exit        (Ctrl+Q)  -> `on_exit`, "activate" signal
//   Options > Fahrenheit  (toggle)  -> `on_fahrenheit`, "toggled" signal
//   About   > About       (F1)      -> `on_about`, "activate" signal
// `accel_group` must already be added to the window (that's what routes
// the keyboard shortcuts); every callback receives `user_data`
GtkWidget* BuildMenuBar(GtkAccelGroup* accel_group,
                        GCallback on_exit,
                        GCallback on_fahrenheit,
                        GCallback on_about,
                        gpointer user_data);

// Registers the dashboard's text color tags on `buffer`. Roles mirror the
// console frontend's ANSI palette; colors are chosen to stay readable on
// both light and dark GTK themes
void CreateDashboardTags(GtkTextBuffer* buffer);

// Appends `text` to the end of `buffer` styled with `tag` ("header",
// "label", "value", "warn", "alert"), or unstyled when tag is nullptr
void AppendTagged(GtkTextBuffer* buffer, const std::string& text, const char* tag);

#endif // RASPIMON_GUI_UTILS_H_
