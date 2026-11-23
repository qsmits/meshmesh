#pragma once
// GPIO numbers of each board. ThinkNode M9 V1.0: Elecrow schematic GPIO names (not package pin numbers).
#include <Arduino.h>
namespace pins {
constexpr float radioTcxo=3.3f;
constexpr int spiClock=40, spiMiso=38, spiMosi=47;
constexpr int radioCs=39, radioIrq=42, radioReset=45, radioBusy=41;
constexpr int lcdCs=16, lcdDc=15, lcdReset=14, backlight=17, peripheralPower=18;
constexpr int sdCs=48, sda=7, scl=6, keyboardSda=20, keyboardScl=21;
// gpsEnable: V1.0 active LOW, V1.1 active HIGH; gpsWake: V1.1 only, a 5 ms HIGH pulse after power-on (stock firmware).
constexpr int gpsRx=2, gpsTx=3, gpsReset=5, gpsEnable=11, gpsWake=10;
constexpr int battery=13, buzzer=9;
// M9: LOW while USB powers the board (charging), HIGH on battery; measured on a V1.1 by unplugging (the battery
// voltage alone stays above 4.25 V for long after a full charge).
constexpr int usbSense=1;
constexpr unsigned keyboardAddress=0x6c;
}
