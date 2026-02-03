#ifndef PROJECT_DALI_HANDLER_H
#define PROJECT_DALI_HANDLER_H

#include <Arduino.h>
#include "project_config.h"
#include "project_dali_protocol.h"
#include "project_dali_lib.h"

extern Dali dali;
extern DaliMessage recentMessages[RECENT_MESSAGES_SIZE];
extern uint8_t recentMessagesIndex;
extern DaliCommand commandQueue[COMMAND_QUEUE_SIZE];
extern uint8_t queueHead;
extern uint8_t queueTail;
extern unsigned long lastDaliCommandTime;
extern unsigned long lastBusActivityTime;
extern bool busIsIdle;
extern CommissioningProgress commissioningProgress;
extern PassiveDevice passiveDevices[DALI_MAX_ADDRESSES];

extern unsigned long daliRxCount;
extern unsigned long daliTxCount;
extern unsigned long daliErrorCount;

void incrementRxCount();
void incrementTxCount();
void incrementErrorCount();

void daliInit();
void updatePassiveDevice(uint8_t address, const DaliMessage& msg);
void clearPassiveDevices();
uint8_t getPassiveDeviceCount();
DaliMessage parseDaliMessage(uint8_t* bytes, uint8_t length, bool is_tx);
bool validateDaliCommand(const DaliCommand& cmd);
bool enqueueDaliCommand(const DaliCommand& cmd);
void processCommandQueue();
bool canSendDaliCommand();
bool isBusIdle();
void updateBusActivity();
void monitorDaliBus();
void performDaliScan();
void sendDaliCommand(uint8_t address, uint8_t level);
void addRecentMessage(const DaliMessage& msg);
DaliScanResult scanDaliDevices();
void commissionDevices(uint8_t start_address);
bool sendCommissioningCommand(uint8_t command, uint8_t data);
int16_t queryCommissioning(uint8_t command);

uint8_t bus_is_high();
void bus_set_low();
void bus_set_high();

#endif
