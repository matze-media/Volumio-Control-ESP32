#include <Arduino.h>
#include <U8g2lib.h>
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#include "Volumio.h"
Volumio volumio;

#include "driver/adc.h"
#include <esp_bt.h>
#include <IRremote.h> 

// Display
//U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
//U8G2_SSD1327_EA_W128128_F_HW_I2C u8g2(U8G2_R0,/* reset=*/ U8X8_PIN_NONE);

#include <esp_wifi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>

// define wifi access
const char* ssid = "m4chine";
const char* password = "DF0526F481E9CA9461BF21FE85";

// define volumio IP
const char* volumio_ip = "192.168.1.100";
int volumio_port = 80;

#define USE_SERIAL Serial
WiFiMulti wifiMulti;


int wifi_counter = 0;
int i = 0;
unsigned long runMillis;
boolean gotoSleep;

#define DEEP_SLEEP_TIME 10 // How many minutes the ESP should sleep
int sleepCount = 0;
esp_sleep_wakeup_cause_t wakeup_reason;

//IRremote
unsigned int lastIRCode;
int RECV_PIN = 15; 
IRrecv IRReceiver(RECV_PIN);
decode_results ir_receiver_result; 

// Battery load pin
int BATTERY_PIN = 34; 
int batteryChargeLevel = 5;
double batteryVoltage = 3.7; 
long lastReadBattery = 0;

// button control
#define STATE_UNPRESSED 0
#define STATE_SHORT 1
#define STATE_LONG 2
volatile int  resultButton = 0; // global value set by checkButton()


// rotery encoder = volumeEncoder
#define PIN_Encoder_SW 27 //  schwarz  blocks one touch pin
#define PIN_Encoder_DT 26 // orange
#define PIN_Encoder_CLK 25 // rot
#include <RotaryEncoder.h>
RotaryEncoder encoder(PIN_Encoder_DT, PIN_Encoder_CLK, RotaryEncoder::LatchMode::FOUR3);
long lastsendvolume = 0;
static int rotaryPos = 0;

// Runningman
#include <RunningMedian.h>
RunningMedian wifiSignalMedian = RunningMedian(15);
RunningMedian batterySignalMedian = RunningMedian(15);


//Source https://github.com/volumio/volumio-graphic-resources
//Converted using InkScape and Paint by @drvolcano
const unsigned char logo_volumio_big_bits[] = {
  0x01, 0x80, 0xE0, 0x0F, 0x08, 0x80, 0x00, 0x02, 0x7F, 0xFC, 0xC0, 0x80, 
  0x3F, 0x00, 0x03, 0xC0, 0x30, 0x18, 0x08, 0x80, 0x00, 0x82, 0xC1, 0x86, 
  0xC1, 0xC0, 0x60, 0x00, 0x02, 0x40, 0x58, 0x30, 0x08, 0x80, 0x00, 0xC2, 
  0x80, 0x03, 0xC3, 0x60, 0xC0, 0x00, 0x06, 0x60, 0xCC, 0x60, 0x08, 0x80, 
  0x00, 0x42, 0x00, 0x01, 0xC2, 0x30, 0x80, 0x01, 0x04, 0x20, 0x84, 0x40, 
  0x08, 0x80, 0x00, 0x42, 0x00, 0x01, 0xC2, 0x10, 0x00, 0x01, 0x0C, 0x30, 
  0x04, 0x40, 0x08, 0x80, 0x00, 0x42, 0x00, 0x01, 0xC2, 0x10, 0x00, 0x01, 
  0x08, 0x10, 0x04, 0x40, 0x08, 0x80, 0x00, 0x42, 0x00, 0x01, 0xC2, 0x10, 
  0x00, 0x01, 0x18, 0x18, 0x04, 0x40, 0x08, 0x80, 0x00, 0x42, 0x00, 0x01, 
  0xC2, 0x10, 0x08, 0x01, 0x10, 0x08, 0x04, 0x40, 0x08, 0x80, 0x00, 0x42, 
  0x00, 0x01, 0xC2, 0x10, 0x18, 0x01, 0x30, 0x0C, 0x0C, 0x60, 0x18, 0x80, 
  0x01, 0x43, 0x00, 0x01, 0xC2, 0x30, 0x90, 0x01, 0x20, 0x04, 0x18, 0x30, 
  0x30, 0x00, 0x83, 0x41, 0x00, 0x01, 0xC2, 0x60, 0xC0, 0x00, 0x60, 0x06, 
  0x30, 0x18, 0x60, 0x00, 0xC6, 0x40, 0x00, 0x01, 0xC2, 0xC0, 0x60, 0x00, 
  0xC0, 0x03, 0xE0, 0x0F, 0xC0, 0x1F, 0x7C, 0x40, 0x00, 0x01, 0xC2, 0x80, 
  0x3F, 0x00, };

void connectWifi();
void goToDeepSleep();
void shutDown();
void interpretIR(unsigned int &ir_receiver_result);
void showSetup();
void playButtonISR();
void checkButton();
ICACHE_RAM_ATTR void checkPosition();
void volumioControl(String cmd);
void showBattery();
void showSeek(int v_seek);
void showQuality();
void showBitrate();
void showVolume();
void showWifi();
void showIRC();
void hideIRC();
void showPlayer();
void showMusic();
void showDebugMessage(const char* message);
void controlVolume(int level);
void showBigVolume();
void showWebradioTitle();
void showTitle();
double getBatteryVoltage();
double getBatteryChargeLevel(double volt);
