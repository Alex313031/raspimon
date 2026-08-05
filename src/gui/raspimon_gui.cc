// Copyright (c) 2026 Alex313031

// raspimon-gui: A small GTK3 GUI system monitor for Raspberry Pi
//
// A windowed "copy" of the console frontend: the same dashboard the
// terminal shows, rendered into a GtkTextView with color tags instead of
// ANSI escapes, refreshed by a GLib timer instead of a sleep loop. All
// hardware access comes from libraspimon. For the Win32 crowd: gtk_main()
// is the message pump, g_timeout_add() is SetTimer(), signals connected
// with g_signal_connect() are the window procedure's message cases.

#include "raspimon_gui.h"

#include "gui_utils.h"

// Whether to display temperatures in Fahrenheit (Options > Fahrenheit)
static bool use_fahrenheit = false;

namespace {
  // Clocks queried with `measure_clock`
  constexpr std::array<Sensor, 13> kClocks{{
      {"CPU", "arm"},
      {"GPU", "core"},
      {"V3D", "v3d"},
      {"H264", "h264"},
      {"Image Sensor", "isp"},
      {"UART", "uart"},
      {"PWM", "pwm"},
      {"eMMC", "emmc"},
      {"eMMC2", "emmc2"},
      {"Display", "pixel"},
      {"Vid Enc.", "vec"},
      {"HDMI", "hdmi"},
      {"DPI", "dpi"},
  }};

  // Voltage rails queried with `measure_volts`
  constexpr std::array<Sensor, 4> kVolts{{
      {"CPU", "core"},
      {"RAM Controller", "sdram_c"},
      {"RAM I/O", "sdram_i"},
      {"RAM PHY", "sdram_p"},
  }};

  // PMIC voltage rails to display on the Pi 5, as {display label, rail
  // name in the pmic_read_adc response}, in display order: CPU and RAM
  // lead the section to mirror the measure_volts layout on older boards.
  // (VDD2 feeds the LPDDR4X RAM core, VDDQ feeds its I/O lines)
  constexpr std::array<Sensor, 14> kPmicRails{{
      {"CPU", "VDD_CORE_V"},
      {"RAM DDR", "DDR_VDD2_V"},
      {"RAM I/O", "DDR_VDDQ_V"},
      {"3.3V System", "3V3_SYS_V"},
      {"1.8V System", "1V8_SYS_V"},
      {"1.1V System", "1V1_SYS_V"},
      {"0.8V Switched", "0V8_SW_V"},
      {"0.8V Always-On", "0V8_AON_V"},
      {"WiFi", "3V7_WL_SW_V"},
      {"Video DAC", "3V3_DAC_V"},
      {"ADC", "3V3_ADC_V"},
      {"HDMI", "HDMI_V"},
      {"5V Input", "EXT5V_V"},
      {"RTC Battery", "BATT_V"},
  }};

  // Returns the wider of `width` and the widest label in `sensors`
  template <size_t N>
  constexpr size_t GetWidestLabel(const std::array<Sensor, N>& sensors, size_t width) {
    for (const Sensor& sensor : sensors) {
      const size_t len = cstrlen(sensor.label);
      if (len > width) {
        width = len;
      }
    }
    return width;
  }

  // Label column width: the widest label across all sensor tables plus one
  // space before the ':', so columns stay aligned when labels change. The
  // seed is the widest label printed outside the tables ("Fan Speed",
  // "SOC Temp", "CPU", "GPU")
  constexpr int kLabelWidth = static_cast<int>(
      GetWidestLabel(kPmicRails,
                     GetWidestLabel(kVolts, GetWidestLabel(kClocks, cstrlen("Fan Speed")))) +
      1);

  // Clock rows to skip because a given generation doesn't have the
  // hardware: the Pi 5 moved PWM into the RP1 I/O chip, dropped the H264
  // encoder block, and replaced the VideoCore ISP with its own PiSP (the
  // firmware's isp clock just echoes the core clock there); emmc2, the
  // Pi 4's real SD-card controller, only answers on a Pi 4
  bool SkipClock(const Sensor& clock) {
    if (IsPi5() && (std::strcmp(clock.arg, "pwm") == 0 || std::strcmp(clock.arg, "h264") == 0 ||
                    std::strcmp(clock.arg, "isp") == 0)) {
      return true;
    }
    if (!IsPi4() && std::strcmp(clock.arg, "emmc2") == 0) {
      return true;
    }
    return false;
  }

  // Voltages print with exactly three decimals, rounded:
  // 0.805792 -> "0.806 V.", 1.1 -> "1.100 V."
  std::string FormatVolts(double volts) {
    std::ostringstream value;
    value << std::fixed << std::setprecision(3) << volts;
    return value.str() + " V.";
  }

  // GUI analogs of the console's PrintOutHeader/PrintOutEntry: same text,
  // same column alignment (the view uses a monospace font), but styled
  // with buffer tags instead of ANSI escapes
  void AppendHeader(GtkTextBuffer* buffer, const std::string& title) {
    std::string line = "--------" + title;
    line.resize(33, '-');
    AppendTagged(buffer, line + "\n", "header");
  }

  void AppendEntry(GtkTextBuffer* buffer,
                   const std::string& name,
                   const std::string& value,
                   const char* value_tag = "value") {
    std::ostringstream label;
    label << "  " << std::left << std::setw(kLabelWidth) << name << ": ";
    AppendTagged(buffer, label.str(), "label");
    AppendTagged(buffer, value + "\n", value_tag);
  }

  // The Pi 5 manages power with a dedicated PMIC chip, and the old
  // measure_volts rails don't exist there. Same parsing as the console
  // frontend: collect the volt() lines, print known rails in kPmicRails
  // order with friendly labels, then any unknown ones
  bool AppendPmicVoltages(GtkTextBuffer* buffer, const Mbox& mbox) {
    const std::optional<std::string> response = mbox.VideoCoreGenCommand("pmic_read_adc");
    if (!response) {
      return false;
    }
    std::vector<std::pair<std::string, double>> rails;
    std::istringstream lines(*response);
    std::string line;
    while (std::getline(lines, line)) {
      const size_t volt = line.find(" volt(");
      if (volt == std::string::npos) {
        continue; // a current(n) line, or blank
      }
      const size_t eq = line.find('=', volt);
      if (eq == std::string::npos) {
        continue;
      }
      const std::optional<double> volts = ParseDouble(line.substr(eq + 1));
      if (!volts) {
        continue;
      }
      std::string name = line.substr(0, volt);
      name.erase(0, name.find_first_not_of(' '));
      rails.emplace_back(name, *volts);
    }

    bool found_any = false;
    std::vector<bool> shown(rails.size(), false);
    for (const Sensor& rail : kPmicRails) {
      for (size_t i = 0; i < rails.size(); ++i) {
        if (!shown[i] && rails[i].first == rail.arg) {
          AppendEntry(buffer, rail.label, FormatVolts(rails[i].second));
          shown[i]  = true;
          found_any = true;
          break;
        }
      }
    }
    for (size_t i = 0; i < rails.size(); ++i) {
      if (shown[i]) {
        continue;
      }
      std::string label = rails[i].first;
      if (label.size() > 2 && label.compare(label.size() - 2, 2, "_V") == 0) {
        label.resize(label.size() - 2);
      }
      AppendEntry(buffer, label, FormatVolts(rails[i].second));
      found_any = true;
    }
    return found_any;
  }
} // namespace

bool RenderDashboard(GtkTextBuffer* buffer, const Mbox& mbox) {
  // Unlike the terminal there is no flicker to fight: the buffer is
  // rebuilt from scratch and GTK repaints once, on its own schedule
  gtk_text_buffer_set_text(buffer, "", -1);

  AppendHeader(buffer, "Model");
  AppendTagged(buffer, "  " + GetPiModelName() + "\n", "value");

  AppendHeader(buffer, "Clock Frequencies");
  for (const Sensor& clock : kClocks) {
    if (SkipClock(clock)) {
      continue;
    }
    const std::optional<std::string> hertz =
        QueryCmd(mbox, std::string("measure_clock ") + clock.arg);
    if (!hertz) {
      return false;
    }
    const std::optional<long long> freq = ParseInt(*hertz);
    if (!freq) {
      return false;
    }
    // Default stream formatting trims trailing zeros: 600 -> "600",
    // 700.5 -> "700.5" (std::to_string on a double would print "600.000000")
    std::ostringstream mhz;
    mhz << (*freq / 1000000.0);
    AppendEntry(buffer, clock.label, mhz.str() + " MHz.");
  }

  AppendHeader(buffer, "Voltages");
  if (IsPi5()) {
    if (!AppendPmicVoltages(buffer, mbox)) {
      return false;
    }
  } else {
    for (const Sensor& rail : kVolts) {
      const std::optional<std::string> response =
          QueryCmd(mbox, std::string("measure_volts ") + rail.arg);
      if (!response) {
        return false;
      }
      const std::optional<double> volts = ParseDouble(*response);
      if (!volts) {
        return false;
      }
      AppendEntry(buffer, rail.label, FormatVolts(*volts));
    }
  }

  AppendHeader(buffer, "Temperatures");
  const std::optional<std::string> temp = QueryCmd(mbox, "measure_temp");
  if (!temp) {
    return false;
  }
  const std::optional<double> celsius = ParseDouble(*temp);
  if (!celsius) {
    return false;
  }
  // Same thresholds as the console: warn from 60C, alert from 80C where
  // the firmware starts throttling
  const char* temp_tag = "value";
  if (*celsius >= 80.0) {
    temp_tag = "alert";
  } else if (*celsius >= 60.0) {
    temp_tag = "warn";
  }
  std::ostringstream degrees;
  degrees << std::fixed << std::setprecision(1);
  if (use_fahrenheit) {
    degrees << (*celsius * 9.0 / 5.0 + 32.0) << " " << kDegreeSymbol << "F.";
  } else {
    degrees << *celsius << " " << kDegreeSymbol << "C.";
  }
  AppendEntry(buffer, "SOC Temp", degrees.str(), temp_tag);
  if (IsPi5()) {
    // The row only appears when a fan is actually plugged in
    if (const std::optional<long long> rpm = GetFanRpm()) {
      AppendEntry(buffer, "Fan Speed", std::to_string(*rpm) + " RPM.");
    }
  }

  AppendHeader(buffer, "Memory Allocation");
  // CPU RAM comes from the kernel, not the firmware: the mailbox only
  // describes the first 1GB of address space, so get_mem arm tops out at
  // 1024MB no matter how much RAM the board has
  const std::optional<MemInfo> mem = GetKernelMemInfo();
  if (!mem) {
    return false;
  }
  AppendEntry(buffer, "CPU",
              std::to_string(mem->total_mb - mem->available_mb) + "/" +
                  std::to_string(mem->total_mb) + " MB.");
  if (IsPi5()) {
    // The Pi 5 has no static GPU memory split; the GPU allocates from
    // system RAM on demand, so no fixed number exists
    AppendEntry(buffer, "GPU", "(shared dynamic)");
  } else {
    const std::optional<std::string> gpu_mem = QueryCmd(mbox, "get_mem gpu");
    if (!gpu_mem) {
      return false;
    }
    const std::optional<long long> megabytes = ParseInt(*gpu_mem);
    if (!megabytes) {
      return false;
    }
    AppendEntry(buffer, "GPU", std::to_string(*megabytes) + " MB.");
  }
  return true;
}

namespace {
  // GTK callbacks are C function pointers, so the state they need rides
  // along through the gpointer user_data argument
  struct AppState {
    Mbox* mbox;
    GtkTextBuffer* buffer;
    GtkWindow* window;
  };

  gboolean RefreshTick(gpointer data) {
    AppState* state = static_cast<AppState*>(data);
    if (!RenderDashboard(state->buffer, *state->mbox)) {
      // Unlike the console (which exits), the GUI reports the failure in
      // the window and keeps trying - the firmware may recover
      AppendTagged(state->buffer, "\nVideoCore query failed.\n", "alert");
    }
    return G_SOURCE_CONTINUE; // keep the timer firing
  }

  // File > Exit (Ctrl+Q): ends the gtk_main() loop, which lets main()
  // fall through and return - the GTK analog of PostQuitMessage()
  void OnExit(GtkMenuItem*, gpointer) {
    gtk_main_quit();
  }

  // Options > Fahrenheit: flip the unit, then repaint immediately rather
  // than letting the stale unit sit on screen until the next timer tick
  void OnFahrenheitToggled(GtkCheckMenuItem* item, gpointer data) {
    use_fahrenheit = gtk_check_menu_item_get_active(item);
    RefreshTick(data);
  }

  // About > About (F1): the stock GTK about dialog - the GUI's own
  // version up top, the library's version reported in the comments line
  void OnAbout(GtkMenuItem*, gpointer data) {
    AppState* state       = static_cast<AppState*>(data);
    GtkWidget* dialog     = gtk_about_dialog_new();
    GtkAboutDialog* about = GTK_ABOUT_DIALOG(dialog);
    gtk_about_dialog_set_logo(about, nullptr); // No icon for now.
    gtk_about_dialog_set_program_name(about, "Raspimon GUI");
    gtk_about_dialog_set_version(about, "Version " RASPIMON_GUI_VERSION);
    // Literal pasting ("a" "b") only works between literals; the library
    // version arrives at runtime, so join with std::string instead
    const std::string comments =
        std::string("A small system monitor for Raspberry Pi.\n") +
#ifdef LIBRASPIMON_SHARED
        std::string("Using (shared) libraspimon v") +
#else
        std::string("Built with libraspimon v") +
#endif
        GetLibRaspiMonVersion();
    gtk_about_dialog_set_comments(about, comments.c_str());
    gtk_about_dialog_set_license_type(about, GTK_LICENSE_BSD_3);
    gtk_about_dialog_set_website(about, "https://github.com/Alex313031/raspimon");
    gtk_about_dialog_set_copyright(about, "Copyright © " COPYRIGHT_YEAR " Alex313031.");
    // Transient-for centers the dialog over the main window and keeps it
    // on top of it, like a Win32 owned window
    gtk_window_set_transient_for(GTK_WINDOW(dialog), state->window);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
  }
} // namespace

int main(int argc, char* argv[]) {
  gtk_init(&argc, &argv);

  // Heap-allocated and deliberately never freed: the mailbox must outlive
  // gtk_main(), and the OS reclaims the fd at process exit anyway
  Mbox* mbox = nullptr;
  try {
    mbox = new Mbox();
  } catch (const std::exception& error) {
    GtkWidget* dialog = gtk_message_dialog_new(nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE, "%s", error.what());
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    return EXIT_FAILURE;
  }
  // Fetch the board revision once so the IsPi*() helpers and the model
  // line know what hardware this is
  if (const std::optional<unsigned int> revision = mbox->GetBoardRevision()) {
    SetBoardRevision(*revision);
  }

  GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "Raspimon GUI v" RASPIMON_GUI_VERSION);
  gtk_window_set_default_size(GTK_WINDOW(window), CW_WIDTH, CW_HEIGHT); // resizable by default
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);

  GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add(GTK_CONTAINER(window), vbox);

  // The dashboard: a read-only monospace text view in a scrolled window,
  // so shrinking the window scrolls instead of truncating
  GtkWidget* scroller = gtk_scrolled_window_new(nullptr, nullptr);
  GtkWidget* view     = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
  gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), FALSE);
  gtk_text_view_set_monospace(GTK_TEXT_VIEW(view), TRUE);
  gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 8);
  gtk_container_add(GTK_CONTAINER(scroller), view);

  GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
  CreateDashboardTags(buffer);

  // Static so the pointer handed to GLib callbacks stays valid for the
  // program's whole life
  static AppState state{mbox, buffer, GTK_WINDOW(window)};

  // The accel group is what routes Ctrl+Q / F1 to the menu items; the
  // menu bar is packed first so it sits above the dashboard
  GtkAccelGroup* accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);
  GtkWidget* menu_bar = BuildMenuBar(accel_group, G_CALLBACK(OnExit),
                                     G_CALLBACK(OnFahrenheitToggled), G_CALLBACK(OnAbout), &state);
  gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), scroller, TRUE, TRUE, 0);

  // First frame immediately, then one per delay
  RefreshTick(&state);
  g_timeout_add(kDefaultDelayMs, RefreshTick, &state);

  gtk_widget_show_all(window);
  gtk_main();
  return EXIT_SUCCESS;
}
