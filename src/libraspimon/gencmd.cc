// Copyright (c) 2026 Alex313031

// The VideoCore mailbox layer: the Pi's firmware exposes hardware info
// (clocks, voltages, temperature, memory split) through a "mailbox" - a
// message passing interface between the ARM cores and the firmware. The
// kernel presents it as the character device /dev/vcio, which we open like
// a file and drive with ioctl(). On Win32 this would be opening a device
// path with CreateFile() and calling DeviceIoControl() on the handle.

#include "gencmd.h"

#include "utils.h"

Mbox::Mbox() {
  // open a char device file used for communicating with kernel mbox driver
  // first try the more restrictive interface but fall back to full if unavailable
  //
  // On Linux, device drivers appear as special files under /dev, opened
  // with the same open() syscall as regular files (compare Win32's
  // CreateFile() on a "\\.\DeviceName" path). Needs root or membership
  // in the `video` group. O_RDONLY = read-only access mode
  constexpr std::array<const char*, 2> kDevices{"/dev/vcio_gencmd", "/dev/vcio"};
  bool denied    = false;
  int last_error = 0;
  for (const char* device : kDevices) {
    file_desc = open(device, O_RDONLY);
    if (file_desc >= 0) {
      return;
    }
    // errno says WHY open() failed (the Win32 analog is GetLastError()
    // after CreateFile), and the next open() overwrites it - capture now.
    // If either device exists but refuses us, it's a permissions story
    last_error = errno;
    if (last_error == EACCES || last_error == EPERM) {
      denied = true;
    }
  }
  // "You may not touch the device" and "there is no such device" need
  // very different advice, so tell them apart instead of one catch-all
  if (denied) {
    throw std::runtime_error(std::string("permission denied opening ") + kDevices.back() +
                             " - run with sudo, or add your user to the video group\n"
                             "(sudo usermod -aG video $USER, then log out and back in)");
  }
  if (last_error == ENOENT) {
    throw std::runtime_error(std::string("can't open device file ") + kDevices.back() +
                             " (are you running on a Raspberry Pi 2/3/4/5?)");
  }
  throw std::runtime_error(std::string("can't open device file ") + kDevices.back() + ": " +
                           std::strerror(last_error));
}

Mbox::~Mbox() {
  close(file_desc); // release the fd, like Win32 CloseHandle()
}

int Mbox::MboxProperty(void* buff) const {
  // ioctl() is the catch-all "device control" syscall: the request code
  // (kIoctlMboxProperty) tells the driver behind file_desc what operation to
  // perform on `buff` - here, "hand this property message to the firmware
  // and write its response back into the same buffer". Directly analogous
  // to Win32 DeviceIoControl(handle, IOCTL_code, buffer, ...)
  int ret_val = ioctl(file_desc, kIoctlMboxProperty, buff);

  if (ret_val < 0) {
    std::cerr << "ioctl_set_msg failed: " << ret_val << std::endl;
  }
  return ret_val;
}

std::optional<std::string> Mbox::VideoCoreGenCommand(const std::string& command) const {
  // maximum length for command or response
  if (command.size() + 1 >= kMaxString) {
    std::cerr << "gencmd command length too long: " << command.size() << std::endl;
    return std::nullopt;
  } else {
    if (IsDebugMode()) {
      // stderr, so `raspimon 2>debug.log` captures the trace without the
      // per-query chatter garbling the dashboard on stdout
      std::cerr << "Querying VideoCore with command: " << command << std::endl;
    }
  }

  // Build a mailbox "property message": an array of 32-bit words handed
  // to the firmware, which overwrites it in place with the response.
  //   p[0]    total message size in bytes
  //   p[1]    request/response code (0 = this is a request)
  //   p[2]    tag id (kGenCmdTag = "execute a gencmd command")
  //   p[3]    size of the tag's value buffer in bytes
  //   p[4]    tag request/response length
  //   p[5]    value buffer word 0: gencmd error code in the response
  //   p[6..]  value buffer: command string in, response string out
  //   last    end-tag terminator (0)
  // (kMaxString >> 2) converts the 4KB value buffer size to word count
  std::array<unsigned int, (kMaxString >> 2) + 7> p{};
  size_t i = 0;
  p[i++]   = 0;          // size
  p[i++]   = 0x00000000; // process request

  p[i++] = kGenCmdTag;                            // (the tag id)
  p[i++] = static_cast<unsigned int>(kMaxString); // buffer_len
  p[i++] = 0;                                     // request_len (set to response length)
  p[i++] = 0;                                     // error response

  std::memcpy(&p[i], command.c_str(), command.size() + 1);
  i += kMaxString >> 2;

  p[i++] = 0x00000000;                                          // end tag
  p[0]   = static_cast<unsigned int>(i * sizeof(unsigned int)); // actual size

  if (MboxProperty(p.data()) < 0) {
    return std::nullopt;
  }
  if (p[5] != 0) { // firmware error code
    return std::nullopt;
  }
  // The response is a NUL-terminated C string the firmware wrote into the
  // value buffer; strnlen() bounds the scan in case the terminator is
  // missing, and the (length-counted) std::string constructor copies it out
  const char* response = reinterpret_cast<const char*>(&p[6]);
  return std::string(response, strnlen(response, kMaxString - 1));
}

std::optional<unsigned int> Mbox::GetBoardRevision() const {
  // A minimal property message (see VideoCoreGenCommand for the layout
  // walkthrough): same framing, but the value buffer is a single 32-bit
  // word the firmware overwrites with the revision code
  std::array<unsigned int, 7> p{};
  p[0] = static_cast<unsigned int>(sizeof(p)); // total size in bytes (28)
  p[1] = 0x00000000;                           // process request
  p[2] = kBoardRevTag;                         // board revision tag id
  p[3] = 4;                                    // value buffer size in bytes
  p[4] = 0;                                    // request length
  p[5] = 0;                                    // value buffer, receives the revision
  p[6] = 0x00000000;                           // end tag

  if (MboxProperty(p.data()) < 0) {
    return std::nullopt;
  }
  if (p[1] != 0x80000000) { // firmware sets this code on success
    return std::nullopt;
  }
  return p[5];
}

// Runs `command` via gencmd and returns the value after the '=' in the
// response (e.g. "frequency(48)=600000000" -> "600000000")
std::optional<std::string> QueryCmd(const Mbox& mbox, const std::string& command) {
  std::optional<std::string> response = mbox.VideoCoreGenCommand(command);
  if (!response) {
    if (IsDebugMode()) {
      std::cerr << "Failed to run command: " << command << std::endl;
    }
    return std::nullopt;
  }
  const size_t eq = response->find('=');
  if (eq == std::string::npos) {
    return response;
  }
  return response->substr(eq + 1);
}
