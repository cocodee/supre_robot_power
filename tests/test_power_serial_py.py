#!/usr/bin/env python3
"""Detailed tests for the pybind11 power_serial module.

The script is intentionally dependency-free so it can run inside the arm64
Docker build image with only the compiled extension on PYTHONPATH.
"""

from __future__ import annotations

import argparse
import binascii
import os
import sys
from typing import Callable

import power_serial


Test = Callable[[], None]


def assert_raises(fn: Callable[[], object], expected_message: str | None = None) -> None:
    try:
        fn()
    except RuntimeError as exc:
        if expected_message is not None:
            assert expected_message in str(exc), (expected_message, str(exc))
        return
    raise AssertionError("expected RuntimeError")


def as_hex(data: bytes) -> str:
    return binascii.hexlify(data).decode("ascii").upper()


def test_checksum_and_length_vectors() -> None:
    assert power_serial.checksum_ascii(b"1203400456ABCEFE") == 0xFC71
    assert power_serial.length_word(0) == 0x0000
    assert power_serial.length_word(1) == 0xE002
    assert power_serial.length_word(32) == 0xC040


def test_frame_roundtrip_no_info() -> None:
    frame = power_serial.make_command(0x01, 0x4F)
    raw = power_serial.encode_frame(frame)
    assert raw == b"~2001464F0000FD99\r"

    decoded = power_serial.decode_frame(raw)
    assert decoded.ver == 0x20
    assert decoded.adr == 0x01
    assert decoded.cid1 == 0x46
    assert decoded.cid2 == 0x4F
    assert decoded.length == 0x0000
    assert decoded.checksum == 0xFD99
    assert decoded.info == []


def test_frame_roundtrip_with_info() -> None:
    frame = power_serial.make_command(0x01, 0x91, [0x03])
    raw = power_serial.encode_frame(frame)
    assert raw.startswith(b"~20014691E00203")
    assert raw.endswith(b"\r")

    decoded = power_serial.decode_frame(raw)
    assert decoded.cid2 == 0x91
    assert decoded.length == 0xE002
    assert decoded.info == [0x03]
    assert power_serial.encode_frame(decoded) == raw


def test_charge_relay_special_frames_from_pdf() -> None:
    open_frame = power_serial.make_command(0x01, 0x99, [0x0A])
    close_frame = power_serial.make_command(0x01, 0x99, [0x0B])

    open_raw = power_serial.encode_frame(open_frame)
    close_raw = power_serial.encode_frame(close_frame)

    assert as_hex(open_raw) == "7E3230303134363939453030323041464433350D"
    assert as_hex(close_raw) == "7E3230303134363939453030323042464433350D"

    assert power_serial.decode_frame(open_raw).info == [0x0A]
    assert power_serial.decode_frame(close_raw).info == [0x0B]


def test_decode_rejects_bad_inputs() -> None:
    assert_raises(lambda: power_serial.decode_frame(b""), "bad frame")
    assert_raises(lambda: power_serial.decode_frame(b"!2001464F0000FD99\r"), "bad frame")
    assert_raises(lambda: power_serial.decode_frame(b"~2001464F0000FD99\n"), "bad frame")
    assert_raises(lambda: power_serial.decode_frame(b"~2001464F0000FFFF\r"), "bad checksum")
    assert_raises(lambda: power_serial.decode_frame(b"~2001464FE000FD99\r"), "bad length")
    assert_raises(lambda: power_serial.decode_frame(b"~2G01464F0000FD99\r"), "bad hex")


def test_device_info_parse() -> None:
    payload = (
        b"SmartPack "
        + bytes([0x01, 0x02])
        + b"SLEC Power          "
    )
    assert len(payload) == 32
    info = power_serial.parse_device_info(list(payload))
    assert info.device_name == "SmartPack "
    assert info.software_version == 0x0102
    assert info.manufacturer == "SLEC Power          "

    assert_raises(lambda: power_serial.parse_device_info([0x00] * 31), "bad length")


def test_pack_response_parse_single_pack() -> None:
    payload = [
        0x01,
        0x02,
        0x0C,
        0xE4,
        0x0C,
        0xE5,
        0x01,
        0x00,
        0xFA,
        0x00,
        0x64,
        0x18,
        0x38,
        0x00,
        0x32,
        0x85,
        0x0F,
        0x09,
        0x01,
        0x02,
    ]
    parsed = power_serial.parse_pack_response(payload)
    assert parsed.command_or_pack_count == 0x01
    assert parsed.pack_count == 1
    assert parsed.warn_state == []

    pack = parsed.packs[0]
    assert pack.pack_index == 1
    assert pack.cell_count == 2
    assert pack.cell_mv == [3300, 3301]
    assert pack.temperature_count == 1
    assert pack.temperatures_raw == [250]
    assert pack.charge_current_raw == 100
    assert pack.total_voltage_raw == 6200
    assert pack.discharge_current_raw == 50

    status = pack.status
    assert status.protection == 0x85
    assert status.fet == 0x0F
    assert status.misc == 0x09
    assert status.cell_fault_1_8 == 0x01
    assert status.cell_fault_9_16 == 0x02
    assert status.total_voltage_low is True
    assert status.charge_over_temp is False
    assert status.discharge_over_temp is False
    assert status.discharge_over_current is False
    assert status.charge_over_current is True
    assert status.cell_low_voltage is False
    assert status.over_voltage is True
    assert status.using_pack is True
    assert status.dfet_on is True
    assert status.cfet_on is True
    assert status.prefet_on is True
    assert status.fully_charged is True
    assert status.buzzer_on is True


def test_pack_response_rejects_truncated_payload() -> None:
    assert_raises(lambda: power_serial.parse_pack_response([]), "invalid argument")
    assert_raises(lambda: power_serial.parse_pack_response([0x01, 0x02, 0x0C]), "bad length")
    assert_raises(lambda: power_serial.parse_pack_response([0x01, 0x11]), "bad length")


def test_client_open_failure_is_reported() -> None:
    missing = "/tmp/power-serial-test-missing-device"
    if os.path.exists(missing):
        os.unlink(missing)
    assert_raises(lambda: power_serial.Client(missing), "i/o error")


def test_optional_live_serial() -> None:
    device = os.environ.get("POWER_SERIAL_TEST_PORT")
    if not device:
        return

    client = power_serial.Client(
        device,
        baudrate=int(os.environ.get("POWER_SERIAL_TEST_BAUD", "9600")),
        timeout_ms=int(os.environ.get("POWER_SERIAL_TEST_TIMEOUT_MS", "1000")),
        adr=int(os.environ.get("POWER_SERIAL_TEST_ADR", "1"), 0),
    )
    count = client.get_pack_count()
    assert 0 <= count <= 8
    version = client.get_protocol_version()
    assert version.cid1 == 0x46


TESTS: list[tuple[str, Test]] = [
    ("checksum_and_length_vectors", test_checksum_and_length_vectors),
    ("frame_roundtrip_no_info", test_frame_roundtrip_no_info),
    ("frame_roundtrip_with_info", test_frame_roundtrip_with_info),
    ("charge_relay_special_frames_from_pdf", test_charge_relay_special_frames_from_pdf),
    ("decode_rejects_bad_inputs", test_decode_rejects_bad_inputs),
    ("device_info_parse", test_device_info_parse),
    ("pack_response_parse_single_pack", test_pack_response_parse_single_pack),
    ("pack_response_rejects_truncated_payload", test_pack_response_rejects_truncated_payload),
    ("client_open_failure_is_reported", test_client_open_failure_is_reported),
    ("optional_live_serial", test_optional_live_serial),
]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--list", action="store_true", help="list tests and exit")
    parser.add_argument("tests", nargs="*", help="specific test names to run")
    args = parser.parse_args()

    if args.list:
        for name, _ in TESTS:
            print(name)
        return 0

    selected = set(args.tests)
    unknown = selected - {name for name, _ in TESTS}
    if unknown:
        print(f"unknown tests: {', '.join(sorted(unknown))}", file=sys.stderr)
        return 2

    failures = 0
    for name, fn in TESTS:
        if selected and name not in selected:
            continue
        try:
            fn()
        except Exception as exc:  # noqa: BLE001 - plain script, report all failures.
            failures += 1
            print(f"FAIL {name}: {exc}", file=sys.stderr)
        else:
            print(f"PASS {name}")

    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
