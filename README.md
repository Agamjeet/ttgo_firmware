# Love Display firmware

Bring-up firmware for the standard LILYGO T-Display-S3 (1.9 inch, 170×320, ST7789). It draws a pixel heart and the message "Made just for you".

The screen program is updated from a public GitHub Release. A website can change the message later. That website is not part of this folder. The first copy of this program still has to be written over USB. After that, a newer `firmware.bin` on GitHub can replace it.

## What actually runs on the board

`src/main.cpp` is the program you edit. `include/TDisplayS3.hpp` tells the screen which pins to use. The ESP32 cannot run those C++ files. PlatformIO compiles them into one binary, `.pio\build\lilygo-t-display-s3\firmware.bin`. That file is the program stored in the chip's flash at address `0x10000`. On boot the ESP32 copies it into RAM and runs it, which draws the heart.

The upload also writes three small Espressif files that are not your message: the bootloader at `0x0`, the partition table at `0x8000`, and the OTA boot record at `0xe000`. Your code is `firmware.bin`.

## Command Prompt

Use Command Prompt. Paste each line by itself. The folder name is `.platformio` with a dot. `C:\Users\padda\platformio` does not exist.

1. Go to the project folder:

```bat
cd /d C:\Users\padda\deepali\love-display-firmware
```

2. See if the board is plugged in. This does not flash anything. The board last showed up as COM3.

```bat
C:\Users\padda\.platformio\penv\Scripts\pio.exe device list
```

3. Compile the program into `firmware.bin`. This does not flash anything.

```bat
C:\Users\padda\.platformio\penv\Scripts\pio.exe run
```

4. Write `firmware.bin` onto the ESP32:

```bat
C:\Users\padda\.platformio\penv\Scripts\pio.exe run -t upload --upload-port COM3
```

5. Read the text the board prints. Press Ctrl+C to close this.

```bat
C:\Users\padda\.platformio\penv\Scripts\pio.exe device monitor --port COM3
```

If `device list` prints a different COM number, use that number instead of COM3 in steps 4 and 5.

If upload cannot connect, hold BOOT, tap RST, release BOOT, and run step 4 again. Press RST once after a successful upload.

The serial monitor should print:

```text
Love Display bring-up
Display ready: 320 x 170 landscape, ST7789.
Welcome screen drawn.
```

This firmware matches the standard T-Display-S3 LCD, not the AMOLED, Long, or Pro variant.

The screen is 320 pixels wide and 170 pixels tall. The top-left corner is (0, 0). The bottom-right corner is (319, 169). The center is (160, 85).

## GitHub Release

Put one file on the release: `firmware.bin`.

On this PC that file is built here:

`C:\Users\padda\deepali\love-display-firmware\.pio\build\lilygo-t-display-s3\firmware.bin`

Upload that file as-is and keep the name `firmware.bin`. Do not upload `firmware.elf`, `bootloader.bin`, or `partitions.bin`. Those are not the program the board downloads.

The repository is [Agamjeet/ttgo_firmware](https://github.com/Agamjeet/ttgo_firmware). It is empty right now, so it is not a release yet. The board reads `firmware/manifest.json` on the `main` branch. That file points at the release. The repository has to stay public because this program does not contain a GitHub token.

### One-time GitHub setup

1. Push this project to `https://github.com/Agamjeet/ttgo_firmware`.
2. `include/config.h` already uses this manifest address:

```text
https://raw.githubusercontent.com/Agamjeet/ttgo_firmware/main/firmware/manifest.json
```

3. Build and USB-flash once, using the Command Prompt commands above. The board cannot download a release until this version is already running.
4. Join the Wi-Fi setup network `LoveDisplay-Setup`. The password is `lovedisplay`. Open `http://192.168.4.1` and enter your home Wi-Fi. The home password is saved on the board. It is not stored in the file you upload to GitHub.

### Publish a new program

1. In `include/firmware_version.h`, change `FIRMWARE_VERSION` to a higher number, such as `1.0.1`.
2. Build:

```bat
cd /d C:\Users\padda\deepali\love-display-firmware
C:\Users\padda\.platformio\penv\Scripts\pio.exe run
```

3. Get the checksum. In the hash output, remove the spaces:

```bat
certutil -hashfile .pio\build\lilygo-t-display-s3\firmware.bin SHA256
```

4. Open [the releases page](https://github.com/Agamjeet/ttgo_firmware/releases), then **Draft a new release**.
5. Set the tag to `v1.0.1` (the same number as the version, with a `v` in front).
6. Attach `firmware.bin` from the path above. Publish the release.
7. Edit `firmware/manifest.json` so it matches that release:

```json
{
  "version": "1.0.1",
  "url": "https://github.com/Agamjeet/ttgo_firmware/releases/download/v1.0.1/firmware.bin",
  "sha256": "the checksum from certutil with no spaces",
  "status": "enabled"
}
```

8. Commit `firmware/manifest.json` and push it to the `main` branch.

The board checks about 20 seconds after it joins Wi-Fi, then every 6 hours. Serial should show the new version after it reboots. If `status` is `disabled`, or the checksum does not match, it keeps the program it already has.

A message or a picture for the screen is not this file. Those will be changed from the website later. Use a GitHub Release only when the C++ program itself changes.