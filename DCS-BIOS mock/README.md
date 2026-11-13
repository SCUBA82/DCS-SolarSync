usage: dcs_bios_solar_mock.py [-h] [--group GROUP] [--port PORT] [--latitude LATITUDE] [--longitude LONGITUDE]
                              [--altitude-ft ALTITUDE_FT] [--day-seconds DAY_SECONDS] [--interval INTERVAL]
                              [--interface INTERFACE] [--start START] [--repeat]
                              [--test-cycle-seconds TEST_CYCLE_SECONDS] [--test-min TEST_MIN] [--test-max TEST_MAX]

Send DCS-BIOS-compatible mock coordinates, altitude, mission start time, elapsed mission seconds and a triangular test
value.

options:
  -h, --help            show this help message and exit
  --group GROUP         Multicast group (default: 239.255.50.10)
  --port PORT           UDP port (default: 5010)
  --latitude LATITUDE   Test latitude in decimal degrees (default: 47 N)
  --longitude LONGITUDE
                        Test longitude in decimal degrees (default: 15 E)
  --altitude-ft ALTITUDE_FT
                        Altitude MSL in feet (default: 0)
  --day-seconds DAY_SECONDS
                        Real seconds for a simulated 24-hour day (default: 120)
  --interval INTERVAL   Seconds between transmitted frame sets (default: 0.2)
  --interface INTERFACE
                        Optional local IPv4 address of the network adapter to send from
  --start START         Mission start time HH:MM or HH:MM:SS (default: 00:00)
  --repeat              Repeat the simulated day indefinitely
  --test-cycle-seconds TEST_CYCLE_SECONDS
                        Real seconds for one full 0x7560 up/down cycle (default: 20)
  --test-min TEST_MIN   Minimum raw value at 0x7560 (default: 0)
  --test-max TEST_MAX   Maximum raw value at 0x7560 (default: 65535)