#pragma once

// Bump this before building a firmware.bin you publish to GitHub Releases.
// The manifest "version" must be this same string, or the board will keep
// seeing the new file as newer and install it again after every reboot.
#define FIRMWARE_VERSION "1.1.5"
