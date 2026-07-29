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
  constexpr std::array<Sensor, 12> kClocks{{
      {"CPU", "arm"},
      {"GPU", "core"},
      {"V3D", "v3d"},
      {"H264", "h264"},
      {"Image Sensor", "isp"},
      {"UART", "uart"},
      {"PWM", "pwm"},
      {"eMMC", "emmc"},
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

  // Memory regions queried with `get_mem`
  constexpr std::array<Sensor, 2> kMem{{{"CPU", "arm"}, {"GPU", "gpu"}}};

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
      GetWidestLabel(kMem, GetWidestLabel(kVolts, GetWidestLabel(kClocks, cstrlen("SOC")))) + 1);
} // namespace

bool GetInfo(const Mbox& mbox) {
  std::ostringstream out;

  PrintOutHeader(out, "Clock Frequencies");
  for (const Sensor& clock : kClocks) {
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
    // Default stream formatting trims trailing zeros:
    // "1.1000V" -> "1.1V", "1.2250V" -> "1.225V"
    std::ostringstream value;
    value << *volts;
    std::string text = value.str();
    // But always keep at least one decimal place: "1" -> "1.0"
    if (text.find('.') == std::string::npos) {
      text += ".0";
    }
    PrintOutEntry(out, rail.label, text + "V", kLabelWidth);
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
  for (const Sensor& region : kMem) {
    const std::optional<std::string> mem = QueryCmd(mbox, std::string("get_mem ") + region.arg);
    if (!mem) {
      return false;
    }
    const std::optional<long long> megabytes = ParseInt(*mem);
    if (!megabytes) {
      return false;
    }
    PrintOutEntry(out, region.label, std::to_string(*megabytes) + "MB", kLabelWidth);
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
