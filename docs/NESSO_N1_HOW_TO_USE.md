# Nesso N1 MeshCore Firmware: How To Use

This guide covers flashing, connecting, diagnosing, and operating the Arduino
Nesso N1 with the MeshCore firmware in this repository.

## What You Need

- Arduino Nesso N1.
- USB-C data cable.
- A computer with Python and `esptool.py`, or PlatformIO.
- A MeshCore client:
  - Web: https://app.meshcore.nz
  - Android: https://play.google.com/store/apps/details?id=com.liamcottle.meshcore.android
  - iOS: https://apps.apple.com/us/app/meshcore/id6742354151

## Recommended Firmware

Use the smart companion merged binary:

```text
artifacts/nesso_n1_companion_radio_smart/firmware-merged.bin
```

This image enables USB serial and BLE companion access with Wi-Fi auto-start
disabled for ESP32-C6 BLE stability. It includes the bootloader, partition
table, boot app image, and application firmware. Flash it at offset `0x0`.

Use this OTA-capable image only when you specifically want KEY1-gated web OTA:

```text
artifacts/nesso_n1_companion_radio_smart_ota/firmware-merged.bin
```

## Flash With esptool

Install esptool if needed:

```bash
python3 -m pip install esptool
```

Connect the Nesso N1 and find the port:

```bash
ls /dev/cu.usbmodem*
```

Flash the recommended smart firmware:

```bash
esptool.py --chip esp32c6 --port /dev/cu.usbmodemXXXX --baud 460800 write_flash 0x0 artifacts/nesso_n1_companion_radio_smart/firmware-merged.bin
```

After flashing, the board should reboot automatically. If it does not, press
reset or unplug/replug USB.

## Flash With PlatformIO

From the repository root:

```bash
pio run -e Nesso_N1_companion_radio_smart -t upload --upload-port /dev/cu.usbmodemXXXX
```

To build without flashing:

```bash
pio run -e Nesso_N1_companion_radio_smart
```

To regenerate merged binaries:

```bash
pio run -e Nesso_N1_companion_radio_smart \
        -e Nesso_N1_companion_radio_smart_ota \
        -t mergebin
```

## Connect Over BLE

1. Reboot the Nesso N1.
2. Open the MeshCore client.
3. Start BLE discovery.
4. Select the device named `MeshCore-...`.
5. Do not pair from the phone or computer operating-system Bluetooth settings.
6. Wait for the client to report that it is connected.

The exact BLE name depends on the node identity stored on the device. A fresh or
restored unit will use a different suffix.

If you already tried OS-level Bluetooth pairing, remove that old pairing before
connecting from the MeshCore app.

## Connect Over Wi-Fi/TCP

The recommended smart firmware intentionally does not auto-start the Wi-Fi AP in
BLE mode. That older behavior could destabilize BLE on the ESP32-C6 while the
MeshCore app was connecting.

To switch the smart firmware into Wi-Fi/TCP mode, open USB CLI rescue or any
available CLI-capable companion path, then run:

```text
mode wifi
```

The command saves Wi-Fi mode and reboots. To return to Bluetooth:

```text
mode ble
```

Holding `KEY1` at boot temporarily forces Wi-Fi mode. Holding `KEY2` at boot
temporarily forces BLE mode.

To enter USB CLI rescue from the Nesso UI, reboot the device and long-press the
user button during the first 8 seconds, then type commands in a 115200 baud USB
serial monitor.

For host-side proof without the physical rescue gesture, use the framed USB
helper:

```bash
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --port /dev/cu.usbmodemXXXX mode query
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --port /dev/cu.usbmodemXXXX mode wifi
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --port /dev/cu.usbmodemXXXX mode ble
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --port /dev/cu.usbmodemXXXX boot-capture --seconds 20
```

The helper uses companion command `0x2C`: query is `2C`, set BLE is `2C 01`,
and set Wi-Fi is `2C 02`.

For BLE GATT proof from a Mac or other host with Python BLE support, install the
optional dependency and query over the MeshCore UART service:

```bash
../.venv-platformio/bin/python -m pip install bleak
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py self-test
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --ble --timeout 12 mode query
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --ble --timeout 15 smoke
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --json --ble --timeout 15 smoke
```

The `smoke` command sends the same startup-style companion frames a client uses
to identify the device and read status: device query, app start, battery/storage,
and mode query. Use `--json` for machine-readable proof output. The `self-test`
command validates the helper's parsers and frame builders without hardware.

After the host is associated with `NessoN1-MeshCore`, the same helper can prove
the Wi-Fi/TCP companion frame path:

```bash
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --tcp 192.168.4.1:5000 mode query
```

Joining the Nesso AP takes the host off its normal Wi-Fi internet connection for
the duration of the test.

You can also use the dedicated Wi-Fi/TCP target when you want a Wi-Fi-only
companion build:

```bash
pio run -e Nesso_N1_companion_radio_wifi -t upload --upload-port /dev/cu.usbmodemXXXX
```

```text
SSID: NessoN1-MeshCore
Password: NessoMesh123
TCP port: 5000
```

Connect your computer or client device to that AP, then use a MeshCore
TCP-capable workflow pointed at port `5000`.

## USB Serial Console

For boot diagnostics and CLI access:

```bash
pio device monitor --port /dev/cu.usbmodemXXXX --baud 115200
```

Useful commands:

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

`doctor` reports expander, battery, USB/battery power source, LoRa enable/LNA,
NSS/BUSY/DIO1, key state, and uptime. On the Nesso display, open the Nesso
Doctor page for the same board-level health view.

## Radio Presets

`preset balanced` restores a general-purpose profile with boosted receive gain
and normal mesh behavior.

`preset range` biases toward field/range use by keeping boosted receive gain and
enabling multi-ACK behavior.

`preset dense` biases toward busy local meshes by reducing receive gain and
adding more delay before forwarding.

Presets are saved to preferences and take effect without reflashing.

## Safe OTA

Safe OTA is intentionally not enabled in the default smart image. Flash the OTA
variant first:

```bash
esptool.py --chip esp32c6 --port /dev/cu.usbmodemXXXX --baud 460800 write_flash 0x0 artifacts/nesso_n1_companion_radio_smart_ota/firmware-merged.bin
```

To start OTA:

1. Open USB serial or a CLI-capable companion connection.
2. Hold `KEY1`.
3. Run `start ota`.
4. Join the printed `NessoN1-OTA-XXXX` Wi-Fi AP.
5. Open the printed `/update` URL.

The OTA AP password is generated from the device MAC. The `/doctor` endpoint is
also available while OTA is running.

## First Configuration

After pairing or connecting:

1. Set a readable node name.
2. Check the LoRa frequency and region settings.
3. Add or confirm the public channel.
4. Add contacts or scan for nearby nodes.
5. Send an advert from the client or from the Nesso UI.
6. Send a test message to a nearby MeshCore node.

Default Nesso N1 radio parameters:

```text
Frequency: 910.525 MHz
Bandwidth: 62.5 kHz
Spreading factor: 7
Coding rate: 5
TX power: 22 dBm
```

Use only radio settings legal in your country and band plan.

## Other Firmware Roles

Buildable Nesso N1 roles:

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

Repeater and room-server builds replace legacy default passwords on first boot
when the Nesso hardening flag is enabled:

```text
Repeater/admin: n1-XXXXXXXX
Room guest:     room-XXXXXXXX
```

`XXXXXXXX` is derived from the first four bytes of the node public key. Set your
own admin/room passwords after deployment.

## Recovery Tips

- If the serial port disappears, unplug and reconnect USB.
- If flashing cannot connect, close serial monitors and retry.
- If BLE pairing fails, remove the old pairing from your phone/computer and
  reconnect from the MeshCore app's BLE discovery flow.
- If Wi-Fi does not appear, run `mode wifi` and let the smart firmware reboot,
  or flash `Nesso_N1_companion_radio_wifi`.
- If the display is on but BLE does not appear, wait 10 seconds after reboot and
  rescan.
- If radio behavior looks wrong, run `doctor`, then try `preset balanced`.

## Firmware Checksums

Checksums are stored beside each artifact:

```text
artifacts/nesso_n1_companion_radio_smart/SHA256SUMS
artifacts/nesso_n1_companion_radio_smart_ota/SHA256SUMS
artifacts/nesso_n1_companion_radio_ble/SHA256SUMS
```

Verify locally:

```bash
cd artifacts/nesso_n1_companion_radio_smart
shasum -a 256 -c SHA256SUMS
```
