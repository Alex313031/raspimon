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

namespace {

// Refresh delay, in ms. (1 sec. default)
static constexpr std::chrono::milliseconds kDefaultDelayMs{kDefaultDelay};

// Whether to display temperatures in Fahrenheit (-f)
bool use_fahrenheit = false;

// The display is drawn with ANSI/VT escape sequences: control codes
// written to stdout that the terminal interprets instead of printing
// (Windows Terminal and modern conhost understand the same codes, as
// enabled by ENABLE_VIRTUAL_TERMINAL_PROCESSING). "\033" is ESC; the ones
// used here are:
//   ESC[?25l hide cursor    ESC[2J clear whole screen   ESC[H cursor home
//   ESC[K    erase to end of line                       ESC[?25h show cursor
//   ESC[J    erase from cursor to end of screen
//
// kEndLine ends a line with erase-to-end-of-line before the newline, so
// values that got shorter since the last frame (e.g. "1.875V" -> "1V")
// don't leave leftover characters on screen
constexpr char kEndLine[] = "\033[K\n";

// A sensor to display, as {display label, gencmd argument}
struct Sensor {
  const char *label;
  const char *arg;
};

// Clocks queried with `measure_clock`
constexpr std::array<Sensor, 12> kClocks{{
  {"CPU", "arm"}, {"GPU", "core"}, {"V3D", "v3d"}, {"H264", "h264"},
  {"Image Sensor", "isp"}, {"UART", "uart"}, {"PWM", "pwm"}, {"eMMC", "emmc"},
  {"Display", "pixel"}, {"Vid Enc.", "vec"}, {"HDMI", "hdmi"}, {"DPI", "dpi"},
}};

// Voltage rails queried with `measure_volts`
constexpr std::array<Sensor, 4> kVolts{{
  {"CPU", "core"}, {"RAM Controller", "sdram_c"},
  {"RAM I/O", "sdram_i"}, {"RAM PHY", "sdram_p"},
}};

// Memory regions queried with `get_mem`
constexpr std::array<Sensor, 2> kMem{{{"CPU", "arm"}, {"GPU", "gpu"}}};

// Compile-time strlen, for computing the label column width
constexpr size_t const_strlen(const char *in) {
  size_t len = 0;
  while (in[len] != '\0') {
    ++len;
  }
  return len;
}

// Returns the wider of `width` and the widest label in `sensors`
template <size_t N>
constexpr size_t widest_label(const std::array<Sensor, N>& sensors,
                              size_t width) {
  for (const Sensor& sensor : sensors) {
    const size_t len = const_strlen(sensor.label);
    if (len > width) {
      width = len;
    }
  }
  return width;
}

// Label column width: the widest label across all sensor tables plus one
// space before the ':', so columns stay aligned when labels change
constexpr int kLabelWidth = static_cast<int>(widest_label(kMem,
    widest_label(kVolts, widest_label(kClocks, const_strlen("SOC")))) + 1);

// Runs `command` via gencmd and returns the value after the '=' in the
// response (e.g. "frequency(48)=600000000" -> "600000000")
std::optional<std::string> query(const Mbox& mbox, const std::string& command) {
  std::optional<std::string> response = mbox.gencmd(command);
  if (!response) {
    return std::nullopt;
  }
  const size_t eq = response->find('=');
  if (eq == std::string::npos) {
    return response;
  }
  return response->substr(eq + 1);
}

// Numeric parsers for firmware responses; std::nullopt on malformed input
std::optional<long long> parse_int(const std::string& in) {
  try {
    return std::stoll(in);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::optional<double> parse_double(const std::string& in) {
  try {
    return std::stod(in);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

// Prints a section header padded with dashes to a fixed width
void print_header(std::ostream& out, const std::string& title) {
  std::string line = "--------" + title;
  line.resize(33, '-');
  out << line << kEndLine;
}

// Prints one "      name    : value" entry
void print_entry(std::ostream& out, const std::string& name,
                 const std::string& value) {
  out << "      " << std::left << std::setw(kLabelWidth) << name << ": "
      << value << kEndLine;
}

// Handles SIGINT (Ctrl+C) and SIGTERM (polite kill) - POSIX signals are
// the rough equivalent of a Win32 console control handler, except the
// handler runs by interrupting the program mid-instruction on its own
// stack. Because of that, only "async-signal-safe" functions are allowed
// here: raw syscalls like write(), but NOT std::cout (it might be halfway
// through a write, holding its internal lock, when the signal hits).
// Restores the cursor with the raw write() syscall (like WriteFile() to
// the stdout handle), then _exit() ends the process immediately without
// running destructors or flushing streams - the unsafe-in-a-handler parts
// of a normal exit()
void handle_signal(int) {
  ssize_t ret = write(STDOUT_FILENO, "\033[?25h\n", 7);
  (void)ret;
  _exit(0);
}

} // namespace

Mbox::Mbox() {
  // open a char device file used for communicating with kernel mbox driver
  // first try the more restrictive interface but fall back to full if unavailable
  //
  // On Linux, device drivers appear as special files under /dev, opened
  // with the same open() syscall as regular files (compare Win32's
  // CreateFile() on a "\\.\DeviceName" path). Needs root or membership
  // in the `video` group. O_RDONLY = read-only access mode
  constexpr std::array<const char *, 2> kDevices{"/dev/vcio_gencmd", "/dev/vcio"};
  for (const char *device : kDevices) {
    fd_ = open(device, O_RDONLY);
    if (fd_ >= 0) {
      return;
    }
  }
  throw std::runtime_error(std::string(kAppName) + ": can't open device file " +
                           kDevices.back() +
                           " (are you running on a Raspberry Pi 2/3/4/5?)");
}

Mbox::~Mbox() {
  close(fd_); // release the fd, like CloseHandle()
}

int Mbox::property(void *buf) const {
  // use ioctl to send mbox property message
  //
  // ioctl() is the catch-all "device control" syscall: the request code
  // (kIoctlMboxProperty) tells the driver behind fd_ what operation to
  // perform on `buf` - here, "hand this property message to the firmware
  // and write its response back into the same buffer". Directly analogous
  // to Win32 DeviceIoControl(handle, IOCTL_code, buffer, ...)
  int ret_val = ioctl(fd_, kIoctlMboxProperty, buf);

  if (ret_val < 0) {
    std::cerr << "ioctl_set_msg failed: " << ret_val << std::endl;
  }
  return ret_val;
}

std::optional<std::string> Mbox::gencmd(const std::string& command) const {
  // maximum length for command or response
  if (command.size() + 1 >= kMaxString) {
    std::cerr << "gencmd length too long: " << command.size() << std::endl;
    return std::nullopt;
  }

  // Build a mailbox "property message": an array of 32-bit words handed
  // to the firmware, which overwrites it in place with the response.
  //   p[0]    total message size in bytes
  //   p[1]    request/response code (0 = this is a request)
  //   p[2]    tag id (kGetGencmdResult = "execute a gencmd command")
  //   p[3]    size of the tag's value buffer in bytes
  //   p[4]    tag request/response length
  //   p[5]    value buffer word 0: gencmd error code in the response
  //   p[6..]  value buffer: command string in, response string out
  //   last    end-tag terminator (0)
  // (kMaxString >> 2) converts the 4KB value buffer size to word count
  std::array<unsigned int, (kMaxString >> 2) + 7> p{};
  size_t i = 0;
  p[i++] = 0; // size
  p[i++] = 0x00000000; // process request

  p[i++] = kGetGencmdResult; // (the tag id)
  p[i++] = static_cast<unsigned int>(kMaxString); // buffer_len
  p[i++] = 0; // request_len (set to response length)
  p[i++] = 0; // error response

  std::memcpy(&p[i], command.c_str(), command.size() + 1);
  i += kMaxString >> 2;

  p[i++] = 0x00000000; // end tag
  p[0] = static_cast<unsigned int>(i * sizeof(unsigned int)); // actual size

  if (property(p.data()) < 0) {
    return std::nullopt;
  }
  if (p[5] != 0) { // firmware error code
    return std::nullopt;
  }
  // The response is a NUL-terminated C string the firmware wrote into the
  // value buffer; strnlen() bounds the scan in case the terminator is
  // missing, and the (length-counted) std::string constructor copies it out
  const char *response = reinterpret_cast<const char *>(&p[6]);
  return std::string(response, strnlen(response, kMaxString - 1));
}

bool get_info(const Mbox& mbox) {
  std::ostringstream out;

  print_header(out, "Clock Frequencies");
  for (const Sensor& clock : kClocks) {
    const std::optional<std::string> hz =
        query(mbox, std::string("measure_clock ") + clock.arg);
    if (!hz) {
      return false;
    }
    const std::optional<long long> freq = parse_int(*hz);
    if (!freq) {
      return false;
    }
    print_entry(out, clock.label, std::to_string(*freq / 1000000) + "Mhz");
  }

  print_header(out, "Voltages");
  for (const Sensor& rail : kVolts) {
    const std::optional<std::string> response =
        query(mbox, std::string("measure_volts ") + rail.arg);
    if (!response) {
      return false;
    }
    const std::optional<double> volts = parse_double(*response);
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
    print_entry(out, rail.label, text + "V");
  }

  print_header(out, "Temperatures");
  const std::optional<std::string> temp = query(mbox, "measure_temp");
  if (!temp) {
    return false;
  }
  const std::optional<double> celsius = parse_double(*temp);
  if (!celsius) {
    return false;
  }
  std::ostringstream degrees;
  degrees << std::fixed << std::setprecision(1);
  if (use_fahrenheit) {
    degrees << (*celsius * 9.0f / 5.0f + 32.0f) << "F";
  } else {
    degrees << *celsius << "C";
  }
  print_entry(out, "SOC", degrees.str());

  print_header(out, "Memory Allocation");
  for (const Sensor& region : kMem) {
    const std::optional<std::string> mem =
        query(mbox, std::string("get_mem ") + region.arg);
    if (!mem) {
      return false;
    }
    const std::optional<long long> megabytes = parse_int(*mem);
    if (!megabytes) {
      return false;
    }
    print_entry(out, region.label, std::to_string(*megabytes) + "MB");
  }

  std::cout << out.str();
  return true;
}

void refresh_output(const Mbox& mbox, const std::chrono::milliseconds delay) {
  // Hide the cursor and clear the terminal once, then redraw in place
  // each cycle to avoid flicker: home the cursor (ESC[H), repaint the
  // frame over the old one, and erase whatever is left below it (ESC[J)
  std::cout << "\033[?25l\033[2J";
  for (;;) {
    std::cout << "\033[H";
    if (!get_info(mbox)) {
      break;
    }
    std::cout << "\033[J" << std::flush;
    std::this_thread::sleep_for(delay);
  }
  std::cout << "\033[?25h" << std::flush;
}

void show_help() {
  std::cout <<
    "Usage: " << kAppName << " [ options ]\n"
    "A small hardware monitor for Raspberry Pi.\n\n"
    "Displays clock frequencies, voltages, temperatures, and memory\n"
    "allocation at a glance, refreshing periodically.\n\n"
    "Options:\n"
    "  -t <seconds>   Refresh every <seconds> seconds (default 1)\n"
    "  -f             Display temperatures in Fahrenheit\n"
    "  -v             Show program version\n"
    "  -h             Show this help message\n";
}

int main(int argc, char *argv[]) {
  std::chrono::milliseconds delay = kDefaultDelayMs;
  int opt;

  // getopt() is the standard POSIX command-line parser (no Win32
  // equivalent - closest is manually walking argv). The "t:fvh" string
  // declares the valid options; the ':' means -t takes an argument, which
  // getopt delivers through the global `optarg`. Returns one option
  // character per call, -1 when done
  while ((opt = getopt(argc, argv, "t:fvh")) != -1) {
    switch (opt) {
      case 't': {
        double seconds = 0.0;
        try {
          seconds = std::stod(optarg);
        } catch (const std::exception&) {
          // fall through to the range check below
        }
        if (seconds <= 0) {
          std::cerr << kAppName << ": invalid refresh delay '" << optarg
                    << "'" << std::endl;
          return EXIT_FAILURE;
        }
        delay = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::duration<double>(seconds));
        break;
      }
      case 'f':
        use_fahrenheit = true;
        break;
      case 'v':
        std::cout << "\n" << kAppName << " v" << VERSION_STRING << std::endl;
        return EXIT_SUCCESS;
      case 'h':
        show_help();
        return EXIT_SUCCESS;
      default:
        show_help();
        return EXIT_FAILURE;
    }
  }

  try {
    // Opens /dev/vcio here; the destructor closes it on any path out of
    // this block (including exceptions), so there is no cleanup code
    const Mbox mbox;
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);
    refresh_output(mbox, delay);
  } catch (const std::exception& error) {
    std::cerr << error.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
