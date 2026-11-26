#pragma once
// Board identity. This firmware targets two boards, each a PlatformIO environment (platformio.ini):
//   m9         Elecrow ThinkNode M9 (v1.0 and v1.1, told apart at runtime by the keyboard address, Hardware.cpp):
//              320x240 colour screen and keyboard, Ui.cpp, LR1110 radio
//   heltec_v4  Heltec WiFi LoRa 32 V4 (MM_HELTEC_V4; MM_HELTEC_R8 for the V4-R8): 128x64 OLED and one PRG
//              button, UiHeltec.cpp (MM_COMPACT), SX1262 radio
// Pins are in BoardPins.h.
//   MM_BOARD_ID    status "board" for the web page and the checks
//   MM_BOARD_NAME  shown on the device page and in the USB boot line
//   MM_NODE_NAME   default node name
//   MM_MAX_POWER   highest LoRa power the front end takes, dBm
//   MM_ABSENT      diagnostic modules the board does not have, each followed by a comma
//                  (web page indices: 1 keyboard, 2 SD, 4 RTC, 5 GPS, 6 compass, 7 IMU)
//   MM_BUTTON      label of the button the status reports (the power button; the only one on the Heltec)
#if defined(MM_HELTEC_V4)
#define MM_BOARD_ID "heltec_v4"
#define MM_BOARD_NAME "Heltec V4"
#define MM_NODE_NAME "Heltec V4"
#define MM_GPS_DEFAULT false
#define MM_ABSENT 1, 2, 4, 6, 7,
#define MM_BUTTON "PRG"
#define MM_MAX_POWER 22
// The 128x64 one-button interface (UiHeltec.cpp, HardwareHeltec.cpp) and the SX1262 transceiver.
#define MM_COMPACT 1
#define MM_RADIO_SX1262 1
#else
#define MM_BOARD_ID "m9"
#define MM_BOARD_NAME "ThinkNode M9"
#define MM_NODE_NAME "M9"
#define MM_GPS_DEFAULT true
#define MM_ABSENT
#define MM_BUTTON "PRG"
#define MM_MAX_POWER 22
#define MM_RADIO_LR1110 1
#endif
// Native USB (ESP32-S3 USB Serial/JTAG, the Heltec) instead of a USB-UART bridge (the M9).
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
#define MM_NATIVE_USB 1
#endif
// Nodes remembered (MeshCore contacts): set per board in platformio.ini, which MeshCore itself must see too; the same
// values here for the screen preview, which builds without those flags.
#ifndef MAX_CONTACTS
#if defined(MM_HELTEC_V4)
#define MAX_CONTACTS 24
#else
#define MAX_CONTACTS 100
#endif
#endif
