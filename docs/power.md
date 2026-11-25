# Power saving

Power handling lives in `src/Power.cpp`. Two mechanisms are used; neither has been measured as a current or
battery-life gain.

**Idle task waits.** With the screen off, a ready idle radio and no active radar/CSI or diagnostic probes, the
application task waits up to 20 ms instead of polling every 2 ms. A LoRa interrupt notifies it without waiting for
the timeout. Keys, GPS and USB are serviced between waits. USB/BLE commands or key presses keep the fast loop for
1 s; transmission and queued sends keep it too.

This blocks a FreeRTOS task rather than forcibly powering down the CPU. Other tasks and BLE/Wi-Fi continue running.
Actual CPU sleep depends on the core, drivers, USB and peripherals.

**CPU clock.** The ESP32-S3 runs at 240 MHz while the device is in use and drops to 80 MHz otherwise. "In use"
means: the screen is lit, the Wi-Fi access point or client is on, the radar, CSI or a Wi-Fi/BLE probe is active, a
key, USB or BLE command arrived in the last 10 s, or the Wi-Fi driver or Bluetooth controller has been started.
The clock never changes under a running Wi-Fi driver or Bluetooth controller (once started they stay up until a
reboot): changing it there caused panics on the M9. There is no light sleep: LoRa reception stays continuous.

`status` fields `idle_waits`, `idle_wait_ms` and `idle_radio_events` count task waits, their elapsed time and LoRa
notifications received while waiting. `cpu_mhz` is the current clock and `slow_ms` the time spent at 80 MHz. They do
not measure current or CPU sleep. (`sleeps` and `sleep_ms` remain in `status` from the removed light-sleep path and
stay 0.)

`python tools/power_check.py --port PORT --peer PEER_PORT` temporarily changes only the screen timeout, checks real
advert reception during idle, USB and simulated UI wake-up, then restores the setting.
`python tools/power_pair_check.py PORT1 PORT2` checks three direct messages and ACKs in each direction without
receiver USB polling until the ACK.

## Power off

The device can be switched off in software: the "Turn off" row in the M9 settings (OK twice), the "Turn off
device" button in the settings of the web page, and the USB/BLE command `poweroff`. After 1.5 s (the reply leaves;
a packet on air gets up to 5 s more) the firmware flushes delayed writes (contacts, the wardrive log),
shows "Device is off" and how to turn it on for 3 s, puts the transceiver to sleep, turns the screen, GPS and
peripheral supply off and enters ESP32 deep sleep.

The M9 has no button on a wake-capable GPIO: it is turned on again with RESET or the power slider.

The current while off has not been measured.

## Not implemented

[EasySkyMesh](https://github.com/IoTThinks/EasySkyMesh/wiki/PowerSaving#3-test-results) informed this work. RXPS
(duty-cycled LoRa reception) and scheduled GPS power cycling are not implemented. Measurements of other firmware do
not establish results for this one.
