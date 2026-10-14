#pragma once

// Wi-Fi and GitHub release checks. The display code does not call into this
// except at startup, and this module does not draw on the screen.
namespace OtaManager {

void begin();
void onDisplayFailed();

}
