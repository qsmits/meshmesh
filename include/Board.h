#pragma once
// Board identity. This firmware targets the Elecrow ThinkNode M9 only (v1.0 and v1.1, told apart at runtime
// by the keyboard address, Hardware.cpp). Pins are in BoardPins.h.
//   MM_BOARD_ID    status "board" for the web page and the checks
//   MM_BOARD_NAME  shown on the device page and in the USB boot line
//   MM_NODE_NAME   default node name
//   MM_MAX_POWER   highest LoRa power the front end takes, dBm
//   MM_ABSENT      diagnostic modules the board does not have, each followed by a comma
//                  (web page indices: 1 keyboard, 2 SD, 4 RTC, 5 GPS, 6 compass, 7 IMU)
//   MM_BUTTON      label of the power button the status reports
#define MM_BOARD_ID "m9"
#define MM_BOARD_NAME "ThinkNode M9"
#define MM_NODE_NAME "M9"
#define MM_GPS_DEFAULT true
#define MM_ABSENT
#define MM_BUTTON "PRG"
#define MM_MAX_POWER 22
#define MM_RADIO_LR1110 1
