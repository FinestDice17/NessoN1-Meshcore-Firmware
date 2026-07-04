# Nesso N1 MeshCore Firmware: How To Use

This guide covers flashing, pairing, and operating the Nesso N1 with the
verified MeshCore BLE companion firmware in this repository.

## What You Need

- Arduino Nesso N1.
- USB-C data cable.
- A computer with Python and `esptool.py`, or PlatformIO.
- A MeshCore client:
  - Web: https://app.meshcore.nz
  - Android: https://play.google.com/store/apps/details?id=com.liamcottle.meshcore.android
  - iOS: https://apps.apple.com/us/app/meshcore/id6742354151

## Recommended Firmware

Use the merged BLE companion binary:

```text
artifacts/nesso_n1_companion_radio_ble/firmware-merged.bin
```

This file includes the bootloader, partition table, boot app image, and
application firmware. Flash it at offset `0x0`.

## Flash With esptool

Install esptool if needed:

```bash
python3 -m pip install esptool
```

Connect the Nesso N1 and find the port:

```bash
ls /dev/cu.usbmodem*
```

Flash:

```bash
esptool.py --chip esp32c6 --port /dev/cu.usbmodemXXXX --baud 460800 write_flash 0x0 artifacts/nesso_n1_companion_radio_ble/firmware-merged.bin
```

After flashing, the board should reboot automatically. If it does not, press
reset or unplug/replug USB.

## Flash With PlatformIO

From the repository root:

```bash
pio run -e Nesso_N1_companion_radio_ble -t upload --upload-port /dev/cu.usbmodemXXXX
```

To build without flashing:

```bash
pio run -e Nesso_N1_companion_radio_ble
```

To regenerate the merged binary:

```bash
pio run -e Nesso_N1_companion_radio_ble -t mergebin
```

## Pair Over BLE

1. Reboot the Nesso N1.
2. Open the MeshCore client.
3. Start BLE discovery.
4. Select the device named `MeshCore-...`.
5. When prompted for a pairing PIN, use the PIN shown on the Nesso display.
6. Wait for the client to report that it is connected.

The exact BLE name depends on the node identity stored on the device. The
verified unit advertised as:

```text
MeshCore-7599DA71
```

Your unit may use a different suffix.

## First Configuration

After pairing:

1. Set a readable node name.
2. Check the LoRa frequency and region settings.
3. Add or confirm the public channel.
4. Add contacts or scan for nearby nodes.
5. Send an advert from the client or from the Nesso UI.
6. Send a test message to a nearby MeshCore node.

The current default radio parameters are:

```text
Frequency: 910.525 MHz
Bandwidth: 62.5 kHz
Spreading factor: 7
Coding rate: 5
TX power: 22 dBm
```

Use only radio settings legal in your country and band plan.

## What The Nesso Buttons Do

The companion UI is based on MeshCore's display companion interface. The user
button can be used to navigate the UI, toggle companion connectivity, send
adverts, and interact with the display prompts. Exact behavior can vary as the
upstream MeshCore UI evolves.

## USB Serial Monitor

For boot diagnostics:

```bash
pio device monitor --port /dev/cu.usbmodemXXXX --baud 115200
```

Expected BLE startup lines in this Nesso build:

```text
BLE: begin device=MeshCore-...
BLE: advertising started
```

If you see a radio init failure, rebuild from the current source and confirm
that `variants/arduino_nesso_n1/target.cpp` uses the global `SPI` instance for
RadioLib.

## Build All Nesso Targets

```bash
pio run -e Nesso_N1_companion_radio_usb \
        -e Nesso_N1_companion_radio_ble \
        -e Nesso_N1_repeater \
        -e Nesso_N1_room_server \
        -e Nesso_N1_kiss_modem
```

Run native tests:

```bash
pio test -e native
```

## Recovery Tips

- If the serial port disappears, unplug and reconnect USB.
- If flashing cannot connect, close serial monitors and retry.
- If BLE pairing fails, remove the old pairing from your phone/computer and
  pair again using the current display PIN.
- If the display is on but BLE does not appear, wait 10 seconds after reboot and
  rescan.

## Firmware Checksums

Checksums are stored in:

```text
artifacts/nesso_n1_companion_radio_ble/SHA256SUMS
```

Verify locally:

```bash
shasum -a 256 -c artifacts/nesso_n1_companion_radio_ble/SHA256SUMS
```
