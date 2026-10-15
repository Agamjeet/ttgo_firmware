#pragma once

#include "OtaManager.h"
#include "TDisplayS3.hpp"

// Local desk modes. Remote config can call setMode later without drawing itself.
enum class DeskMode { Message, Clock, Animation, Photo };

namespace Companion {

void apply(DeskFaces &faces);
void begin(TDisplayS3 &display);
void setScene(const DisplayScene &scene);
void setMode(DeskMode mode);
void loop();

}
