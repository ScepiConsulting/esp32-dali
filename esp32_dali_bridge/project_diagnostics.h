#ifndef DIAGNOSTICS_DATA_H
#define DIAGNOSTICS_DATA_H

#include <Arduino.h>
#include <vector>
#include "base_diagnostics.h"

std::vector<DiagnosticSection> getFunctionDiagnosticSections();
String getFunctionDiagnosticsJSON();

#endif
