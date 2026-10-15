#pragma once

// Public raw URL of firmware/manifest.json.
// https://github.com/Agamjeet/ttgo_firmware
// The repository must stay public. This firmware does not contain a GitHub token.
#define GITHUB_MANIFEST_URL "https://raw.githubusercontent.com/Agamjeet/ttgo_firmware/main/firmware/manifest.json"

// display.json on the raw address stays cached for about 5 minutes, and a
// changing ?t= is ignored. The API returns the file that was just saved.
#define GITHUB_DISPLAY_URL "https://api.github.com/repos/Agamjeet/ttgo_firmware/contents/firmware/display.json"

// First check after Wi-Fi connects, then the regular interval.
#define OTA_FIRST_CHECK_MS (20UL * 1000UL)
#define OTA_CHECK_INTERVAL_MS (6UL * 60UL * 60UL * 1000UL)
#define OTA_RETRY_MS (5UL * 60UL * 1000UL)
// How often the board looks for a remote "check now" request.
#define OTA_TRIGGER_INTERVAL_MS (15UL * 1000UL)
// How often it reads the saved message. Every 90 seconds stays under GitHub's
// hourly limit while the board is left on. A save still shows up in a couple of minutes.
#define OTA_DISPLAY_INTERVAL_MS (90UL * 1000UL)

// POSIX timezone. IST is UTC+5:30. The sign is reversed in this format.
#define COMPANION_TZ "IST-5:30"
#define COMPANION_NTP_1 "pool.ntp.org"
#define COMPANION_NTP_2 "time.google.com"
