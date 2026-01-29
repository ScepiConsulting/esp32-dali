#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <Arduino.h>

struct Diagnostics {
    unsigned long uptime_seconds;
    uint32_t messages_rx;
    uint32_t messages_tx;
    uint32_t errors;
    String bus_health;
    unsigned long last_activity;
    float free_heap_kb;
};

extern Diagnostics stats;
extern bool diagnostics_enabled;

void diagnosticsInit();
void updateDiagnostics();
void incrementRxCount();
void incrementTxCount();
void incrementErrorCount();
void updateLastActivity();
String getDiagnosticsJSON();

#endif
