// Copyright (c) 2026 Alex313031

// raspimon: A small system monitor for Raspberry Pi

#include "raspimon.h"

// Default refresh delay (1 second)
unsigned long kDefaultDelay = 1000UL;

// Handle to the mbox char device, shared between refreshes
static int mbox_fd = -1;

// Whether to display temperatures in Fahrenheit (-f)
static bool use_fahrenheit = false;

// Clocks queried with `measure_clock`, as {display name, clock name} pairs
static const char *kClocks[][2] = {
  {"CPU", "arm"}, {"GPU", "core"}, {"h264", "h264"}, {"isp", "isp"},
  {"v3d", "v3d"}, {"uart", "uart"}, {"pwm", "pwm"}, {"emmc", "emmc"},
  {"pixel", "pixel"}, {"vec", "vec"}, {"hdmi", "hdmi"}, {"dpi", "dpi"},
};

// Voltage rails queried with `measure_volts`
static const char *kVolts[] = {"core", "sdram_c", "sdram_i", "sdram_p"};

namespace {
  int mbox_open() {
    // open a char device file used for communicating with kernel mbox driver
    // first try the more restrictive interface but fall back to full if unavailable
    const char *devices[] = {"/dev/vcio_gencmd", "/dev/vcio"};
    for (size_t i = 0; i < sizeof devices / sizeof *devices; i++) {
      int file_desc = open(devices[i], 0);
      if (file_desc >= 0) {
        return file_desc;
      }
    }
    std::cerr << kAppName << ": can't open device file " << devices[1]
              << " (are you running on a Raspberry Pi 2/3/4/5?)" << std::endl;
    exit(-1);
  }

  void mbox_close(int file_desc) {
    close(file_desc);
  }

  int mbox_property(int file_desc, void *buf) {
    // use ioctl to send mbox property message
    int ret_val = ioctl(file_desc, IOCTL_MBOX_PROPERTY, buf);

    if (ret_val < 0) {
      std::cerr << "ioctl_set_msg failed: " << ret_val << std::endl;
    }
    return ret_val;
  }


  // Runs `command` via gencmd and returns the value after the '=' in the
  // response (e.g. "frequency(48)=600000000" -> "600000000").
  // Returns an empty string on failure.
  std::string query(const std::string& command) {
    char result[MAX_STRING] = {};
    if (gencmd(mbox_fd, command.c_str(), result, sizeof(result)) != 0) {
      return std::string();
    }
    const char *eq = strchr(result, '=');
    return eq ? std::string(eq + 1) : std::string(result);
  }
} // namespace

// Prints a section header padded with dashes to a fixed width
static void print_header(std::ostream& out, const std::string& title) {
  std::string line = "--------" + title;
  line.resize(33, '-');
  out << line << "\n";
}

// Prints one "      name    : value" entry
static void print_entry(std::ostream& out, const std::string& name,
                        const std::string& value) {
  out << "      " << std::left << std::setw(8) << name << ": " << value << "\n";
}

unsigned int gencmd(int file_desc, const char *command, char *result, int result_len) {
  int i = 0;
  unsigned int p[(MAX_STRING >> 2) + 7];
  int len = strlen(command);
  // maximum length for command or response
  if (len + 1 >= MAX_STRING) {
    std::cerr << "gencmd length too long: " << len << std::endl;
    return -1;
  }
  p[i++] = 0; // size
  p[i++] = 0x00000000; // process request

  p[i++] = GET_GENCMD_RESULT; // (the tag id)
  p[i++] = MAX_STRING; // buffer_len
  p[i++] = 0; // request_len (set to response length)
  p[i++] = 0; // error response

  memcpy(p + i, command, len + 1);
  i += MAX_STRING >> 2;

  p[i++] = 0x00000000; // end tag
  p[0] = i * sizeof *p; // actual size

  if (mbox_property(file_desc, p) < 0) {
    return -1;
  }
  result[0] = 0;
  strncat(result, (const char *)(p + 6), result_len - 1);

  return p[5];
}

bool get_info() {
  std::ostringstream out;
  char buf[32];

  print_header(out, "Clock Frequencies");
  for (size_t i = 0; i < sizeof kClocks / sizeof *kClocks; i++) {
    std::string hz = query(std::string("measure_clock ") + kClocks[i][1]);
    if (hz.empty()) {
      return false;
    }
    long long mhz = strtoll(hz.c_str(), nullptr, 10) / 1000000LL;
    print_entry(out, kClocks[i][0], std::to_string(mhz) + "Mhz");
  }

  print_header(out, "Voltages");
  for (size_t i = 0; i < sizeof kVolts / sizeof *kVolts; i++) {
    std::string volts = query(std::string("measure_volts ") + kVolts[i]);
    if (volts.empty()) {
      return false;
    }
    // %g trims trailing zeros: "1.1000V" -> "1.1V", "1.2250V" -> "1.225V"
    snprintf(buf, sizeof buf, "%gV", strtod(volts.c_str(), nullptr));
    print_entry(out, kVolts[i], buf);
  }

  print_header(out, "Temperatures");
  std::string temp = query("measure_temp");
  if (temp.empty()) {
    return false;
  }
  double celsius = strtod(temp.c_str(), nullptr);
  if (use_fahrenheit) {
    snprintf(buf, sizeof buf, "%.1fF", celsius * 9.0 / 5.0 + 32.0);
  } else {
    snprintf(buf, sizeof buf, "%.1fC", celsius);
  }
  print_entry(out, "SOC", buf);

  print_header(out, "Memory Allocation");
  static const char *kMem[][2] = {{"CPU", "arm"}, {"GPU", "gpu"}};
  for (size_t i = 0; i < sizeof kMem / sizeof *kMem; i++) {
    std::string mem = query(std::string("get_mem ") + kMem[i][1]);
    if (mem.empty()) {
      return false;
    }
    long mb = strtol(mem.c_str(), nullptr, 10);
    print_entry(out, kMem[i][0], std::to_string(mb) + "MB");
  }

  std::cout << out.str();
  return true;
}

void refresh_output(const unsigned long delay) {
  // Hide the cursor and clear the terminal once, then redraw in place
  // each cycle to avoid flicker
  std::cout << "\033[?25l\033[2J";
  for (;;) {
    std::cout << "\033[H";
    if (!get_info()) {
      break;
    }
    std::cout << "\033[J" << std::flush;
    struct timespec ts;
    ts.tv_sec = delay / 1000UL;
    ts.tv_nsec = (delay % 1000UL) * 1000000L;
    nanosleep(&ts, nullptr);
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

// Restores the cursor on Ctrl+C so the terminal is left usable.
// Only async-signal-safe calls allowed here.
static void handle_signal(int) {
  ssize_t ret = write(STDOUT_FILENO, "\033[?25h\n", 7);
  (void)ret;
  _exit(0);
}

int main(int argc, char *argv[]) {
  unsigned long delay = kDefaultDelay;
  int opt;

  while ((opt = getopt(argc, argv, "t:fvh")) != -1) {
    switch (opt) {
      case 't': {
        double seconds = strtod(optarg, nullptr);
        if (seconds <= 0) {
          std::cerr << kAppName << ": invalid refresh delay '" << optarg << "'" << std::endl;
          return 1;
        }
        delay = (unsigned long)(seconds * 1000.0);
        break;
      }
      case 'f':
        use_fahrenheit = true;
        break;
      case 'v':
        std::cout << kAppName << " v" << VERSION_STRING << std::endl;
        return 0;
      case 'h':
        show_help();
        return 0;
      default:
        show_help();
        return 1;
    }
  }

  mbox_fd = mbox_open();
  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  refresh_output(delay);

  mbox_close(mbox_fd);
  return 0;
}
