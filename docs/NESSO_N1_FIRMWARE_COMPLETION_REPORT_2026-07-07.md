# Nesso N1 Firmware Completion Report - 2026-07-07

## Scope

This pass continued the Nesso N1 MeshCore firmware lane after the earlier
mode-switching work. The target was the host-buildable and attached-device
firmware path, not RF validation. The attached device was available on
`/dev/cu.usbmodem1101`.

No RF packet proof is claimed here. The hardware proof below is boot and local
USB serial evidence only.

## Firmware Changes

- Guarded ESP32 BLE and Wi-Fi companion transports so callbacks, writes, and
  receive polling do not run after a transport is disabled.
- Cleared connection flags, partial frame headers, and queued frames on transport
  disable.
- Added Wi-Fi payload draining for malformed or oversized frames before stream
  reuse or close.
- Retried Nesso PI4IOE5V6408 expander initialization and made board helpers
  work with per-bank expander availability.
- Changed Nesso expander and fuel-gauge register reads from repeated-start
  transactions to STOP-before-read transactions to avoid the ESP32-C6
  `i2cWriteReadNonStop` error path observed during boot.
- Kept USB rescue preset writes limited to fields present in the current
  companion `NodePrefs` type.
- Added Nesso smart companion command `0x2C` so a host can query/set saved BLE
  or Wi-Fi mode over the framed USB companion protocol without physical
  CLI-rescue button timing.
- Added `tools/nesso_n1_companion_mode.py` for mode query/set, reconnecting USB
  boot capture, USB/BLE companion smoke, BLE GATT mode query, and Wi-Fi/TCP mode
  query.
- Added helper `self-test` and `--json` output for hardware-free parser checks
  and machine-readable USB/BLE proof captures.

## Build And Test Commands

```bash
../.venv-platformio/bin/pio test -e native
```

Result:

```text
Processing test_utils in native environment
native:test_utils [PASSED]
0 test cases: 0 succeeded
```

The native target is a smoke gate only because it reports zero individual test
cases.

```bash
../.venv-platformio/bin/pio run \
  -e Nesso_N1_companion_radio_usb \
  -e Nesso_N1_companion_radio_ble \
  -e Nesso_N1_companion_radio_wifi \
  -e Nesso_N1_companion_radio_smart \
  -e Nesso_N1_companion_radio_smart_ota \
  -e Nesso_N1_repeater \
  -e Nesso_N1_room_server \
  -e Nesso_N1_kiss_modem
```

Result:

```text
Nesso_N1_companion_radio_usb        SUCCESS   00:00:06.857
Nesso_N1_companion_radio_ble        SUCCESS   00:00:07.055
Nesso_N1_companion_radio_wifi       SUCCESS   00:00:07.206
Nesso_N1_companion_radio_smart      SUCCESS   00:00:05.646
Nesso_N1_companion_radio_smart_ota  SUCCESS   00:00:08.348
Nesso_N1_repeater                   SUCCESS   00:00:05.467
Nesso_N1_room_server                SUCCESS   00:00:05.348
Nesso_N1_kiss_modem                 SUCCESS   00:00:05.459
8 succeeded in 00:00:51.386
```

## Artifact Refresh

Final smart artifacts were copied from `.pio/build/Nesso_N1_companion_radio_smart`.
Final safe-OTA artifacts were copied from
`.pio/build/Nesso_N1_companion_radio_smart_ota`.

The merged images were regenerated with explicit DIO flash settings:

```bash
../.venv-platformio/bin/python /Users/ryan/.platformio/packages/tool-esptoolpy/esptool.py \
  --chip esp32c6 merge_bin \
  -o artifacts/nesso_n1_companion_radio_smart/firmware-merged.bin \
  --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0000 artifacts/nesso_n1_companion_radio_smart/bootloader.bin \
  0x8000 artifacts/nesso_n1_companion_radio_smart/partitions.bin \
  0xe000 artifacts/nesso_n1_companion_radio_smart/boot_app0.bin \
  0x10000 artifacts/nesso_n1_companion_radio_smart/firmware.bin
```

```bash
../.venv-platformio/bin/python /Users/ryan/.platformio/packages/tool-esptoolpy/esptool.py \
  --chip esp32c6 merge_bin \
  -o artifacts/nesso_n1_companion_radio_smart_ota/firmware-merged.bin \
  --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0000 artifacts/nesso_n1_companion_radio_smart_ota/bootloader.bin \
  0x8000 artifacts/nesso_n1_companion_radio_smart_ota/partitions.bin \
  0xe000 artifacts/nesso_n1_companion_radio_smart_ota/boot_app0.bin \
  0x10000 artifacts/nesso_n1_companion_radio_smart_ota/firmware.bin
```

Merge results:

```text
artifacts/nesso_n1_companion_radio_smart/firmware-merged.bin      0x227230 bytes
artifacts/nesso_n1_companion_radio_smart_ota/firmware-merged.bin  0x244040 bytes
```

Manifest verification:

```bash
cd artifacts/nesso_n1_companion_radio_smart
shasum -a 256 -c SHA256SUMS
```

```text
bootloader.bin: OK
partitions.bin: OK
boot_app0.bin: OK
firmware.bin: OK
firmware-merged.bin: OK
```

```bash
cd artifacts/nesso_n1_companion_radio_smart_ota
shasum -a 256 -c SHA256SUMS
```

```text
bootloader.bin: OK
partitions.bin: OK
boot_app0.bin: OK
firmware.bin: OK
firmware-merged.bin: OK
```

Final hashes:

```text
9c2124effaac9985c00ffa3caf64e179fed73d0522a81ab030366386685ecf79  bootloader.bin
bd0f7954aca2ef7d925ee21aaa1f3dc8822d1d6ce5cbbd26a135e5886bfff6ce  partitions.bin
f94c5d786a7a8fab06ac5d10e33bf37711a6697636dc037559ea19cc410a17f0  boot_app0.bin
d528642ef6188624b91696dc2bc64f7aa4750b3a27564681f05b97fa4be4a7e3  nesso_n1_companion_radio_smart/firmware.bin
7a72c5081cada7ec415f3e153f6cafe543032366ad7e0351193dd5bfbc12e1df  nesso_n1_companion_radio_smart/firmware-merged.bin
3859e61e17d2ba4152e12d5cfacfa74870aab369b1305ce1dcc02d1872154fd3  nesso_n1_companion_radio_smart_ota/firmware.bin
264136be8fb00d271d6f394ab6fb7ef167b7c5a06169e166e3feaae285f83649  nesso_n1_companion_radio_smart_ota/firmware-merged.bin
```

## Device Flash And Boot Proof

Device discovery:

```bash
ls -1 /dev/cu.*
```

Relevant result:

```text
/dev/cu.usbmodem1101
```

Earlier direct PlatformIO/esptool uploads at high baud were unreliable on this
USB-JTAG serial path. The successful write path used manual USB-JTAG reset and
small app-slot chunks at 115200 baud with verified writes. The final app-slot
write completed all chunks through the last chunk and the boot-app partition was
restored. The final reset after chunk flashing hit the same serial instability,
so boot proof was captured separately after hard reset.

Initial captures that held the same serial session only showed ROM and bootloader
handoff because USB CDC re-enumerated between bootloader and app:

```text
ESP-ROM:esp32c6-20220919
rst:0x15 (USB_UART_HPSYS),boot:0xc (SPI_FAST_FLASH_BOOT)
mode:DIO, clock div:2
entry 0x4086c110
```

A reconnecting capture showed the app had reached storage startup:

```text
[NESSO_BOOT] rng ready
[NESSO_BOOT] spiffs begin
```

The final tight reconnecting capture reached complete startup:

```text
[NESSO_BOOT] setup start
[NESSO_BOOT] board begin
[NESSO_BOOT] rng ready
[NESSO_BOOT] spiffs begin
[NESSO_BOOT] store begin
[NESSO_BOOT] mesh begin
[NESSO_BOOT] mesh ready
[NESSO_BOOT] serial begin ready
Nesso: BLE advertising started
[NESSO_BOOT] serial enabled
[NESSO_BOOT] sensors begin
[NESSO_BOOT] setup complete
```

The updated v0.2.5 app image was then flashed to the app slot with the same
verified low-baud chunked path. Direct 16 KiB writes still failed after the first
chunk, so the resume used verified 4 KiB writes from offset `0x14000` through the
end of the app image.

After flashing v0.2.5, the helper proved BLE query over USB, saved Wi-Fi mode,
Wi-Fi-mode boot, Wi-Fi/TCP companion query, saved BLE mode, BLE-mode boot, and
USB/BLE GATT companion smoke:

```bash
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --port /dev/cu.usbmodem1101 mode query
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --port /dev/cu.usbmodem1101 --timeout 8 smoke
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --json --port /dev/cu.usbmodem1101 --timeout 8 smoke
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --port /dev/cu.usbmodem1101 mode wifi
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --port /dev/cu.usbmodem1101 boot-capture --seconds 20
networksetup -setairportnetwork en0 NessoN1-MeshCore NessoMesh123
ping -c 1 -W 1000 192.168.4.1
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --tcp 192.168.4.1:5000 --timeout 5 mode query
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --port /dev/cu.usbmodem1101 mode query
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --port /dev/cu.usbmodem1101 mode ble
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --port /dev/cu.usbmodem1101 boot-capture --seconds 20
../.venv-platformio/bin/python -m pip install bleak
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py self-test
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --json self-test
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --ble --timeout 12 mode query
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --ble --timeout 15 smoke
../.venv-platformio/bin/python tools/nesso_n1_companion_mode.py --json --ble --timeout 15 smoke
```

Results:

```text
mode=ble
mode=wifi
[NESSO_BOOT] smart serial begin mode=wifi forced=0 wifi_auto=0 tcp=5000
Nesso: WiFi AP started: NessoN1-MeshCore, TCP 5000
[NESSO_BOOT] setup complete
en0 inet 192.168.4.2
64 bytes from 192.168.4.1
mode=wifi
mode=wifi
mode=ble
[NESSO_BOOT] smart serial begin mode=ble forced=0 wifi_auto=0 tcp=5000
Nesso: BLE advertising started
[NESSO_BOOT] setup complete
mode=ble
usb smoke:
  firmware_code=13
  model=Arduino Nesso N1
  version=v1.16.0
  node_name=MrWonderful
  freq_khz=910525
  bw_mhz=62.5
  sf=7
  cr=5
  battery_mv=4141
  storage_used_kb=3
  storage_total_kb=3169
  mode=ble
ble smoke:
  firmware_code=13
  model=Arduino Nesso N1
  version=v1.16.0
  node_name=MrWonderful
  freq_khz=910525
  bw_mhz=62.5
  sf=7
  cr=5
  battery_mv=4141
  storage_used_kb=3
  storage_total_kb=3169
  mode=ble
self-test:
  status=ok
  checks=frame_payload, mode_payloads, mode_response, device_info_parse, self_info_parse, battery_parse
usb json:
  transport=usb
  model=Arduino Nesso N1
  node_name=MrWonderful
  mode=ble
ble json:
  transport=ble
  model=Arduino Nesso N1
  node_name=MrWonderful
  mode=ble
```

The first TCP set-back attempt timed out after macOS roamed off the Nesso AP, so
the device was returned to BLE mode over USB instead. Joining `NessoN1-MeshCore`
also takes the Mac off its normal Wi-Fi internet path while the test runs.

Conclusion: the attached Nesso N1 boots the smart firmware to `setup complete`,
starts BLE advertising in saved BLE mode, starts the Wi-Fi AP/TCP companion path
in saved Wi-Fi mode, answers a framed companion mode query over TCP, and answers
client-startup companion frames over USB and BLE GATT. This is local
boot/startup plus host USB/BLE/TCP companion proof, not proof of the official
phone app workflow, LoRa receive/transmit behavior, or RF mesh delivery.

## Remaining Hardware Work

- Stabilize the flash/monitor transport by using physical BOOT/RESET access, a
  different cable/hub, or a JTAG/debug setup, then replace the chunked flash
  workaround with a repeatable one-command flash path.
- Capture official MeshCore phone-app BLE workflow separately from host BLE GATT
  proof.
- Capture richer Wi-Fi companion behavior separately, such as app handshake and
  contact/channel sync over TCP.
- Run the first RF bridge proof as a separate two-node task, likely Nesso N1 plus
  T-Deck Plus, with explicit firmware images, serial ports, frequency plan, and
  packet acceptance criteria.
  Current attached serial devices only showed `/dev/cu.usbmodem1101` as the
  Nesso device, so two-node RF proof was not available in this pass.
