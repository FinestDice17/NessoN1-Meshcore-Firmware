# NessoN1-Meshcore-Firmware

Production-ready MeshCore firmware for the Arduino Nesso N1, with a verified
BLE companion build and source support for the full Nesso N1 MeshCore target
set.

This repository packages the Nesso N1 board support work on top of MeshCore so
the device can be used as a compact LoRa mesh companion radio: flash it, pair
over Bluetooth LE, and use the MeshCore mobile or web clients to send messages
without relying on cellular service or internet infrastructure.

## What This Firmware Does

- Adds a dedicated Arduino Nesso N1 PlatformIO board definition for the ESP32-C6
  with 16 MB flash.
- Enables the onboard SX1262 LoRa radio with the correct Nesso N1 SPI pins,
  reset path, RF switch behavior, TCXO voltage, current limit, and 22 dBm TX
  power setting.
- Fixes Nesso's PI4IOE5V6408 I/O expander handling for radio power, radio reset,
  antenna/LNA control, buttons, LED, and power-management lines.
- Shares the ESP32-C6 SPI bus correctly between the LovyanGFX display driver and
  RadioLib so the display and LoRa radio can boot together reliably.
- Enables the Nesso display, button UI, buzzer, battery-voltage cache, and USB
  CDC console.
- Builds the MeshCore BLE companion firmware with the Nordic UART-style BLE
  service used by MeshCore clients.
- Keeps source build targets for USB companion, BLE companion, repeater, room
  server, and KISS modem firmware.

## Verified Firmware

The ready-to-flash BLE companion image is included here:

```text
artifacts/nesso_n1_companion_radio_ble/firmware-merged.bin
```

SHA-256:

```text
41c408a327d527abb6605aa0d58843728c30d2cc099da0327a0c6d0d3a628bab
```

The final verified boot log from the flashed Nesso N1 included:

```text
BLE: begin device=MeshCore-7599DA71
BLE: advertising started
```

That proves the production BLE firmware reached the MeshCore BLE companion
interface and started advertising.

## Fast Flash

1. Connect the Nesso N1 by USB.
2. Find the serial port:

   ```bash
   ls /dev/cu.usbmodem*
   ```

3. Flash the merged binary at offset `0x0`:

   ```bash
   esptool.py --chip esp32c6 --port /dev/cu.usbmodemXXXX --baud 460800 write_flash 0x0 artifacts/nesso_n1_companion_radio_ble/firmware-merged.bin
   ```

Replace `/dev/cu.usbmodemXXXX` with your detected port.

If you prefer PlatformIO:

```bash
pio run -e Nesso_N1_companion_radio_ble -t upload --upload-port /dev/cu.usbmodemXXXX
```

## How To Use

1. Flash the BLE companion firmware.
2. Reboot the Nesso N1.
3. Open a MeshCore client:
   - Web client: https://app.meshcore.nz
   - Android app: https://play.google.com/store/apps/details?id=com.liamcottle.meshcore.android
   - iOS app: https://apps.apple.com/us/app/meshcore/id6742354151
4. Connect to the BLE device named `MeshCore-...`.
5. If a PIN is requested, use the PIN shown on the Nesso display.
6. Configure your node name, channels, contacts, and LoRa settings from the app.
7. Send an advert or message to confirm radio operation.

The default Nesso N1 build is configured for:

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
pio run -e Nesso_N1_companion_radio_ble
pio run -e Nesso_N1_companion_radio_usb
pio run -e Nesso_N1_repeater
pio run -e Nesso_N1_room_server
pio run -e Nesso_N1_kiss_modem
```

Full local verification completed before publishing:

```bash
pio run -e Nesso_N1_companion_radio_usb \
        -e Nesso_N1_companion_radio_ble \
        -e Nesso_N1_repeater \
        -e Nesso_N1_room_server \
        -e Nesso_N1_kiss_modem

pio test -e native
```

Result: all five Nesso N1 firmware targets built successfully, and the native
test target passed.

## Project Layout

- `boards/arduino_nesso_n1.json` - PlatformIO board manifest for the Nesso N1.
- `variants/arduino_nesso_n1/` - Nesso N1 board support, display, expander, and
  radio target code.
- `artifacts/nesso_n1_companion_radio_ble/` - verified flash artifacts and
  checksums.
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
- Look for a `MeshCore-...` device name, not `Nesso`.
- Use the PIN shown on the Nesso display.

Radio init fails during development:

- Confirm the display and radio are sharing the global ESP32 `SPI` instance.
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
