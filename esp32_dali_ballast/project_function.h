#ifndef FUNCTION_HANDLER_H
#define FUNCTION_HANDLER_H

#include <Arduino.h>
#include "project_ballast_handler.h"

void functionInit();
void functionLoop();
void handleFunctionPage();
void handleBallastSave();
void handleBallastControl();
void handleAPIRecent();

#endif
