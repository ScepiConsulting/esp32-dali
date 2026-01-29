#ifndef BALLAST_HANDLER_H
#define BALLAST_HANDLER_H

#include <Arduino.h>
#include "config.h"
#include "ballast_state.h"
#include "dali_protocol.h"
#include "DALI_Lib.h"

extern Dali dali;
extern BallastState ballastState;
extern BallastMessage recentMessages[RECENT_MESSAGES_SIZE];
extern uint8_t recentMessagesIndex;
extern QueuedResponse responseQueue[RESPONSE_QUEUE_SIZE];
extern uint8_t responseQueueHead;
extern uint8_t responseQueueTail;
extern unsigned long lastBusActivityTime;
extern bool busIsIdle;

void ballastInit();
void loadBallastConfig();
void saveBallastConfig();
void monitorDaliBus();
bool isForThisBallast(uint8_t addr_byte);
bool isSpecialCommand(uint8_t addr_byte);
void processCommand(uint8_t addr_byte, uint8_t data_byte);
void processSpecialCommand(uint8_t cmd_byte, uint8_t param);
void processDeviceCommand(uint8_t inst_byte, uint8_t opcode);
void setLevel(uint8_t level);
void updateFade();
void updateLED();
void sendBackwardFrame(uint8_t data);
bool enqueueResponse(uint8_t data);
void processResponseQueue();
bool isBusIdle();
void updateBusActivity();
uint8_t getStatusByte();
void addRecentMessage(const BallastMessage& msg);
void recallScene(uint8_t scene);
void storeScene(uint8_t scene, uint8_t level);
uint8_t queryScene(uint8_t scene);

uint8_t bus_is_high();
void bus_set_low();
void bus_set_high();

#endif
