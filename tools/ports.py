"""USB port of the M9. The name changes after reconnection, so the board is found by its CH340 USB bridge
(1a86:7522). MESHMESH_M9 overrides; otherwise the last known name is kept. flash.py still verifies the chip
model and MAC before writing."""
import os
from serial.tools.list_ports import comports


def _find(variable, vid, pid, fallback):
    if os.environ.get(variable):
        return os.environ[variable]
    found = [p.device for p in comports() if p.vid == vid and p.pid == pid and p.device.startswith('/dev/cu.')]
    return found[0] if len(found) == 1 else fallback


M9_PORT = _find('MESHMESH_M9', 0x1a86, 0x7522, '/dev/cu.wchusbserial10')
