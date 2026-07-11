# Nesso N1 MeshCore Firmware Release Notes

## v0.2.5

Host-visible companion mode proof follow-up.

### Ready-To-Flash Images

Recommended mode-switching smart companion:

```text
artifacts/nesso_n1_companion_radio_smart/firmware-merged.bin
7a72c5081cada7ec415f3e153f6cafe543032366ad7e0351193dd5bfbc12e1df
```

KEY1-gated safe OTA companion:

```text
artifacts/nesso_n1_companion_radio_smart_ota/firmware-merged.bin
264136be8fb00d271d6f394ab6fb7ef167b7c5a06169e166e3feaae285f83649
```

### Changes

- Added Nesso smart companion command `0x2C` on the framed companion protocol.
  Query returns `PACKET_OK` with mode value `0` for BLE or `1` for Wi-Fi.
  Set BLE uses payload `2C 01`; set Wi-Fi uses payload `2C 02`.
- Added `tools/nesso_n1_companion_mode.py` to query/set the mode over USB and
  run a reconnecting boot capture without relying on the physical CLI-rescue
  button timing. The same helper can query the mode over BLE GATT with `--ble`
  or over Wi-Fi/TCP with `--tcp 192.168.4.1:5000` after the host joins the
  Nesso AP.
- Extended the helper with `device-query`, `app-start`, `battery`, and `smoke`
  commands. `smoke` sends the client-startup companion frames and parses the
  device info, self info, battery/storage status, and mode response.
- Added helper `self-test` and `--json` output so host proof can be run without
  hardware and captured as machine-readable acceptance evidence.
- Verified saved Wi-Fi mode boots the smart image with the AP/TCP path started,
  accepts a TCP framed companion query, then switched the attached device back to
  saved BLE mode over USB and verified USB plus BLE GATT companion smoke.

### Verified Locally

```text
Nesso_N1_companion_radio_usb        SUCCESS
Nesso_N1_companion_radio_ble        SUCCESS
Nesso_N1_companion_radio_wifi       SUCCESS
Nesso_N1_companion_radio_smart      SUCCESS
Nesso_N1_companion_radio_smart_ota  SUCCESS
Nesso_N1_repeater                   SUCCESS
Nesso_N1_room_server                SUCCESS
Nesso_N1_kiss_modem                 SUCCESS
native:test_utils                   PASSED
```

Artifact manifests:

```text
artifacts/nesso_n1_companion_radio_smart/SHA256SUMS      all OK
artifacts/nesso_n1_companion_radio_smart_ota/SHA256SUMS  all OK
```

Hardware proof on `/dev/cu.usbmodem1101`:

```text
mode=ble
mode=wifi
[NESSO_BOOT] smart serial begin mode=wifi forced=0 wifi_auto=0 tcp=5000
Nesso: WiFi AP started: NessoN1-MeshCore, TCP 5000
[NESSO_BOOT] setup complete
host_ip=192.168.4.2
ping 192.168.4.1 ok
tcp 192.168.4.1:5000 mode=wifi
mode=wifi
mode=ble
[NESSO_BOOT] smart serial begin mode=ble forced=0 wifi_auto=0 tcp=5000
Nesso: BLE advertising started
[NESSO_BOOT] setup complete
ble gatt mode=ble
usb smoke model=Arduino Nesso N1 node_name=MrWonderful battery_mv=4141 mode=ble
ble smoke model=Arduino Nesso N1 node_name=MrWonderful battery_mv=4141 mode=ble
self-test status=ok
usb json transport=usb mode=ble
ble json transport=ble mode=ble
```

Joining the Nesso AP takes the host off its normal Wi-Fi internet path while the
test runs. No RF packet proof was claimed in this release note.

## v0.2.4

Boot hardening and transport guard follow-up for the Nesso N1 smart companion
firmware.

### Ready-To-Flash Images

Recommended mode-switching smart companion:

```text
artifacts/nesso_n1_companion_radio_smart/firmware-merged.bin
e35d2241a679e3cbfb5ec6b064250e6bcfad7f8ce2671045e1fe6d90cca47d13
```

KEY1-gated safe OTA companion:

```text
artifacts/nesso_n1_companion_radio_smart_ota/firmware-merged.bin
cf70d0937099638bfa2dd9ab977f589a137b5ffa2b945efed152891b1b89f2c0
```

### Changes

- Added disabled-state guards to the ESP32 BLE and Wi-Fi companion transports so
  stale callbacks, writes, and receive polling cannot run after a transport is
  turned off.
- Cleared BLE/Wi-Fi connection state and queued frames on disable.
- Drained malformed or oversized Wi-Fi frame payloads before closing/reusing the
  TCP stream, avoiding parser desynchronization and tight retry loops.
- Retried Nesso expander initialization and made board helpers tolerate one
  missing expander bank without blocking the whole board path.
- Changed Nesso expander and battery-gauge register reads to use STOP-before-read
  I2C transactions, avoiding the ESP32-C6 `i2cWriteReadNonStop` error path seen
  during boot.
- Kept USB rescue preset writes within the current companion `NodePrefs` shape.

### Verified Locally

```text
Nesso_N1_companion_radio_usb        SUCCESS
Nesso_N1_companion_radio_ble        SUCCESS
Nesso_N1_companion_radio_wifi       SUCCESS
Nesso_N1_companion_radio_smart      SUCCESS
Nesso_N1_companion_radio_smart_ota  SUCCESS
Nesso_N1_repeater                   SUCCESS
Nesso_N1_room_server                SUCCESS
Nesso_N1_kiss_modem                 SUCCESS
native:test_utils                   PASSED
```

Artifact manifests:

```text
artifacts/nesso_n1_companion_radio_smart/SHA256SUMS      all OK
artifacts/nesso_n1_companion_radio_smart_ota/SHA256SUMS  all OK
```

Hardware boot proof on `/dev/cu.usbmodem1101`:

```text
[NESSO_BOOT] setup complete
Nesso: BLE advertising started
```

No RF packet proof was claimed in this release note.

## v0.2.3

Reboot-based BLE/Wi-Fi companion mode switching.

### Ready-To-Flash Images

Recommended mode-switching smart companion:

```text
artifacts/nesso_n1_companion_radio_smart/firmware-merged.bin
90700c2d8b97ff2d715f72e90c74ba27f209b7107ac87dbd526b5726b0976d81
```

KEY1-gated safe OTA companion:

```text
artifacts/nesso_n1_companion_radio_smart_ota/firmware-merged.bin
e138f4b2ce8a5042133bf1f7f49878014e849f2b21fb7b3b92f941989d14c2b0
```

### Changes

- Added persistent companion modes: `mode ble` and `mode wifi` save the mode
  and reboot cleanly.
- Kept LoRa active in both companion modes. BLE mode runs BLE + USB + LoRa;
  Wi-Fi mode runs Wi-Fi AP/TCP + USB + LoRa with BLE left off.
- Added `start wifi`, `wifi start`, `start ble`, and `ble start` aliases.
- Added boot overrides: hold `KEY1` to temporarily force Wi-Fi mode, or `KEY2`
  to temporarily force BLE mode.
- Fixed the Nesso display's AP-mode IP reporting by using `WiFi.softAPIP()`;
  BLE mode now shows `MODE:BLE` instead of `IP: 0.0.0.0`.
- Changed the open-GATT BLE display state from a stale PIN prompt to `BLE Ready`.
- Kept sleep inhibited in smart mode so USB charging/power and BLE can be used
  at the same time.
- Deferred the first battery fuel-gauge read until after boot so display startup
  does not produce early I2C bus warnings.

### Verified Locally

```text
Nesso_N1_companion_radio_smart      SUCCESS
Nesso_N1_companion_radio_smart_ota  SUCCESS
```

## v0.2.2

BLE-priority stability fix for MeshCore app connection failures on ESP32-C6.

### Ready-To-Flash Images

Recommended BLE-priority smart companion:

```text
artifacts/nesso_n1_companion_radio_smart/firmware-merged.bin
5bf1b2e6c659168b3ed17ae730cafc17d5ae4b2b6d67f60e961a905398930e1f
```

KEY1-gated safe OTA companion:

```text
artifacts/nesso_n1_companion_radio_smart_ota/firmware-merged.bin
14b1d8a2d412e670fe64f3aa66ead1746e42a487e3d2195c934e8c7eff4b4e1a
```

### Changes

- Opened the BLE GATT UART characteristics for the Nesso smart build so the
  MeshCore app can read/write device info without the ESP32-C6 reporting
  `GATT_INSUF_AUTHENTICATION`.
- Disabled smart-image Wi-Fi AP auto-start by default. The earlier delayed AP
  start could exhaust Bluetooth HCI memory while the app was connecting and
  trigger `hci_driver_vhci_host_tx` asserts.
- Kept Wi-Fi/TCP companion support available through the dedicated
  `Nesso_N1_companion_radio_wifi` target.
- Lowered the Nesso I2C bus to 100 kHz and throttled battery-voltage reads to
  reduce display/button expander bus pressure.
- Added compact boot-stage and BLE lifecycle diagnostics on USB serial.

### Verified Locally

```text
Nesso_N1_companion_radio_smart      SUCCESS
Nesso_N1_companion_radio_smart_ota  SUCCESS
```

### Hardware Flash Proof

```text
Hash of data verified.
[NESSO_BOOT] setup complete
Nesso: BLE advertising started
Nesso: BLE connected conn_id=0 mtu=23
auth_errors=0
asserts=0
panics=0
reboots=0
wifi_ap_started=0
i2c_errors=0
```

## v0.2.1

BLE pairing stability hotfix for the all-connectivity smart firmware.

### Ready-To-Flash Images

Recommended all-connectivity smart companion:

```text
artifacts/nesso_n1_companion_radio_smart/firmware-merged.bin
ec56cdc29d7f925316431b9e5ad2c557c7735a4f749d7eb52f6163a2783b83de
```

KEY1-gated safe OTA companion:

```text
artifacts/nesso_n1_companion_radio_smart_ota/firmware-merged.bin
c919750e9e543de4845dbe2398805692d01b7c2743fefd3e7469252c27ab8321
```

### Changes

- Fixed a boot loop triggered by Bluetooth pairing attempts on the ESP32-C6
  smart companion image.
- Defaulted Nesso smart BLE to encrypted static-PIN pairing without persistent
  bonding/MITM unless explicitly enabled by build flags.
- Reduced smart BLE/Wi-Fi frame queues from 6 to 3 frames per transport to lower
  runtime heap pressure.
- Starts BLE first, then starts the Wi-Fi/TCP AP 30 seconds after boot or 3
  seconds after a successful BLE connection.
- Defers the TCP server until after the Wi-Fi stack exists, preventing a
  startup assert in `NetworkServer::begin`.
- Regenerated merged flash images with DIO flash headers.

### Verified Locally

```text
Nesso_N1_companion_radio_smart      SUCCESS
Nesso_N1_companion_radio_smart_ota  SUCCESS
```

### Hardware Flash Proof

```text
Hash of data verified.
Nesso: WiFi AP started: NessoN1-MeshCore, TCP 5000
asserts=0
reboots=0
```

## v0.2.0

Smart connectivity, diagnostics, safer defaults, and field-tuning release.

### Included Firmware

- `Nesso_N1_companion_radio_usb`
- `Nesso_N1_companion_radio_ble`
- `Nesso_N1_companion_radio_wifi`
- `Nesso_N1_companion_radio_smart`
- `Nesso_N1_companion_radio_smart_ota`
- `Nesso_N1_repeater`
- `Nesso_N1_room_server`
- `Nesso_N1_kiss_modem`

### Ready-To-Flash Images

Recommended all-connectivity smart companion:

```text
artifacts/nesso_n1_companion_radio_smart/firmware-merged.bin
58d4d8f6722fd4259a415ebb64f6be504944344b100b3d5f3ab0cf235249fb54
```

KEY1-gated safe OTA companion:

```text
artifacts/nesso_n1_companion_radio_smart_ota/firmware-merged.bin
fbcc4beec3d8f1bed0755b9d7d4a6c67b56a40df136efcf490e25262d00a042f
```

### Highlights

- Added smart companion mode with USB serial, BLE, and Wi-Fi/TCP active in one
  firmware image.
- Added Wi-Fi AP companion target using SSID `NessoN1-MeshCore`, password
  `NessoMesh123`, and TCP port `5000`.
- Reworked ESP32 BLE and Wi-Fi frame queues as ring buffers to avoid linear
  queue shifting under traffic.
- Added `Nesso Doctor` board diagnostics through CLI, display, `get board.doctor`,
  and the safe OTA `/doctor` endpoint.
- Added board power status commands for Nesso: `get pwrmgt.support`,
  `get pwrmgt.source`, and `get pwrmgt.bootmv`.
- Added field presets: `preset range`, `preset balanced`, and `preset dense`.
- Added a safe OTA target that requires holding `KEY1` before `start ota` starts
  a local web updater.
- Disabled serial private-key import/export by default in public Nesso builds.
- Replaced legacy default repeater and room-server passwords on first boot with
  device-derived `n1-XXXXXXXX` and `room-XXXXXXXX` values.
- Expanded CI coverage to all Nesso N1 firmware roles.

### Verified Locally

```text
Nesso_N1_companion_radio_usb        SUCCESS
Nesso_N1_companion_radio_ble        SUCCESS
Nesso_N1_companion_radio_wifi       SUCCESS
Nesso_N1_companion_radio_smart      SUCCESS
Nesso_N1_companion_radio_smart_ota  SUCCESS
Nesso_N1_repeater                   SUCCESS
Nesso_N1_room_server                SUCCESS
Nesso_N1_kiss_modem                 SUCCESS
```

Native test target:

```text
native:test_utils PASSED
```

Merged image generation:

```text
Nesso_N1_companion_radio_smart      SUCCESS
Nesso_N1_companion_radio_smart_ota  SUCCESS
```

## v0.1.0

Initial Nesso N1 release.

### Included Firmware

- `Nesso_N1_companion_radio_ble`
- Ready-to-flash merged binary:
  `artifacts/nesso_n1_companion_radio_ble/firmware-merged.bin`

### Highlights

- ESP32-C6 / 16 MB flash board definition for Arduino Nesso N1.
- SX1262 LoRa radio support on the Nesso N1 pinout.
- PI4IOE5V6408 expander setup for LoRa power, reset, RF control, keys, LED, and
  board power-management lines.
- LovyanGFX display support for the Nesso display.
- Shared SPI handling between display and RadioLib.
- BLE companion mode with MeshCore client compatibility.
- USB companion, repeater, room server, and KISS modem targets build from the
  same Nesso board support.

### Verified Locally

```text
Nesso_N1_companion_radio_usb  SUCCESS
Nesso_N1_companion_radio_ble  SUCCESS
Nesso_N1_repeater             SUCCESS
Nesso_N1_room_server          SUCCESS
Nesso_N1_kiss_modem           SUCCESS
```

Native test target:

```text
native:test_utils PASSED
```

Hardware flash proof:

```text
Chip is ESP32-C6 (QFN40) (revision v0.2)
Auto-detected Flash size: 16MB
Hash of data verified.
```

Production boot proof:

```text
BLE: begin device=MeshCore-7599DA71
BLE: advertising started
```

### SHA-256

```text
41c408a327d527abb6605aa0d58843728c30d2cc099da0327a0c6d0d3a628bab  firmware-merged.bin
```
