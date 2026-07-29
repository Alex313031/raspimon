// Copyright (c) 2026 Alex313031

// raspimon: A small system monitor for Raspberry Pi
//
// How it works: the Pi's VideoCore firmware exposes hardware info (clocks,
// voltages, temperature, memory split) through a "mailbox" - a message
// passing interface between the ARM cores and the firmware. The kernel
// presents it as the character device /dev/vcio, which we open like a file
// and drive with ioctl(). On Win32 this would be opening a device path
// with CreateFile() and calling DeviceIoControl() on the handle. Through
// it we send the same text commands the stock vcgencmd tool uses
// ("measure_clock arm", "measure_volts core", ...), parse the text
// responses, and redraw them as a dashboard in the terminal.

#include "raspimon.h"

#include "utils.h"

// Whether to display temperatures in Fahrenheit (-f)
static bool use_fahrenheit = false;

namespace {
  // Refresh delay, in ms. (1 sec. default)
  constexpr std::chrono::milliseconds kDefaultDelayMs{kDefaultDelay};

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
      {"RAM Core", "DDR_VDD2_V"},
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
  // space before the ':', so columns stay aligned when labels change
  constexpr int kLabelWidth = static_cast<int>(
      GetWidestLabel(kPmicRails,
                     GetWidestLabel(kVolts, GetWidestLabel(kClocks, cstrlen("SOC")))) +
      1);

  // Clock rows to skip because a given generation doesn't have the
  // hardware: the Pi 5 moved PWM into the RP1 I/O chip, dropped the H264
  // encoder block, and replaced the VideoCore ISP with its own PiSP (the
  // firmware's isp clock just echoes the core clock there); emmc2, the
  // Pi 4's real SD-card controller, only answers on a Pi 4
  bool SkipClock(const Sensor& clock) {
    if (IsPi5() &&
        (std::strcmp(clock.arg, "pwm") == 0 || std::strcmp(clock.arg, "h264") == 0 ||
         std::strcmp(clock.arg, "isp") == 0)) {
      return true;
    }
    if (!IsPi4() && std::strcmp(clock.arg, "emmc2") == 0) {
      return true;
    }
    return false;
  }

  // Voltages print with exactly three decimals, rounded:
  // 0.805792 -> "0.806V", 1.1 -> "1.100V"
  std::string FormatVolts(double volts) {
    std::ostringstream value;
    value << std::fixed << std::setprecision(3) << volts;
    return value.str() + "V";
  }

  // The Pi 5 manages power with a dedicated PMIC chip, and the old
  // measure_volts rails don't exist there. Instead the firmware answers
  // `pmic_read_adc` with the PMIC's live ADC readings, one rail per line:
  //   VDD_CORE_A current(7)=2.16880000A
  //   VDD_CORE_V volt(15)=0.71620000V
  // Prints every voltage rail (the "volt(" lines), labeled by rail name.
  // Uses VideoCoreGenCommand directly because QueryCmd would chop the
  // multi-line response at the first '='
  bool PrintPmicVoltages(std::ostream& out, const Mbox& mbox) {
    const std::optional<std::string> response = mbox.VideoCoreGenCommand("pmic_read_adc");
    if (!response) {
      return false;
    }
    // First collect every voltage line as a {rail name, volts} pair, in
    // firmware order: " VDD_CORE_V volt(15)=0.71620000V" -> {"VDD_CORE_V", 0.7162}
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

    // Print the rails kPmicRails knows in its order (CPU and RAM first),
    // with its friendly labels
    bool found_any = false;
    std::vector<bool> shown(rails.size(), false);
    for (const Sensor& rail : kPmicRails) {
      for (size_t i = 0; i < rails.size(); ++i) {
        if (!shown[i] && rails[i].first == rail.arg) {
          PrintOutEntry(out, rail.label, FormatVolts(rails[i].second), kLabelWidth);
          shown[i]  = true;
          found_any = true;
          break;
        }
      }
    }
    // Any rail the table doesn't know (say, from newer firmware) still
    // shows, after the known ones, labeled by its raw name minus the _V
    for (size_t i = 0; i < rails.size(); ++i) {
      if (shown[i]) {
        continue;
      }
      std::string label = rails[i].first;
      if (label.size() > 2 && label.compare(label.size() - 2, 2, "_V") == 0) {
        label.resize(label.size() - 2);
      }
      PrintOutEntry(out, label, FormatVolts(rails[i].second), kLabelWidth);
      found_any = true;
    }
    return found_any;
  }
} // namespace

bool GetInfo(const Mbox& mbox) {
  std::ostringstream out;

  PrintOutHeader(out, "Model");
  out << GetPiModelName() << kEndLine;

  PrintOutHeader(out, "Clock Frequencies");
  for (const Sensor& clock : kClocks) {
    if (SkipClock(clock)) {
      continue;
    }
    const std::optional<std::string> hertz = QueryCmd(mbox, std::string("measure_clock ") + clock.arg);
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
    PrintOutEntry(out, clock.label, mhz.str() + "MHz", kLabelWidth);
  }

  PrintOutHeader(out, "Voltages");
  if (IsPi5()) {
    // The measure_volts rails below don't exist on Pi 5; read the PMIC's
    // ADC instead, which reports more rails and real measured values
    if (!PrintPmicVoltages(out, mbox)) {
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
      PrintOutEntry(out, rail.label, FormatVolts(*volts), kLabelWidth);
    }
  }

  PrintOutHeader(out, "Temperatures");
  const std::optional<std::string> temp = QueryCmd(mbox, "measure_temp");
  if (!temp) {
    return false;
  }
  const std::optional<double> celsius = ParseDouble(*temp);
  if (!celsius) {
    return false;
  }
  std::ostringstream degrees;
  degrees << std::fixed << std::setprecision(1);
  if (use_fahrenheit) {
    degrees << (*celsius * 9.0 / 5.0 + 32.0) << "F";
  } else {
    degrees << *celsius << "C";
  }
  PrintOutEntry(out, "SOC", degrees.str(), kLabelWidth);

  PrintOutHeader(out, "Memory Allocation");
  // CPU RAM comes from the kernel, not the firmware: the mailbox only
  // describes the first 1GB of address space, so get_mem arm tops out at
  // 1024MB no matter how much RAM the board has
  const std::optional<MemInfo> mem = GetKernelMemInfo();
  if (!mem) {
    return false;
  }
  PrintOutEntry(out, "CPU",
                std::to_string(mem->total_mb - mem->available_mb) + "/" +
                    std::to_string(mem->total_mb) + "MB",
                kLabelWidth);
  if (IsPi5()) {
    // The Pi 5 has no static GPU memory split (gpu_mem is ignored); the
    // GPU allocates from system RAM on demand, so no fixed number exists
    PrintOutEntry(out, "GPU", "(dynamic)", kLabelWidth);
  } else {
    const std::optional<std::string> gpu_mem = QueryCmd(mbox, "get_mem gpu");
    if (!gpu_mem) {
      return false;
    }
    const std::optional<long long> megabytes = ParseInt(*gpu_mem);
    if (!megabytes) {
      return false;
    }
    PrintOutEntry(out, "GPU", std::to_string(*megabytes) + "MB", kLabelWidth);
  }

  std::cout << out.str();
  return true;
}

bool RefreshTermOutput(const Mbox& mbox, const std::chrono::milliseconds delay) {
  if (IsDebugMode()) {
    const long kDelay = delay.count();
    // stderr: on stdout the ESC[2J clear below would erase this instantly,
    // and redirecting stderr keeps it out of the dashboard entirely
    std::cerr << "Using " << kDelay << " ms. delay." << std::endl;
  }
  // Hide the cursor and clear the terminal once, then redraw in place
  // each cycle to avoid flicker: home the cursor (ESC[H), repaint the
  // frame over the old one, and erase whatever is left below it (ESC[J)
  std::cout << "\033[?25l\033[2J";
  bool ok = true;
  for (;;) {
    std::cout << "\033[H";
    ok = GetInfo(mbox);
    if (!ok) {
      break;
    }
    std::cout << "\033[J" << std::flush;
    std::this_thread::sleep_for(delay);
  }
  // Restore the cursor before reporting any error, so the message lands
  // below the dashboard instead of overwriting it
  std::cout << "\033[?25h" << std::flush;
  if (!ok) {
    std::cerr << kAppName << ": VideoCore query failed, exiting" << std::endl;
  }
  return ok;
}

void ShowHelp() {
  std::cout << "Usage: " << kAppName
            << " [ options ]\n"
               "A small hardware monitor for Raspberry Pi.\n\n"
               "Displays clock frequencies, voltages, temperatures, and memory\n"
               "allocation at a glance, refreshing periodically.\n\n"
               "Options:\n"
               "  -t, --time <seconds>   Refresh every <seconds> seconds (default 1)\n"
               "  -f, --fahrenheit       Display temperatures in Fahrenheit\n"
               "  -d, --debug            Print extra debug output to stderr\n"
               "  -v, --version          Show program version\n"
               "  -h, --help             Show this help message\n";
}

void ShowVersion() {
  static constexpr char app_ver[] = VERSION_STRING;
  std::cout << kAppName << " v" << app_ver << std::endl;
}

std::optional<int> ParseOptions(int argc, char* argv[], std::chrono::milliseconds& delay) {
  int opt;

  // getopt_long() is the GNU extension of getopt(), the standard POSIX
  // command-line parser (no Win32 equivalent - closest is manually walking
  // argv). The "t:fvh" string declares the short options: one letter per
  // flag, and a ':' after a letter means that flag requires a value, which
  // is delivered through the global `optarg` (so "t:" = "-t <seconds>").
  // Each entry in the table below maps a --long spelling to the same
  // character its short option returns, so one switch handles both
  // (--time also accepts "--time=2" and "--time 2"). Returns one option
  // character per call, '?' for anything unrecognized, -1 when done.
  //
  // To add a new flag: add its letter to the string (plus ':' if it takes
  // a value), a row in the table, a case in the switch, and a line in
  // ShowHelp()
  static constexpr struct option kLongOptions[] = {
      {"time", required_argument, nullptr, 't'},
      {"fahrenheit", no_argument, nullptr, 'f'},
      {"debug", no_argument, nullptr, 'd'},
      {"version", no_argument, nullptr, 'v'},
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0}, // all-zeros terminator marks the table's end
  };
  while ((opt = getopt_long(argc, argv, "t:fdvh", kLongOptions, nullptr)) != -1) {
    switch (opt) {
      case 't': {
        double seconds = 0.0;
        try {
          seconds = std::stod(optarg);
        } catch (const std::exception&) {
          // leave seconds at 0.0 so the range check below rejects it
        }
        if (seconds <= 0) {
          std::cerr << kAppName << ": invalid refresh delay '" << optarg << "'" << std::endl;
          return EXIT_FAILURE;
        }
        delay = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::duration<double>(seconds));
        break;
      }
      case 'd':
        want_debug = true;
        break;
      case 'f':
        use_fahrenheit = true;
        break;
      case 'v':
        ShowVersion();
        return EXIT_SUCCESS;
      case 'h':
        ShowHelp();
        return EXIT_SUCCESS;
      default:
        ShowHelp();
        return EXIT_FAILURE;
    }
  }
  return std::nullopt;
}

int main(int argc, char* argv[]) {
  std::chrono::milliseconds delay = kDefaultDelayMs;
  // A returned value means a flag already did its job (-h/-v printed) or
  // the command line was invalid; either way, quit with that exit code
  if (const std::optional<int> exit_code = ParseOptions(argc, argv, delay)) {
    return *exit_code;
  }

  try {
    // Opens /dev/vcio here; the destructor closes it on any path out of
    // this block (including exceptions), so there is no cleanup code
    const Mbox mbox;
    // Fetch the board revision once so the IsPi*() helpers and the model
    // line know what hardware this is; on failure it stays 0 (unknown) and
    // the generation-gated tweaks simply don't apply
    if (const std::optional<unsigned int> revision = mbox.GetBoardRevision()) {
      SetBoardRevision(*revision);
      if (IsDebugMode()) {
        std::cerr << "Board revision: 0x" << std::hex << *revision << std::dec << std::endl;
      }
    }
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);
    if (!RefreshTermOutput(mbox, delay)) {
      return EXIT_FAILURE;
    }
  } catch (const std::exception& error) {
    // Prefix the app name here so lower layers (like Mbox) don't need to
    // know what binary they live in
    std::cerr << kAppName << ": " << error.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
