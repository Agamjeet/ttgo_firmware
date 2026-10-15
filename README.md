# Love Display firmware

Program for the standard LILYGO T-Display-S3 (1.9 inch, 170×320, ST7789). It is for Deepali.

The chip is already running this program, version **1.0.0**, written over USB. A website changes the message, the stamp, the picture, and which screen is showing. A new `firmware.bin` is only for a later change to this C++ program. The first copy still has to be written over USB. After that, a newer release can replace it.

## What she sees

Four screens. The side button moves to the next one: message, clock, animation, photo. The other button changes the theme on every screen: Night, Sunny, Sunset, Sea, Pink, Lilac, Dawn, Forest, Gold. The photo screen draws her picture inside a frame. The website crops that picture to the frame shape, then resizes it to 256×118.

While the saved message is still on its way, the screen says Made just for you. Under that it can say Waiting for Wi-Fi, then Setting the clock. That is only before the saved screen is up. It then keeps that message, so the next time it turns on it shows the saved line straight away and replaces it when a newer one arrives. The message opens as a letter. The clock shows the time, a short line from the website, and the Wi-Fi name at the bottom right. That line starts as Boss. The photo can have a short caption under the frame. The animation shows the dinosaur, hearts, and sunflowers. A picture, when one is saved, sits behind those. The moon, sun, stars, and clouds are left off when a picture is there.

Stamps the board can draw: heart, star, sparkle, smile, flower, sun, moon, and note.

## For the receiver

Plug it into a charger. If it already knows this Wi-Fi, it joins by itself and shows the saved screen. No computer and no app.

If this Wi-Fi was never saved, or the saved one is not here, wait 30 seconds. The board then offers a setup network. The screen itself stays on the heart or the last saved picture. It does not print the setup name on the screen.

1. On the phone, open Wi-Fi and join **LoveDisplay-Setup**. The password is `lovedisplay`.
2. The phone may say this network has no internet. Stay connected anyway.
3. Open the phone browser and go to **http://192.168.4.1**
4. Type the home Wi-Fi name and password, then save.
5. The board leaves the setup network and joins the home Wi-Fi. Within about a minute it shows the saved screen.

Changing network is the same page as the first join. Hold the theme button for about 3 seconds. **LoveDisplay-Setup** then shows in the phone's Wi-Fi list. The password is `lovedisplay`. Join it, open **http://192.168.4.1**, and type the new Wi-Fi name and password. The screen stays on the current picture. The setup name leaves the list after the new network connects, or after about 3 minutes. The clock shows which name it joined.

## If the Wi-Fi drops

A short drop does not change the screen. The message, clock, photo, and buttons keep working. It does not go back to Waiting for Wi-Fi, and it does not start the picture over when the signal returns.

It waits 15 seconds before trying the saved Wi-Fi again, so a blip of a few seconds can heal by itself. The setup network starts only after the saved Wi-Fi has been missing for 30 seconds. New pictures from the website wait until Wi-Fi is back. Anything already on the screen stays.

## What you can change after it is given away

Messages, stamps, pictures, and which screen is showing do **not** need a new `firmware.bin`. The website writes `firmware/display.json`. After it joins Wi-Fi it reads that right away. After a save, the board reads the new message and the chosen screen about every minute and a half. A changed message opens the message screen, a changed clock line opens the clock, and a changed caption opens the photo. Show this screen opens the screen you picked. The side button still works until the next save.

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

A new program is a separate check from the message. The board looks about 20 seconds after it first joins Wi-Fi, then about every 6 hours. If that check fails, it tries again in 5 minutes. Raising `firmware/check.json` asks it to look on the next 15-second pass. The website does that when you save or publish. Saving a message does not download a new program. If the release status is `disabled`, or the checksum does not match, the board keeps the program it already has.

The website repository is [Agamjeet/ttgo_control_panel](https://github.com/Agamjeet/ttgo_control_panel).
