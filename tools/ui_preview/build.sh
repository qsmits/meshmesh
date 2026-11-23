#!/bin/sh
# Build and render the M9 screen UI on the host: tools/ui_preview/build.sh m9 OUTDIR [LANG]
# LANG is a code of include/I18n.h (en by default); a list with spaces ("en de uk") renders each into OUTDIR/<code>/.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
LIB=$ROOT/.pio/libdeps/m9
BOARD=${1:-m9};OUT=${2:-$ROOT/artifacts/ui-preview}
simflags=""
simlibs=""
if [ "$BOARD" = "--simulator" ]; then
  BOARD=m9
  OUT=${2:-$ROOT/artifacts/m9-simulator}
  command -v sdl2-config >/dev/null || { echo "Install SDL2 development files (macOS: brew install sdl2)." >&2; exit 1; }
  simflags="-DMM_DESKTOP_SIMULATOR $(sdl2-config --cflags)"
  simlibs=$(sdl2-config --libs)
  mkdir -p "$(dirname "$OUT")"
else
  mkdir -p "$OUT"
fi
DEF="-DARDUINO=10819 -DMM_UI_PREVIEW=1 -DARDUINOJSON_ENABLE_PROGMEM=0 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=0 -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 -DARDUINOJSON_ENABLE_STD_STRING=0 -DARDUINOJSON_ENABLE_STD_STREAM=0"
SRC=$ROOT/src/Ui.cpp
INC="-I$ROOT/tools/ui_preview/shim -I$ROOT/include -I$ROOT/lib/MeshProtocol -I$LIB/ArduinoJson/src -I$LIB/Adafruit\ GFX\ Library -I$LIB/U8g2_for_Adafruit_GFX/src"
B=$(mktemp -d)
trap 'rm -rf "$B"' EXIT
cc -c -O1 -w "$LIB/U8g2_for_Adafruit_GFX/src/u8g2_fonts.c" -o "$B/fonts.o"
eval c++ -std=gnu++17 -O1 -w $DEF $INC -c "\"$LIB/Adafruit GFX Library/Adafruit_GFX.cpp\"" -o "$B/gfx.o"
eval c++ -std=gnu++17 -O1 -w $DEF $INC -c "$LIB/U8g2_for_Adafruit_GFX/src/U8g2_for_Adafruit_GFX.cpp" -o "$B/u8g2.o"
eval c++ -std=gnu++17 -O1 -Wall -Wno-unused-function $DEF $INC -c "$SRC" -o "$B/ui.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/Config.cpp" -o "$B/config.o"
eval c++ -std=gnu++17 -O1 -w $DEF $INC $simflags -c "$ROOT/tools/ui_preview/preview.cpp" -o "$B/preview.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/I18n.cpp" -o "$B/i18n.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/UiIcons.cpp" -o "$B/icons.o"
eval c++ -std=gnu++17 -O1 -Wall -Wextra $DEF $INC -c "$ROOT/src/Emoji.cpp" -o "$B/emoji.o"
eval c++ -std=gnu++17 -O1 -Wall -Wextra $DEF $INC -c "$ROOT/src/Apps.cpp" -o "$B/apps.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/RadarModel.cpp" -o "$B/radar.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -I$LIB/Crypto -c "$ROOT/src/Channels.cpp" -o "$B/channels.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -I$LIB/Crypto -c "$ROOT/src/Regions.cpp" -o "$B/regions.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/Notifications.cpp" -o "$B/notifications.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/ChannelPolicy.cpp" -o "$B/policy.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/QuickSend.cpp" -o "$B/quick.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/People.cpp" -o "$B/people.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/Wardrive.cpp" -o "$B/wardrive.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/History.cpp" -o "$B/history.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/HistoryStorage.cpp" -o "$B/history-storage.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/HistoryReply.cpp" -o "$B/history-reply.o"
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/Clock.cpp" -o "$B/clock.o"
for f in SHA256 Hash Crypto;do eval c++ -std=gnu++17 -O1 -w $DEF $INC -c "$LIB/Crypto/$f.cpp" -o "$B/crypto-$f.o";done
eval c++ -std=gnu++17 -O1 -Wall $DEF $INC -c "$ROOT/src/ScreenCredential.cpp" -o "$B/credential.o"
if [ "$(uname -s)" != Darwin ];then simlibs="$simlibs -lcrypto";fi
c++ "$B"/*.o $simlibs -o "$B/preview"
if [ -n "$simflags" ]; then
  mv -f "$B/preview" "$OUT"
  echo "$OUT"
  exit 0
fi
# KEEP=PATH: keep the program (tools/ui_preview/preview.cpp "--dice" serves the dice to the web page checks).
if [ -n "$KEEP" ];then rm -f "$KEEP";cp "$B/preview" "$KEEP";fi # a new file: macOS kills a binary overwritten in place
# Several languages ("en de uk"): one build, a folder per language.
case "${3:-en}" in
*" "*)for L in $3;do mkdir -p "$OUT/$L";"$B/preview" "$OUT/$L" $L >/dev/null;done;;
*)"$B/preview" "$OUT" ${3:-en};;
esac
