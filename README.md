# NessoN1-Meshcore-Firmware

MeshCore firmware for the Arduino Nesso N1, tuned for the board's ESP32-C6,
SX1262 LoRa radio, display, buttons, battery monitor, and PI4IOE5V6408 I/O
expanders.

The recommended build is now the Nesso N1 mode-switching smart companion firmware:
one image with USB serial, Bluetooth LE, local diagnostics, radio behavior
presets, and safe OTA support available as an alternate target. A dedicated
Wi-Fi/TCP companion target is included for TCP workflows.

## What This Firmware Does

- Adds a dedicated Arduino Nesso N1 PlatformIO board definition for ESP32-C6
  with 16 MB flash.
- Enables the onboard SX1262 LoRa radio on the Nesso N1 pinout with TCXO, RF
  switch, LNA, current-limit, and 22 dBm TX-power settings.
- Drives the Nesso display, buzzer, buttons, battery-voltage cache, VIN detect,
  poweroff line, radio power, and LoRa antenna controls through the board
  expanders.
- Shares the ESP32-C6 SPI bus safely between LovyanGFX and RadioLib.
- Provides companion firmware over BLE, USB serial, and Wi-Fi/TCP targets. The
  recommended smart image keeps Wi-Fi auto-start disabled to preserve ESP32-C6
  BLE stability.
- Adds `Nesso Doctor` diagnostics on the display, CLI, and safe OTA `/doctor`
  endpoint.
- Adds `preset range`, `preset balanced`, and `preset dense` radio behavior
  profiles for field tuning without reflashing.
- Hardens public Nesso builds by disabling serial private-key import/export by
  default and replacing legacy repeater/room default passwords on first boot.

## Ready-To-Flash Firmware

Recommended mode-switching smart companion image:

```text
artifacts/nesso_n1_companion_radio_smart/firmware-merged.bin
```

SHA-256:

```text
7a72c5081cada7ec415f3e153f6cafe543032366ad7e0351193dd5bfbc12e1df
```

Optional safe-OTA image:

```text
artifacts/nesso_n1_companion_radio_smart_ota/firmware-merged.bin
```

SHA-256:

```text
264136be8fb00d271d6f394ab6fb7ef167b7c5a06169e166e3feaae285f83649
```

Lean BLE-only compatibility image:

```text
artifacts/nesso_n1_companion_radio_ble/firmware-merged.bin
```

SHA-256:

```text
41c408a327d527abb6605aa0d58843728c30d2cc099da0327a0c6d0d3a628bab
```

## Fast Flash

1. Connect the Nesso N1 by USB.
2. Find the serial port:

   ```bash
   ls /dev/cu.usbmodem*
   ```

3. Flash the merged binary at offset `0x0`:

   ```bash
   esptool.py --chip esp32c6 --port /dev/cu.usbmodemXXXX --baud 460800 write_flash 0x0 artifacts/nesso_n1_companion_radio_smart/firmware-merged.bin
   ```

Replace `/dev/cu.usbmodemXXXX` with your detected port.

PlatformIO upload:

```bash
pio run -e Nesso_N1_companion_radio_smart -t upload --upload-port /dev/cu.usbmodemXXXX
```

## Connectivity

The recommended smart companion image exposes the stable companion paths first:

- USB serial: direct local console and companion transport.
- Bluetooth LE: advertises as `MeshCore-...`; connect from the MeshCore app's
  BLE discovery flow instead of pairing in OS Bluetooth settings.

Wi-Fi/TCP support remains available in the repository through
`Nesso_N1_companion_radio_wifi` using AP SSID `NessoN1-MeshCore`, password
`NessoMesh123`, and TCP port `5000`. The smart image can also reboot into Wi-Fi
mode with `mode wifi` or `start wifi`. It no longer auto-starts the Wi-Fi AP in
BLE mode because the ESP32-C6 controller ran out of Bluetooth HCI memory during
BLE/app connection attempts when Wi-Fi started in the background.

The transport selector prefers the last active path, then connected BLE, then
connected Wi-Fi/TCP when that transport is present, then recently active USB.
This lets the same codebase work as a phone BLE node, a USB desktop node, or a
Wi-Fi/TCP field node.

## Useful Commands

Run these from a MeshCore CLI-capable client or serial console:

```text
mode
mode ble
mode wifi
start ble
start wifi
doctor
nesso doctor
get board.doctor
get pwrmgt.source
get pwrmgt.bootmv
preset range
preset balanced
preset dense
start ota
```

`start ota` only starts the web updater in the `Nesso_N1_companion_radio_smart_ota`
build. Hold `KEY1` while running the command; the firmware then creates an AP
named `NessoN1-OTA-XXXX` and prints the generated password and `/update` URL.

`mode ble` and `mode wifi` save the companion mode and reboot. BLE mode keeps
Wi-Fi off for stable phone/app pairing while LoRa keeps running. Wi-Fi mode
starts the AP/TCP companion path while LoRa keeps running and BLE stays off.
Holding `KEY1` at boot temporarily forces Wi-Fi mode; holding `KEY2` at boot
temporarily forces BLE mode.

For host-side smoke and mode proof without using the physical rescue gesture,
use the framed USB helper:

```bash
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --port /dev/cu.usbmodemXXXX mode query
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --port /dev/cu.usbmodemXXXX mode wifi
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --port /dev/cu.usbmodemXXXX mode ble
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --port /dev/cu.usbmodemXXXX boot-capture --seconds 20
```

For host BLE proof, install the optional BLE client dependency in the local venv
and query over the MeshCore UART GATT service:

```bash
../.venv-platformio/bin/python -m pip install bleak
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py self-test
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --ble --timeout 12 mode query
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --ble --timeout 15 smoke
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --json --ble --timeout 15 smoke
```

`smoke` sends the client-startup companion frames `CMD_DEVICE_QUERY`,
`CMD_APP_START`, `CMD_GET_BATT_AND_STORAGE`, and the Nesso mode query. Use
`--json` when saving proof artifacts or feeding the result to another script.

The helper can also prove the Wi-Fi/TCP companion frame path after the host is
associated with the Nesso AP:

```bash
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --tcp 192.168.4.1:5000 mode query
```

Joining `NessoN1-MeshCore` makes the host leave its normal Wi-Fi network while
the test runs.

## How To Use

1. Flash `artifacts/nesso_n1_companion_radio_smart/firmware-merged.bin`.
2. Reboot the Nesso N1.
3. Open a MeshCore client:
   - Web client: https://app.meshcore.nz
   - Android app: https://play.google.com/store/apps/details?id=com.liamcottle.meshcore.android
   - iOS app: https://apps.apple.com/us/app/meshcore/id6742354151
4. Connect to the BLE device named `MeshCore-...`, or connect over USB if your
   client workflow supports it.
5. If your phone previously paired at the OS level, remove that old pairing and
   connect from inside the MeshCore app.
6. Configure your node name, channels, contacts, and LoRa settings.
7. Send an advert or message to confirm radio operation.
8. Run `doctor` if the display, radio, battery, or expander state looks wrong.

Default Nesso N1 radio parameters:

```text
Frequency: 910.525 MHz
Bandwidth: 62.5 kHz
Spreading factor: 7
Coding rate: 5
TX power: 22 dBm
```

Use only frequencies and power levels legal in your region.

For more detail, see [docs/NESSO_N1_HOW_TO_USE.md](docs/NESSO_N1_HOW_TO_USE.md).

## Build Targets

```bash
pio run -e Nesso_N1_companion_radio_usb
pio run -e Nesso_N1_companion_radio_ble
pio run -e Nesso_N1_companion_radio_wifi
pio run -e Nesso_N1_companion_radio_smart
pio run -e Nesso_N1_companion_radio_smart_ota
pio run -e Nesso_N1_repeater
pio run -e Nesso_N1_room_server
pio run -e Nesso_N1_kiss_modem
```

Regenerate merged smart images:

```bash
pio run -e Nesso_N1_companion_radio_smart \
        -e Nesso_N1_companion_radio_smart_ota \
        -t mergebin
```

Full local verification completed before publishing:

```bash
pio run -e Nesso_N1_companion_radio_usb \
        -e Nesso_N1_companion_radio_ble \
        -e Nesso_N1_companion_radio_wifi \
        -e Nesso_N1_companion_radio_smart \
        -e Nesso_N1_companion_radio_smart_ota \
        -e Nesso_N1_repeater \
        -e Nesso_N1_room_server \
        -e Nesso_N1_kiss_modem

pio test -e native
```

Result: all eight Nesso N1 firmware targets built successfully, both smart
merged images were regenerated with explicit DIO flash settings, the SHA256
manifests passed, and the native test target passed. The attached Nesso N1 also
booted the smart image through `[NESSO_BOOT] setup complete` and started BLE
advertising on USB serial capture.

## Project Layout

- `boards/arduino_nesso_n1.json` - PlatformIO board manifest for the Nesso N1.
- `variants/arduino_nesso_n1/` - Nesso N1 board support, display, expander, and
  radio target code.
- `artifacts/nesso_n1_companion_radio_smart/` - recommended mode-switching smart
  merged flash image and checksums.
- `artifacts/nesso_n1_companion_radio_smart_ota/` - KEY1-gated safe OTA merged
  flash image and checksums.
- `artifacts/nesso_n1_companion_radio_ble/` - lean BLE-only compatibility image.
- `examples/companion_radio/` - MeshCore companion firmware application.
- `src/` - MeshCore core library and shared helpers.

## Hardware Notes

The Nesso N1 firmware support was cross-checked against:

- Arduino Nesso N1 documentation: https://docs.arduino.cc/hardware/nesso-n1
- Arduino Nesso N1 pinout and schematic PDFs.
- M5Stack Unit C6L / Stamp C6 LoRa documentation for the SX1262 radio module.
- PI4IOE5V6408 I/O expander datasheet.

## Troubleshooting

No BLE device appears:

- Reboot the Nesso N1 after flashing.
- Keep it near the phone or computer during the first pairing.
- Prefer the MeshCore client's BLE discovery flow over pairing from the
  operating system Bluetooth settings.
- Look for a `MeshCore-...` device name, not `Nesso`.
- Use the PIN shown on the Nesso display.
- Run `doctor` from USB serial if the board is reachable but BLE is not.

Wi-Fi/TCP companion access is needed:

- Use `Nesso_N1_companion_radio_wifi`; the recommended smart image intentionally
  does not auto-start Wi-Fi.
- Join SSID `NessoN1-MeshCore` with password `NessoMesh123`.
- Connect to TCP port `5000`.

Radio init fails during development:

- Run `doctor` or `get board.doctor`.
- Confirm `P_LCD_CS` and `P_LORA_NSS` are both driven high before SPI traffic.
- Confirm the PI4IOE expander enables the LoRa power/reset/RF-control lines.

Flash fails:

- Make sure no serial monitor is holding the USB port.
- Press reset and retry.
- Use the correct ESP32-C6 port, usually `/dev/cu.usbmodem*` on macOS.

## Attribution

This project is based on MeshCore, originally released by Scott Powell /
rippleradios.com under the MIT License. The Nesso N1 board support and verified
firmware artifacts in this repository are packaged for the Arduino Nesso N1.

MeshCore upstream documentation and clients:

- https://docs.meshcore.io
- https://meshcore.io/flasher
- https://github.com/meshcore-dev/MeshCore

## License

MIT License. See [license.txt](license.txt).
