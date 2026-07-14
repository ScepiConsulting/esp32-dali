#ifndef FUNCTION_HANDLER_H
#define FUNCTION_HANDLER_H

#include <Arduino.h>
#include "project_dali_handler.h"

void handleFunctionPage();
void handleDALISend();
void handleDALIScan();
void handleDALICommission();
void handleAPICommissionProgress();
void handleAPIRecent();
void handleAPIPassiveDevices();

#endif
