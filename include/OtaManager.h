#pragma once

#include <stddef.h>
#include <stdint.h>

// Wi-Fi and GitHub release checks. The display code does not call into this
// except at startup, and this module does not draw on the screen.
struct DisplayScene {
    char mode[12];
    char message[121];
    char icon[12];
};

struct BackgroundFile {
    char name[40];
    uint8_t *jpeg;
    size_t length;
};

constexpr int kDeskFaceCount = 4;

struct DeskFaces {
    char active[12];
    char clockLine[19];
    char photoCaption[23];
    uint32_t show;
    DisplayScene message;
    BackgroundFile backgrounds[kDeskFaceCount];
};

namespace OtaManager {

void begin();
void onDisplayFailed();
bool takeScene(DisplayScene &out);
bool takeFaces(DeskFaces &out);

}
