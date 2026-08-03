#ifndef LIBRASPIMON_GENCMD_H_
#define LIBRASPIMON_GENCMD_H_

#include "pch.h"

// Mailbox "tag id" that tells the VideoCore firmware to execute a gencmd
// command (the text commands vcgencmd uses, e.g. "measure_temp")
inline constexpr unsigned int kGenCmdTag = 0x00030080; // tag id

// Mailbox tag id that asks the firmware for the board revision code
inline constexpr unsigned int kBoardRevTag = 0x00010002; // tag id

// The ioctl request code the vcio kernel driver expects. _IOWR() packs the
// transfer direction (read+write), the driver's magic number (100), a
// command number (0), and the argument size into one integer - Linux's
// version of the Win32 CTL_CODE() macro for building IOCTL_* codes
inline constexpr unsigned long kIoctlMboxProperty = _IOWR(100, 0, char*); // for accessing mbox

inline constexpr size_t kMaxString = (4u * 1024u); // Max command/response string len

// RAII wrapper around the VideoCore mailbox char device: the constructor
// opens the device and the destructor closes it, so holding an Mbox object
// is holding the open device (no separate init/cleanup calls to forget)
class Mbox {
 public:
  // Opens the mbox device; throws std::runtime_error if unavailable
  Mbox();
  ~Mbox();

  Mbox(const Mbox&)            = delete;
  Mbox& operator=(const Mbox&) = delete;

  // Roughly equivalent to vcgencmd: sends `command` to the VideoCore and
  // returns the raw response (e.g. "frequency(48)=600000000"), or
  // std::nullopt on failure
  std::optional<std::string> VideoCoreGenCommand(const std::string& command) const;

  // Asks the firmware for the board revision code: a bitfield encoding the
  // model, SoC, and RAM size (the same value shown on the "Revision:" line
  // of /proc/cpuinfo). Decoded by the IsPi*()/GetPiModelName() helpers in
  // utils.h. Returns std::nullopt on failure
  std::optional<unsigned int> GetBoardRevision() const;

 private:
  // use ioctl to send mbox property message, buff receives data
  int MboxProperty(void* buff) const;

  // The "file descriptor" from open(): a small integer handle to the open
  // device, POSIX's equivalent of a Win32 HANDLE. -1 means "not open"
  // (POSIX calls that fail return -1, not NULL/INVALID_HANDLE_VALUE)
  int file_desc = -1;
};

// Public Helper for VideoCoreGenCommand
std::optional<std::string> QueryCmd(const Mbox& mbox, const std::string& command);

#endif // LIBRASPIMON_GENCMD_H_
