// Copyright (c) 2026 Alex313031

// raspimon: A small system monitor for Raspberry Pi

#include "raspimon.h"

namespace {

// Whether to display temperatures in Fahrenheit (-f)
bool use_fahrenheit = false;

// A sensor to display, as {display label, gencmd argument}
struct Sensor {
  const char *label;
  const char *arg;
};

// Clocks queried with `measure_clock`
constexpr std::array<Sensor, 12> kClocks{{
  {"CPU", "arm"}, {"GPU", "core"}, {"h264", "h264"}, {"isp", "isp"},
  {"v3d", "v3d"}, {"uart", "uart"}, {"pwm", "pwm"}, {"emmc", "emmc"},
  {"pixel", "pixel"}, {"vec", "vec"}, {"hdmi", "hdmi"}, {"dpi", "dpi"},
}};

// Voltage rails queried with `measure_volts`
constexpr std::array<Sensor, 4> kVolts{{
  {"core", "core"}, {"sdram_c", "sdram_c"},
  {"sdram_i", "sdram_i"}, {"sdram_p", "sdram_p"},
}};

// Memory regions queried with `get_mem`
constexpr std::array<Sensor, 2> kMem{{{"CPU", "arm"}, {"GPU", "gpu"}}};

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
  out << line << "\n";
}

// Prints one "      name    : value" entry
void print_entry(std::ostream& out, const std::string& name,
                 const std::string& value) {
  out << "      " << std::left << std::setw(8) << name << ": " << value << "\n";
}

// Restores the cursor on Ctrl+C so the terminal is left usable.
// Only async-signal-safe calls allowed here.
void handle_signal(int) {
  ssize_t ret = write(STDOUT_FILENO, "\033[?25h\n", 7);
  (void)ret;
  _exit(0);
}

} // namespace

Mbox::Mbox() {
  // open a char device file used for communicating with kernel mbox driver
  // first try the more restrictive interface but fall back to full if unavailable
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
  close(fd_);
}

int Mbox::property(void *buf) const {
  // use ioctl to send mbox property message
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
    value << *volts << "V";
    print_entry(out, rail.label, value.str());
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
    degrees << (*celsius * 9.0 / 5.0 + 32.0) << "F";
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
  // each cycle to avoid flicker
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
  std::chrono::milliseconds delay = kDefaultDelay;
  int opt;

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
