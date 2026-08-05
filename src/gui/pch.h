#ifndef RASPIMON_GUI_PCH_H_
#define RASPIMON_GUI_PCH_H_

// The GUI frontend's precompiled header. Hardware access comes from
// libraspimon (whose own pch also defines `is_debug` - do not redefine it
// here, both pchs end up in every GUI translation unit)

// C++ Runtime Headers
#include <array>     // std::array
#include <cstdlib>   // EXIT_SUCCESS / EXIT_FAILURE
#include <cstring>   // std::strcmp()
#include <iomanip>   // std::setw(), std::setprecision()
#include <iostream>  // std::cerr (debug/error output)
#include <optional>  // std::optional
#include <sstream>   // std::ostringstream / std::istringstream
#include <stdexcept> // std::exception, for the Mbox constructor throw
#include <string>    // std::string
#include <utility>   // std::pair
#include <vector>    // std::vector

// GTK headers (we target the plain GTK3 C API for maximum compatibility;
// gtkmm, the C++ wrapper, is a separate much heavier dependency)
#include <gdk/gdkkeysyms.h> // GDK_KEY_* codes for menu accelerators
#include <gtk/gtk.h>        // Baseline GTK header

#endif // RASPIMON_GUI_PCH_H_
