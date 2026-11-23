#pragma once
#include <Arduino.h>
// All boards: with the screen off, block the application task for up to 20 ms, woken by LoRa IRQ.
// The RTOS idle task controls actual CPU sleep; its duration is not measured here.
// Power saving:
// CPU clock: 240 MHz while someone uses the device (screen lit, Wi-Fi, radar, recent USB or BLE
// commands) or the Wi-Fi driver or Bluetooth controller has been started, 80 MHz otherwise. There is no light sleep.
void powerWake(); // input arrived: full clock now
void powerTick(bool usbIdle); // end of each loop pass; usbIdle: no USB output or baud change pending
uint8_t powerMhz();
uint32_t powerSleeps();
uint32_t powerSleptMs();
uint32_t powerSlowMs(); // light sleeps, time asleep, time at 80 MHz
void powerRadioIrq(); // RadioLib ISR only
uint32_t powerIdleWaits();
uint32_t powerIdleMs();
uint32_t powerRadioEvents();
// Power off: deep sleep (Hardware::powerDown). The request answers at once; 1.5 s later (the reply leaves over USB, Wi-Fi or
// BLE; a packet being sent gets up to 5 s more) delayed writes are flushed, the transceiver sleeps and
// the board turns off. RESET or the power slider starts it again.
String powerOff();
bool powerOffPending();
int powerOnWay(); // Hardware::wakePin(): a pin = hold that button; -1: RESET or the power switch; -2: the PMU key
void powerOffTick(); // main loop
void powerBootCheck(); // first thing at boot: a wake that is not a held button goes back to sleep
