"""USB ports of the two boards. Names change after reconnection, so each board is found by its USB bridge: the M9
by its CH340 (1a86:7522), the Heltec V4 by the ESP32-S3 native USB Serial/JTAG (303a:1001). MESHMESH_M9 and
MESHMESH_HELTEC override; otherwise the last known name is kept. flash.py still verifies the chip model and MAC
before writing."""
import os
from serial.tools.list_ports import comports


def _find(variable, vid, pid, fallback):
    if os.environ.get(variable):
        return os.environ[variable]
    found = [p.device for p in comports() if p.vid == vid and p.pid == pid and p.device.startswith('/dev/cu.')]
    return found[0] if len(found) == 1 else fallback


M9_PORT = _find('MESHMESH_M9', 0x1a86, 0x7522, '/dev/cu.wchusbserial10')
HELTEC_PORT = _find('MESHMESH_HELTEC', 0x303a, 0x1001, '/dev/cu.usbmodem1101')
