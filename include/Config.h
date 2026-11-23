#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "Board.h"
#include "I18n.h"

// The radio parameters shared by a network (a profile of the networks catalogue): frequency, bandwidth, SF, CR.
struct RadioProfile {
  float frequency = 0, bandwidth = 0;
  uint8_t sf = 0, cr = 0;
  bool set() const { return frequency > 0; }
};
struct Config {
  char name[25] = MM_NODE_NAME;
  float frequency = 868.731f, bandwidth = 62.5f;
  uint8_t sf = 8, cr = 6, hops = 3, brightness = 180;
  uint8_t pathHash = 2; // bytes per hop in the path of our flood packets, 1..3 (MeshCore path hash mode + 1)
  int8_t power = 10;
  bool relay = true, gps = true, sound = true, batteryVolts = false;
  uint8_t lang = LangEn; // interface language, I18n.h
  uint16_t autoLock = 90, dimAfter = 30;
  int16_t utcOffset = 180;
  uint8_t key[32] = {};
  uint32_t bootCounter = 0;
  uint32_t blePin = 0; // pairing PIN, made once: a restart (e.g. a USB-UART reset) keeps it
  bool bleOn = false; // Bluetooth on after boot: the last choice, else on
  uint8_t theme = 0, accentColor = 0, nightBrightness = 0; // 0 night brightness follows daytime.
  bool nightAt(time_t utc) const {
    if (utc < 1700000000) return false;
    int hour = ((int64_t(utc) / 60 + utcOffset) % 1440) / 60;
    return hour < 7 || hour >= 20;
  }
  bool lightAt(time_t utc) const { return theme == 1 || (theme == 2 && utc >= 1700000000 && !nightAt(utc)); }
  uint8_t backlight(time_t utc) const { return nightBrightness && nightAt(utc) ? nightBrightness : brightness; }
  bool launcherTiles = false; // Home and folders default to List
  bool lockDetails = true; // the locked screen names message senders
  uint8_t notifyLed = 1, notifyPopup = 1;
  uint8_t notifyWake = 0; // preserve the keyboard boards' existing screen-off behaviour
  bool notifyFailed = false;
  char notifyWords[97] = "";
  char apps[256] = ""; // screen menu: order of the apps, "-" hides one (App.h); empty = the default
  // Default region of our flood packets (Regions.h): empty - none; the key is that of "#name" unless an app set
  // another (a private region of the MeshCore app).
  char region[31] = "";
  uint8_t regionKey[16] = {};
  // The regional preset chosen last (RadioPresets.h), by name: tells apart the presets that share the radio.
  char preset[32] = "";
  // The profile before the last change of the shared radio parameters ("radio previous" returns to it), and
  // during a test ("radio try") the saved one: the test lives in memory only, a restart brings the saved one back.
  RadioProfile previous, trial;
  bool doubleBack = false; // one-button boards: a double click steps back (single clicks then wait for a second one)
  RadioProfile radio() const {
    RadioProfile r;
    r.frequency = frequency;
    r.bandwidth = bandwidth;
    r.sf = sf;
    r.cr = cr;
    return r;
  }
  void setRadio(const RadioProfile& r) {
    frequency = r.frequency;
    bandwidth = r.bandwidth;
    sf = r.sf;
    cr = r.cr;
  }
  bool sameRadio(const RadioProfile& r) const {
    return fabsf(frequency - r.frequency) < .0005f && bandwidth == r.bandwidth && sf == r.sf && cr == r.cr;
  }
  void load();
  bool save();
  bool valid() const;
  String keyHex() const;
  bool setKey(const String& text);
  void saveBle(bool on);
};
extern Config config;
