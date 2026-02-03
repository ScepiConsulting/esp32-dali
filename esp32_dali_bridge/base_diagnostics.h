#ifndef DIAGNOSTICS_CORE_H
#define DIAGNOSTICS_CORE_H

#include <Arduino.h>
#include <vector>

struct DiagnosticItem {
  String label;
  String value;
};

struct DiagnosticSection {
  String title;
  std::vector<DiagnosticItem> items;
};

struct CoreDiagnostics {
  unsigned long uptime_seconds;
  float free_heap_kb;
  String chip_model;
  uint8_t chip_revision;
  uint32_t flash_size;
  uint32_t sketch_size;
  uint32_t free_sketch_space;
};

extern CoreDiagnostics coreStats;
extern bool diagnostics_mqtt_enabled;

void diagnosticsCoreInit();
void updateCoreDiagnostics();
String getDiagnosticsJSON();
String getCoreDiagnosticsHTML();

void handleDiagnosticsPage();
void handleDiagnosticsSave();
void handleAPIDiagnostics();

extern std::vector<DiagnosticSection> getFunctionDiagnosticSections();
extern String getFunctionDiagnosticsJSON();

#endif
