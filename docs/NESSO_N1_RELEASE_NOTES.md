# Nesso N1 MeshCore Firmware Release Notes

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
