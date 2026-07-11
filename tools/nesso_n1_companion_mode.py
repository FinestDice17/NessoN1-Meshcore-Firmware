#!/usr/bin/env python3
"""Nesso N1 smart companion proof helper.

Uses the MeshCore companion framing:
host -> device: '<' uint16_le(length) payload
device -> host: '>' uint16_le(length) payload
"""

from __future__ import annotations

import argparse
import asyncio
import glob
import json
import socket
import struct
import sys
import time

import serial


CMD_NESSO_COMPANION_MODE = 0x2C
CMD_APP_START = 0x01
CMD_GET_BATT_AND_STORAGE = 0x14
CMD_DEVICE_QUERY = 0x16
RESP_CODE_OK = 0x00
RESP_CODE_ERR = 0x01
RESP_CODE_SELF_INFO = 0x05
RESP_CODE_BATT_AND_STORAGE = 0x0C
RESP_CODE_DEVICE_INFO = 0x0D
SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"


def find_port(port_hint: str | None) -> str:
    if port_hint:
        return port_hint
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        raise SystemExit("No /dev/cu.usbmodem* port found")
    return ports[0]


def frame_payload(payload: bytes) -> bytes:
    return b"<" + struct.pack("<H", len(payload)) + payload


def read_frame_serial(ser: serial.Serial, timeout: float) -> bytes:
    deadline = time.time() + timeout
    while time.time() < deadline:
        b = ser.read(1)
        if not b:
            continue
        if b != b">":
            continue
        hdr = ser.read(2)
        if len(hdr) != 2:
            continue
        length = struct.unpack("<H", hdr)[0]
        data = ser.read(length)
        if len(data) == length:
            return data
    raise TimeoutError("timed out waiting for device response frame")


def recv_exact(sock: socket.socket, length: int) -> bytes:
    data = bytearray()
    while len(data) < length:
        chunk = sock.recv(length - len(data))
        if not chunk:
            raise TimeoutError("socket closed while waiting for response frame")
        data.extend(chunk)
    return bytes(data)


def read_frame_socket(sock: socket.socket, timeout: float) -> bytes:
    sock.settimeout(timeout)
    while True:
        b = recv_exact(sock, 1)
        if b != b">":
            continue
        hdr = recv_exact(sock, 2)
        length = struct.unpack("<H", hdr)[0]
        return recv_exact(sock, length)


def build_mode_payload(action: str) -> bytes:
    payload = bytes([CMD_NESSO_COMPANION_MODE])
    if action == "ble":
        payload += b"\x01"
    elif action == "wifi":
        payload += b"\x02"
    elif action != "query":
        raise SystemExit(f"unknown mode action: {action}")
    return payload


def send_payload_serial(port: str, payload: bytes, timeout: float) -> bytes:
    with serial.Serial(port, 115200, timeout=0.05) as ser:
        ser.reset_input_buffer()
        ser.write(frame_payload(payload))
        ser.flush()
        return read_frame_serial(ser, timeout)


def send_payload_tcp(target: str, payload: bytes, timeout: float) -> bytes:
    if ":" not in target:
        raise SystemExit("--tcp must be HOST:PORT")
    host, port_text = target.rsplit(":", 1)
    with socket.create_connection((host, int(port_text)), timeout=timeout) as sock:
        sock.sendall(frame_payload(payload))
        return read_frame_socket(sock, timeout)


async def select_ble_device(selector: str | None, timeout: float):
    try:
        from bleak import BleakScanner
    except ImportError as exc:
        raise SystemExit("BLE mode requires bleak: ../.venv-platformio/bin/python -m pip install bleak") from exc

    discovered = await BleakScanner.discover(timeout=timeout, service_uuids=[SERVICE_UUID], return_adv=True)
    candidates = []
    for device, adv in discovered.values():
        name = adv.local_name or device.name or ""
        if selector:
            if selector.lower() in name.lower() or selector.lower() in str(device.address).lower():
                candidates.append((adv.rssi, device))
        elif "MeshCore" in name:
            candidates.append((adv.rssi, device))
    if not candidates and not selector:
        candidates = [(adv.rssi, device) for device, adv in discovered.values()]
    if not candidates:
        raise TimeoutError("no MeshCore BLE device found")
    candidates.sort(key=lambda item: item[0], reverse=True)
    return candidates[0][1]


async def send_payload_ble_async(selector: str | None, payload: bytes, timeout: float) -> bytes:
    try:
        from bleak import BleakClient
    except ImportError as exc:
        raise SystemExit("BLE mode requires bleak: ../.venv-platformio/bin/python -m pip install bleak") from exc

    selected = await select_ble_device(selector, timeout)
    loop = asyncio.get_running_loop()
    response_future: asyncio.Future[bytes] = loop.create_future()

    def on_notify(_sender, data: bytearray) -> None:
        if not response_future.done():
            response_future.set_result(bytes(data))

    async with BleakClient(selected, timeout=timeout) as client:
        await client.start_notify(TX_UUID, on_notify)
        await client.write_gatt_char(RX_UUID, payload, response=True)
        response = await asyncio.wait_for(response_future, timeout=timeout)
        await client.stop_notify(TX_UUID)
    return response


def send_payload_ble(selector: str | None, payload: bytes, timeout: float) -> bytes:
    return asyncio.run(send_payload_ble_async(selector, payload, timeout))


def parse_ok_or_error(response: bytes) -> bytes:
    if not response:
        raise SystemExit("empty device response")
    if response[0] == RESP_CODE_ERR:
        code = response[1] if len(response) > 1 else -1
        raise SystemExit(f"device returned PACKET_ERROR code={code}")
    return response


def parse_mode_response(response: bytes) -> int:
    response = parse_ok_or_error(response)
    if response[0] != RESP_CODE_OK:
        raise SystemExit(f"unexpected response packet 0x{response[0]:02x}")
    if len(response) >= 5:
        return struct.unpack("<I", response[1:5])[0]
    return -1


def companion_mode_serial(port: str, action: str, timeout: float) -> int:
    return parse_mode_response(send_payload_serial(port, build_mode_payload(action), timeout))


def companion_mode_tcp(target: str, action: str, timeout: float) -> int:
    return parse_mode_response(send_payload_tcp(target, build_mode_payload(action), timeout))


async def companion_mode_ble_async(selector: str | None, action: str, timeout: float) -> int:
    return parse_mode_response(await send_payload_ble_async(selector, build_mode_payload(action), timeout))


def companion_mode_ble(selector: str | None, action: str, timeout: float) -> int:
    return asyncio.run(companion_mode_ble_async(selector, action, timeout))


def clean_string(data: bytes) -> str:
    return data.split(b"\x00", 1)[0].decode("utf-8", errors="replace")


def parse_device_info(response: bytes) -> dict[str, object]:
    response = parse_ok_or_error(response)
    if len(response) < 82 or response[0] != RESP_CODE_DEVICE_INFO:
        raise SystemExit(f"unexpected device-info response: {response.hex()}")
    return {
        "firmware_code": response[1],
        "max_contacts": response[2] * 2,
        "max_channels": response[3],
        "ble_pin": struct.unpack("<I", response[4:8])[0],
        "build": clean_string(response[8:20]),
        "model": clean_string(response[20:60]),
        "version": clean_string(response[60:80]),
        "client_repeat": response[80],
        "path_hash_mode": response[81],
    }


def parse_self_info(response: bytes) -> dict[str, object]:
    response = parse_ok_or_error(response)
    if len(response) < 58 or response[0] != RESP_CODE_SELF_INFO:
        raise SystemExit(f"unexpected app-start response: {response.hex()}")
    return {
        "advert_type": response[1],
        "tx_power": response[2],
        "max_tx_power": response[3],
        "pubkey": response[4:36].hex(),
        "lat_e6": struct.unpack("<i", response[36:40])[0],
        "lon_e6": struct.unpack("<i", response[40:44])[0],
        "multi_acks": response[44],
        "advert_loc_policy": response[45],
        "telemetry_mode": response[46],
        "manual_add_contacts": response[47],
        "freq_khz": struct.unpack("<I", response[48:52])[0],
        "bw_mhz": struct.unpack("<I", response[52:56])[0] / 1000.0,
        "sf": response[56],
        "cr": response[57],
        "node_name": response[58:].decode("utf-8", errors="replace"),
    }


def parse_battery(response: bytes) -> dict[str, object]:
    response = parse_ok_or_error(response)
    if len(response) < 11 or response[0] != RESP_CODE_BATT_AND_STORAGE:
        raise SystemExit(f"unexpected battery response: {response.hex()}")
    return {
        "battery_mv": struct.unpack("<H", response[1:3])[0],
        "storage_used_kb": struct.unpack("<I", response[3:7])[0],
        "storage_total_kb": struct.unpack("<I", response[7:11])[0],
    }


def print_kv(data: dict[str, object]) -> None:
    for key, value in data.items():
        print(f"{key}={value}")


def transport_name(args: argparse.Namespace) -> str:
    if args.ble is not None:
        return "ble"
    if args.tcp:
        return "tcp"
    return "usb"


def emit(args: argparse.Namespace, data: dict[str, object]) -> None:
    if args.json:
        print(json.dumps(data, sort_keys=True))
    else:
        print_kv(data)


def mode_result(value: int) -> dict[str, object]:
    if value == 0:
        return {"mode": "ble", "mode_value": 0}
    if value == 1:
        return {"mode": "wifi", "mode_value": 1}
    return {"mode": "unknown", "mode_value": value}


def send_command(args: argparse.Namespace, payload: bytes) -> bytes:
    if args.ble is not None:
        return send_payload_ble(args.ble or None, payload, args.timeout)
    if args.tcp:
        return send_payload_tcp(args.tcp, payload, args.timeout)
    return send_payload_serial(find_port(args.port), payload, args.timeout)


def build_device_query_payload(version: int = 3) -> bytes:
    return bytes([CMD_DEVICE_QUERY, version & 0xFF])


def build_app_start_payload(name: str = "nesso-proof") -> bytes:
    return bytes([CMD_APP_START]) + b"\x00" * 7 + name.encode("utf-8")


def smoke_result(args: argparse.Namespace) -> dict[str, object]:
    return {
        "transport": transport_name(args),
        "device_query": parse_device_info(send_command(args, build_device_query_payload(3))),
        "app_start": parse_self_info(send_command(args, build_app_start_payload())),
        "battery": parse_battery(send_command(args, bytes([CMD_GET_BATT_AND_STORAGE]))),
        "mode": mode_result(parse_mode_response(send_command(args, build_mode_payload("query")))),
    }


def print_smoke_human(result: dict[str, object]) -> None:
    for section in ("device_query", "app_start", "battery"):
        print(f"[{section.replace('_', '-')}]")
        print_kv(result[section])  # type: ignore[arg-type]
    print("[mode]")
    mode = result["mode"]
    if isinstance(mode, dict):
        print(f"mode={mode['mode']}")


def run_self_test() -> dict[str, object]:
    device = bytearray([RESP_CODE_DEVICE_INFO, 13, 175, 40])
    device += struct.pack("<I", 0)
    device += b"6 Jun 2026\x00\x00"
    device += b"Arduino Nesso N1".ljust(40, b"\x00")
    device += b"v1.16.0".ljust(20, b"\x00")
    device += bytes([0, 1])

    self_info = bytearray([RESP_CODE_SELF_INFO, 1, 22, 22])
    self_info += bytes.fromhex("7599da71673c70f4b088e3f8300a22f46b8b7c16ec10b9eca7dfcdfcdcf509fc")
    self_info += struct.pack("<i", 0)
    self_info += struct.pack("<i", 0)
    self_info += bytes([0, 0, 0, 0])
    self_info += struct.pack("<I", 910525)
    self_info += struct.pack("<I", 62500)
    self_info += bytes([7, 5])
    self_info += b"MrWonderful"

    battery = bytes([RESP_CODE_BATT_AND_STORAGE]) + struct.pack("<HII", 4141, 3, 3169)
    assert frame_payload(b"\x2c") == b"<\x01\x00\x2c"
    assert build_mode_payload("ble") == b"\x2c\x01"
    assert build_mode_payload("wifi") == b"\x2c\x02"
    assert parse_mode_response(bytes([RESP_CODE_OK]) + struct.pack("<I", 1)) == 1

    parsed_device = parse_device_info(bytes(device))
    parsed_self = parse_self_info(bytes(self_info))
    parsed_battery = parse_battery(battery)
    assert parsed_device["model"] == "Arduino Nesso N1"
    assert parsed_device["max_contacts"] == 350
    assert parsed_self["node_name"] == "MrWonderful"
    assert parsed_self["freq_khz"] == 910525
    assert parsed_battery["battery_mv"] == 4141
    return {
        "status": "ok",
        "checks": [
            "frame_payload",
            "mode_payloads",
            "mode_response",
            "device_info_parse",
            "self_info_parse",
            "battery_parse",
        ],
    }


def hard_reset(port: str) -> None:
    with serial.Serial(port, 115200, timeout=0.02) as ser:
        ser.dtr = False
        ser.rts = True
        time.sleep(0.2)
        ser.rts = False


def boot_capture(port_hint: str | None, seconds: float) -> int:
    port = find_port(port_hint)
    hard_reset(port)
    deadline = time.time() + seconds
    total = 0

    while time.time() < deadline:
        ports = sorted(glob.glob("/dev/cu.usbmodem*"))
        if not ports:
            time.sleep(0.05)
            continue
        for port in ports:
            try:
                ser = serial.Serial(port, 115200, timeout=0.02)
            except serial.SerialException:
                continue
            with ser:
                local_deadline = min(time.time() + 0.2, deadline)
                while time.time() < local_deadline:
                    data = ser.read(4096)
                    if data:
                        total += len(data)
                        sys.stdout.write(data.decode("utf-8", errors="replace"))
                        sys.stdout.flush()
        time.sleep(0.02)
    return total


def main() -> int:
    parser = argparse.ArgumentParser(description="Nesso N1 smart companion proof helper")
    parser.add_argument("--port", help="serial port, default: first /dev/cu.usbmodem*")
    parser.add_argument("--tcp", help="TCP companion target as HOST:PORT for mode commands")
    parser.add_argument("--ble", nargs="?", const="", help="BLE device name/address substring for mode commands")
    parser.add_argument("--timeout", type=float, default=5.0, help="response timeout in seconds")
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    sub = parser.add_subparsers(dest="cmd", required=True)

    mode = sub.add_parser("mode", help="query or set companion mode")
    mode.add_argument("action", choices=["query", "ble", "wifi"])

    device_query = sub.add_parser("device-query", help="send CMD_DEVICE_QUERY and print parsed device info")
    device_query.add_argument("--version", type=int, default=3, help="protocol version byte")

    app_start = sub.add_parser("app-start", help="send CMD_APP_START and print parsed self info")
    app_start.add_argument("--name", default="nesso-proof", help="host app name")

    sub.add_parser("battery", help="send CMD_GET_BATT_AND_STORAGE and print parsed battery/storage info")

    sub.add_parser("smoke", help="run device-query, app-start, battery, and mode query")

    sub.add_parser("self-test", help="run hardware-free parser and frame tests")

    capture = sub.add_parser("boot-capture", help="hard reset and capture USB boot logs")
    capture.add_argument("--seconds", type=float, default=18.0)

    args = parser.parse_args()

    if args.cmd == "boot-capture":
        total = boot_capture(args.port, args.seconds)
        if args.json:
            print(json.dumps({"capture_bytes": total}, sort_keys=True))
        else:
            print(f"\nCAPTURE_BYTES {total}")
        return 0

    if args.cmd == "self-test":
        result = run_self_test()
        if args.json:
            print(json.dumps(result, sort_keys=True))
        else:
            print_kv(result)
        return 0

    if args.cmd == "device-query":
        result = parse_device_info(send_command(args, build_device_query_payload(args.version)))
        result["transport"] = transport_name(args)
        emit(args, result)
        return 0

    if args.cmd == "app-start":
        result = parse_self_info(send_command(args, build_app_start_payload(args.name)))
        result["transport"] = transport_name(args)
        emit(args, result)
        return 0

    if args.cmd == "battery":
        result = parse_battery(send_command(args, bytes([CMD_GET_BATT_AND_STORAGE])))
        result["transport"] = transport_name(args)
        emit(args, result)
        return 0

    if args.cmd == "smoke":
        result = smoke_result(args)
        if args.json:
            print(json.dumps(result, sort_keys=True))
        else:
            print_smoke_human(result)
        return 0

    if args.ble is not None:
        value = companion_mode_ble(args.ble or None, args.action, args.timeout)
    elif args.tcp:
        value = companion_mode_tcp(args.tcp, args.action, args.timeout)
    else:
        port = find_port(args.port)
        value = companion_mode_serial(port, args.action, args.timeout)
    if value == 0:
        result = {"transport": transport_name(args), "mode": "ble", "mode_value": 0}
    elif value == 1:
        result = {"transport": transport_name(args), "mode": "wifi", "mode_value": 1}
    else:
        result = {"transport": transport_name(args), "mode": "unknown", "mode_value": value}
    if args.json:
        print(json.dumps(result, sort_keys=True))
    elif value in (0, 1):
        print(f"mode={result['mode']}")
    else:
        print(f"mode_value={value}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
