#ifndef CONFIG_H
#define CONFIG_H

// Uncomment to enable serial debug logging (saves ~40KB flash when disabled)
#define DEBUG_SERIAL

#define DALI_TX_PIN 17
#define DALI_RX_PIN 14
#define DALI_TIMER_FREQ 9600000

#define AP_SSID "ESP32-DALI-Ballast"
#define AP_PASSWORD "daliconfig"
#define DNS_PORT 53

// Onboard LED pin (WS2812 RGB LED on Waveshare ESP32-S3-PICO)
#define LED_PIN 21
#define LED_COUNT 1
// LED color order: 0=RGB, 1=GRB, 2=BGR, 3=RBG, 4=GBR, 5=BRG
#define LED_COLOR_ORDER 1

#define RECENT_MESSAGES_SIZE 20
#define QUERY_RESPONSE_TIMEOUT_MS 22
#define RESPONSE_QUEUE_SIZE 10
#define BUS_IDLE_TIMEOUT_MS 100
#define MAX_RESPONSE_RETRIES 3

#define WDT_TIMEOUT 30

#endif
