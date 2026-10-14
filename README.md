# Love Display firmware

Program for the standard LILYGO T-Display-S3 (1.9 inch, 170×320, ST7789). It is for Deepali.

The chip is already running this program, version **1.0.0**, written over USB. A website changes the message, the stamp, the picture, and which screen is showing. A new `firmware.bin` is only for a later change to this C++ program. The first copy still has to be written over USB. After that, a newer release can replace it.

## What she sees

Three screens. The side button moves to the next one: message, clock, animation. The other button changes the theme on every screen: Night, Sunny, Sunset, Sea, Pink, Lilac, Dawn, Forest, Gold.

The message opens as a letter. The starting words are `Happy Bday`. The clock shows the time and the word Boss. The animation shows the dinosaur, hearts, and sunflowers. A picture, when one is saved, sits behind those. The moon, sun, stars, and clouds are left off when a picture is there.

Stamps the board can draw: heart, star, sparkle, smile, flower, sun, moon, and note.

## For the receiver

This is the only setup. It is done once, on the Wi-Fi where the board will stay. After that, plug it into a charger and it shows the screen by itself. No computer and no app.

1. Plug the board into a phone charger or any USB power supply. Wait until the screen is on.
2. On the phone, open Wi-Fi and join **LoveDisplay-Setup**. The password is `lovedisplay`.
3. The phone may say this network has no internet. Stay connected anyway.
4. Open the phone browser and go to **http://192.168.4.1**
5. Type the home Wi-Fi name and password, then save.
6. The board leaves the setup network and joins the home Wi-Fi. Within about a minute it shows the saved screen.

Do this again only if the home Wi-Fi name or password changes.

## What you can change after it is given away

Messages, stamps, pictures, and which screen is showing do **not** need a new `firmware.bin`. The website writes `firmware/display.json`. The board reads that about every 30 seconds while it has Wi-Fi.

A new program file is only for a change to the C++ itself, such as a new drawing. The version in `include/firmware_version.h` is `1.0.0`. The board installs a release only when the published version is **higher** than that. Publishing `1.0.0` again does nothing. The release listed in `firmware/manifest.json` is disabled, so it will not replace the program already on the chip.

You do not have to publish a firmware file now.

## What actually runs on the board

`src/main.cpp` starts the board. `src/PixelScene.cpp` draws the screens. `src/Companion.cpp` handles the buttons, the clock, and the saved message. `include/TDisplayS3.hpp` tells the screen which pins to use. PlatformIO compiles them into `.pio\build\lilygo-t-display-s3\firmware.bin`. That file is stored in flash at address `0x10000`.

The side button is GPIO14. The other button is GPIO0.

## Secret

Up is the other button, the one that changes the theme. Down is the side button, the one that changes the screen. Press them within about two seconds of each other:

Up, Up, Down, Down, Up, Down, Up, Down, Up, Down.

The screen says I love you.

## Command Prompt

Use Command Prompt. Paste each line by itself. The folder name is `.platformio` with a dot.

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

4. The reliable USB write, if `pio run -t upload` stops halfway, is app-only at 115200 with no stub:

```bat
C:\Users\padda\.platformio\penv\Scripts\python.exe C:\Users\padda\.platformio\packages\tool-esptoolpy\esptool.py --chip esp32s3 --port COM3 --baud 115200 --before default_reset --after hard_reset --no-stub write_flash -z --flash_mode dio --flash_freq 80m --flash_size 16MB 0x10000 .pio\build\lilygo-t-display-s3\firmware.bin
```

5. Read the text the board prints. Press Ctrl+C to close this.

```bat
C:\Users\padda\.platformio\penv\Scripts\pio.exe device monitor --port COM3
```

If `device list` prints a different COM number, use that number instead of COM3.

If the write cannot connect, hold BOOT, tap RST, release BOOT, and run the write again. Do not hold BOOT while the board is meant to be running.

This firmware matches the standard T-Display-S3 LCD, not the AMOLED, Long, or Pro variant. The screen is 320 pixels wide and 170 pixels tall.

## Publish a new program later

Use this only after the C++ changes and she already has the board.

1. In `include/firmware_version.h`, change `FIRMWARE_VERSION` to a higher number, such as `1.0.1`.
2. Build with the compile command above.
3. On the website, open **Update the program**, choose that `firmware.bin`, set the same version, and publish.

The website uploads `firmware.bin` to a GitHub release, writes the checksum into `firmware/manifest.json`, and asks the board to install it. The file on this PC is:

`C:\Users\padda\deepali\love-display-firmware\.pio\build\lilygo-t-display-s3\firmware.bin`

Upload that file as-is and keep the name `firmware.bin`. Do not upload `firmware.elf`, `bootloader.bin`, or `partitions.bin`.

The repository is [Agamjeet/ttgo_firmware](https://github.com/Agamjeet/ttgo_firmware). It stays public because this program does not contain a GitHub token. The board reads:

```text
https://raw.githubusercontent.com/Agamjeet/ttgo_firmware/main/firmware/manifest.json
```

It checks about 20 seconds after it joins Wi-Fi, then about every 6 hours. Raising `firmware/check.json` asks it to look sooner. The website does that when you save or publish. If the release status is `disabled`, or the checksum does not match, the board keeps the program it already has.

The website repository is [Agamjeet/ttgo_control_panel](https://github.com/Agamjeet/ttgo_control_panel).
