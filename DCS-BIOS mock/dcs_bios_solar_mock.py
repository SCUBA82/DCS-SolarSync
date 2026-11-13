#!/usr/bin/env python3
"""DCS-BIOS UDP mock for testing ESP32 solar-based lamp scenes.

Sends binary DCS-BIOS export frames to 239.255.50.10:5010.

ALT_MSL_FT:       16-bit altitude in feet at address 0x0434.
TIME_START_HIGH:   16-bit high word at address 0x0446.
TIME_START_LOW:    16-bit low word at address 0x0448.
MOD_TIME_A:        elapsed mission seconds as six ASCII bytes.
TEST_VALUE:        triangular 16-bit value at address 0x7560.

No third-party Python packages are required.
"""

from __future__ import annotations

import argparse
import ipaddress
import socket
import struct
import time
from datetime import datetime

DEFAULT_GROUP = "239.255.50.10"
DEFAULT_PORT = 5010

ADDR_LAT_DEG_DIR = 1054
ADDR_LON_DEG_LAT_SEC = 1056
ADDR_LON_SEC_DIR = 1072

ADDR_ALT_MSL_FT = 0x0434

ADDR_TIME_START_HIGH = 0x0446
ADDR_TIME_START_LOW = 0x0448
ADDR_MOD_TIME = 0x0440

ADDR_TEST_VALUE = 0x7560

SYNC = b"\x55\x55\x55\x55"


def write_record(address: int, data: bytes) -> bytes:
    """Encode one DCS-BIOS export write: address, byte count, payload."""
    if not 0 <= address <= 0xFFFF:
        raise ValueError("DCS-BIOS address must fit in 16 bits")

    if not data or len(data) % 2:
        raise ValueError(
            "DCS-BIOS write payload length must be a non-zero even number"
        )

    return struct.pack("<HH", address, len(data)) + data


def encode_mod_time(elapsed_seconds: int) -> bytes:
    """Elapsed mission seconds as six zero-padded ASCII digits."""
    if elapsed_seconds < 0:
        raise ValueError("elapsed_seconds must not be negative")

    elapsed_seconds %= 1_000_000
    return f"{elapsed_seconds:06d}".encode("ascii")


def encode_altitude(altitude_ft: int) -> bytes:
    """Encode MSL altitude as unsigned 16-bit feet."""
    if not 0 <= altitude_ft <= 65535:
        raise ValueError("Altitude must be between 0 and 65535 ft")

    return struct.pack("<H", altitude_ft)


def encode_test_value(value: int) -> bytes:
    """Encode test value as unsigned 16-bit integer."""
    if not 0 <= value <= 65535:
        raise ValueError("Test value must be between 0 and 65535")

    return struct.pack("<H", value)


def calculate_test_value(
    elapsed_seconds: float,
    cycle_seconds: float = 20.0,
    minimum: int = 0,
    maximum: int = 65535,
) -> int:
    """Generate a triangular value: minimum -> maximum -> minimum."""
    if cycle_seconds <= 0:
        raise ValueError("cycle_seconds must be greater than zero")

    if not 0 <= minimum < maximum <= 65535:
        raise ValueError("Require 0 <= minimum < maximum <= 65535")

    phase = (elapsed_seconds % cycle_seconds) / cycle_seconds

    if phase < 0.5:
        value = minimum + (maximum - minimum) * (phase * 2.0)
    else:
        value = maximum - (maximum - minimum) * ((phase - 0.5) * 2.0)

    return int(value)


def encode_time_start_high_low(start_time: str) -> tuple[bytes, bytes]:
    """Encode mission start time into two 16-bit words.

    Assumption:
        combined = (HIGH << 16) | LOW
        combined = seconds since midnight
    """
    try:
        parsed = datetime.strptime(start_time, "%H:%M:%S")
    except ValueError as exc:
        raise ValueError("Start time must be HH:MM:SS") from exc

    seconds_since_midnight = (
        parsed.hour * 3600
        + parsed.minute * 60
        + parsed.second
    )

    high = (seconds_since_midnight >> 16) & 0xFFFF
    low = seconds_since_midnight & 0xFFFF

    return struct.pack("<H", high), struct.pack("<H", low)


def make_frames(
    latitude: float,
    longitude: float,
    altitude_ft: int,
    mission_start_time: str,
    mission_elapsed_seconds: int,
    test_value: int = 0,
) -> list[bytes]:
    """Build DCS-BIOS frames, one memory write per frame."""
    if not -90.0 <= latitude <= 90.0:
        raise ValueError("Latitude must be between -90 and 90 degrees")

    if not -180.0 <= longitude <= 180.0:
        raise ValueError("Longitude must be between -180 and 180 degrees")

    lat_dir = ord("S") if latitude < 0 else ord("N")
    lon_dir = ord("W") if longitude < 0 else ord("E")

    lat_abs = abs(latitude)
    lon_abs = abs(longitude)

    lat_deg_int = int(lat_abs)
    lon_deg_int = int(lon_abs)

    lat_sec_field = int((lat_abs - lat_deg_int) * 60.0)
    lon_sec_field = int((lon_abs - lon_deg_int) * 60.0)

    word_1054 = ((lat_deg_int & 0x7F) << 8) | lat_dir
    word_1056 = ((lon_deg_int & 0xFF) << 6) | (lat_sec_field & 0x3F)
    word_1072 = ((lon_sec_field & 0x3F) << 8) | lon_dir

    altitude_data = encode_altitude(altitude_ft)
    high_data, low_data = encode_time_start_high_low(mission_start_time)
    mod_time_data = encode_mod_time(mission_elapsed_seconds)
    test_value_data = encode_test_value(test_value)

    records = [
        (ADDR_LAT_DEG_DIR, struct.pack("<H", word_1054)),
        (ADDR_LON_DEG_LAT_SEC, struct.pack("<H", word_1056)),
        (ADDR_LON_SEC_DIR, struct.pack("<H", word_1072)),
        (ADDR_ALT_MSL_FT, altitude_data),
        (ADDR_TIME_START_HIGH, high_data),
        (ADDR_TIME_START_LOW, low_data),
        (ADDR_MOD_TIME, mod_time_data),
        (ADDR_TEST_VALUE, test_value_data),
    ]

    return [SYNC + write_record(address, data) for address, data in records]


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Send DCS-BIOS-compatible mock coordinates, altitude, mission "
            "start time, elapsed mission seconds and a triangular test value."
        )
    )

    parser.add_argument(
        "--group",
        default=DEFAULT_GROUP,
        help=f"Multicast group (default: {DEFAULT_GROUP})",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=DEFAULT_PORT,
        help=f"UDP port (default: {DEFAULT_PORT})",
    )
    parser.add_argument(
        "--latitude",
        type=float,
        default=47.0,
        help="Test latitude in decimal degrees (default: 47 N)",
    )
    parser.add_argument(
        "--longitude",
        type=float,
        default=15.0,
        help="Test longitude in decimal degrees (default: 15 E)",
    )
    parser.add_argument(
        "--altitude-ft",
        type=int,
        default=0,
        help="Altitude MSL in feet (default: 0)",
    )
    parser.add_argument(
        "--day-seconds",
        type=float,
        default=120.0,
        help="Real seconds for a simulated 24-hour day (default: 120)",
    )
    parser.add_argument(
        "--interval",
        type=float,
        default=0.2,
        help="Seconds between transmitted frame sets (default: 0.2)",
    )
    parser.add_argument(
        "--interface",
        default="",
        help="Optional local IPv4 address of the network adapter to send from",
    )
    parser.add_argument(
        "--start",
        default="00:00",
        help="Mission start time HH:MM or HH:MM:SS (default: 00:00)",
    )
    parser.add_argument(
        "--repeat",
        action="store_true",
        help="Repeat the simulated day indefinitely",
    )
    parser.add_argument(
        "--test-cycle-seconds",
        type=float,
        default=20.0,
        help="Real seconds for one full 0x7560 up/down cycle (default: 20)",
    )
    parser.add_argument(
        "--test-min",
        type=int,
        default=0,
        help="Minimum raw value at 0x7560 (default: 0)",
    )
    parser.add_argument(
        "--test-max",
        type=int,
        default=65535,
        help="Maximum raw value at 0x7560 (default: 65535)",
    )

    args = parser.parse_args()

    try:
        ipaddress.IPv4Address(args.group)
    except ipaddress.AddressValueError:
        parser.error("--group must be a valid IPv4 address")

    if args.day_seconds <= 0 or args.interval <= 0:
        parser.error("--day-seconds and --interval must be greater than zero")

    if args.test_cycle_seconds <= 0:
        parser.error("--test-cycle-seconds must be greater than zero")

    if not 0 <= args.test_min < args.test_max <= 65535:
        parser.error("--test-min and --test-max must satisfy 0 <= min < max <= 65535")

    if not 1 <= args.port <= 65535:
        parser.error("--port must be between 1 and 65535")

    if not 0 <= args.altitude_ft <= 65535:
        parser.error("--altitude-ft must be between 0 and 65535")

    try:
        try:
            start_dt = datetime.strptime(args.start, "%H:%M:%S")
        except ValueError:
            start_dt = datetime.strptime(args.start, "%H:%M")
    except ValueError:
        parser.error(
            "--start must use HH:MM or HH:MM:SS format, "
            "e.g. 06:30 or 06:30:00"
        )

    mission_start_time = start_dt.strftime("%H:%M:%S")
    simulated_seconds_per_second = 86400.0 / args.day_seconds

    print("DCS-BIOS Solar Mock gestartet")
    print(f"Ziel: {args.group}:{args.port}")
    print(f"Position: {args.latitude:.6f}, {args.longitude:.6f}")
    print(f"Altitude MSL: {args.altitude_ft} ft")
    print(f"Missionsstartzeit: {mission_start_time}")
    print(f"Simulierter 24h-Zeitraum: {args.day_seconds:g} reale Sekunden")
    print(f"Sendeintervall: {args.interval:g} Sekunden")
    print(
        f"0x7560 Testwert: {args.test_min} -> {args.test_max} -> "
        f"{args.test_min}; Zyklus {args.test_cycle_seconds:g} s"
    )

    print("ALT_MSL_FT:       Adresse 0x0434")
    print("TIME_START_HIGH:   Adresse 0x0446")
    print("TIME_START_LOW:    Adresse 0x0448")
    print("MOD_TIME:          Sekunden seit Missionsbeginn, 6 ASCII-Bytes")
    print("TEST_VALUE:        Adresse 0x7560, unsigned 16-bit")
    print("Es werden 8 DCS-BIOS-Datagramme pro Aktualisierung gesendet.")

    if args.interface:
        print(f"Netzwerkadapter: {args.interface}")

    print("Abbruch mit Strg+C.\n")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 1)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)

    if args.interface:
        sock.setsockopt(
            socket.IPPROTO_IP,
            socket.IP_MULTICAST_IF,
            socket.inet_aton(args.interface),
        )

    started = time.monotonic()
    last_elapsed_second = None
    last_test_value = None

    try:
        while True:
            elapsed = time.monotonic() - started

            elapsed_mission_seconds = int(elapsed * simulated_seconds_per_second)
            finished = elapsed_mission_seconds >= 86400 and not args.repeat

            if finished:
                elapsed_mission_seconds = 86399

            test_value = calculate_test_value(
                elapsed_seconds=elapsed,
                cycle_seconds=args.test_cycle_seconds,
                minimum=args.test_min,
                maximum=args.test_max,
            )

            if (
                elapsed_mission_seconds != last_elapsed_second
                or test_value != last_test_value
            ):
                frames = make_frames(
                    args.latitude,
                    args.longitude,
                    args.altitude_ft,
                    mission_start_time,
                    elapsed_mission_seconds,
                    test_value,
                )

                for frame in frames:
                    sock.sendto(frame, (args.group, args.port))
                    time.sleep(0.002)

                hours, remainder = divmod(elapsed_mission_seconds, 3600)
                minutes, seconds = divmod(remainder, 60)

                print(
                    f"Mission +{elapsed_mission_seconds:6d} s "
                    f"({hours:02d}:{minutes:02d}:{seconds:02d} elapsed) | "
                    f"Start={mission_start_time} | "
                    f"Alt={args.altitude_ft} ft | "
                    f"Lat {args.latitude:.5f} | "
                    f"Lon {args.longitude:.5f} | "
                    f"0x7560={test_value:5d} (0x{test_value:04X})"
                )

                last_elapsed_second = elapsed_mission_seconds
                last_test_value = test_value

            if finished:
                break

            time.sleep(args.interval)

    except KeyboardInterrupt:
        print("\nMock durch Benutzer beendet.")

    finally:
        sock.close()


if __name__ == "__main__":
    main()
