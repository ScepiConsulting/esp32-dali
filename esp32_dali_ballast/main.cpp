// DALI Ballast entry point. The esp32-base framework owns setup()/loop();
// project behaviour is added via the app* hooks in the project_*.cpp modules.
#include "base_app.h"

void setup() { baseSetup(); }

void loop() { baseLoop(); }
