# RIP — Furuta Pendulum Control System

`RIP` is a Furuta pendulum control project split across three layers:

```
Simulink (controller)  ⇄  bridge (shared library)  ⇄  ESP32 (SPP, drives the motor)
```

| Directory | Purpose |
|---|---|
| `bridge/` | C++ shared library (`libbridge_uart.so`) loaded into Simulink via a `C Caller` block. Relays torque commands ⇄ state estimates over a serial-style link. |
| `embedded/` | ESP-IDF firmware on the ESP32. Runs a Bluetooth Classic **SPP server** (no motor yet — returns a stub counter state). |
| `test_bridge/` | Standalone test harness that exercises the full `bridge ⇄ ESP32` comms path without Simulink. |
| `control/`, `docs/` | Reserved (currently empty). |

## Communications protocol

One-time handshake, then a framed request/reply per control step.

- **Handshake:** PC sends `HELLO` (`0x50`), ESP replies `HELLO_RESPONSE` (`0x58`).
- **Step request** (7 bytes): `[SYNC 0xAA][seq][torque f32 LE][crc8]`
- **Step reply** (19 bytes): `[SYNC 0xAA][seq][theta1][theta2][theta1_dot][theta2_dot][crc8]`
- **CRC8:** polynomial `0x07`, init `0x00`.

The transport is **Bluetooth Classic SPP**. The ESP32 runs as an SPP **server**
(no pairing/security); the PC connects and the OS exposes it as an ordinary
serial device (`/dev/rfcomm0` on Linux, a COM port on Windows). This lets the
bridge's `boost::asio::serial_port` code work unchanged over Bluetooth.

---

## Prerequisites

- **ESP-IDF** (for `embedded/`) — tested with **v5.5.5**.
  - **CMake 3.30.x** is required (system CMake 4.x is too new for ESP-IDF v5.5).
  - The ESP-IDF Python tooling + `xtensa-esp32-elf` toolchain must be installed.
- **CMake + a C++17 compiler + Boost** (for `bridge/` and `test_bridge/`).
  - On Linux: `vcpkg` (see below) or system Boost.
  - On Windows: `vcpkg` is the recommended path.

> **Note:** The ESP-IDF toolchain Python env currently runs on Python 3.14 here.
> Newer Python may work, but if ESP-IDF's installer complains, use Python 3.8–3.12.

---

## Embedded firmware (`embedded/`)

### Build & flash (Linux)

```bash
# One-time: install toolchain (see ESP-IDF docs). Then install deps + toolchain:
cd ~/esp/esp-idf && ./install.sh esp32

# Build the firmware
cd embedded
export IDF_PATH="$HOME/esp/esp-idf"
. $HOME/esp/esp-idf/export.sh
idf.py build

# Flash to the ESP32 (adjust the port)
idf.py -p /dev/ttyUSB0 flash
```

> If your system CMake is 4.x, prefix the build with a compatible CMake on
> `PATH`, e.g. `export PATH=/path/to/cmake-3.30.x/bin:$PATH`.

### Windows

Use the ESP-IDF Windows installer (`idf.py` + `xtensa-esp32-elf` toolchain),
then build/flash the same way from the ESP-IDF command prompt, substituting the
COM port (e.g. `idf.py -p COM3 flash`).

---

## Bridge library (`bridge/`)

Builds `libbridge_uart.so` (Linux) / a DLL (Windows). Not a standalone program —
it is loaded into Simulink via a `C Caller` block. See `bridge/readme.md`.

```bash
cmake --preset vcpkg
cmake --build build
```

---

## End-to-end test (`test_bridge/`)

`test_bridge` builds the bridge library (reusing `../bridge/src`) plus a small
`bridge_test` executable that performs the handshake and N step round-trips
against the ESP32.

### Linux

1. Flash the firmware (above) and confirm the ESP32 is running.
2. Pair-free connect to the SPP server:

   ```bash
   ./connect_esp.sh          # discovers, binds /dev/rfcomm0 (uses sudo)
   ```

3. Build + run:

   ```bash
   cd test_bridge
   cmake --preset vcpkg
   cmake --build build
   ./build/bridge_test --port /dev/rfcomm0 --steps 10 --torque 0.5
   ```

   Expected output: handshake OK, then `theta1` counting `0, 1, 2, ...` each
   step (the stub counter state).

### Manual Linux pairing/binding (without the helper)

```bash
# discover the ESP32
hcitool scan                   # note the MAC, e.g. 78:1C:3C:A9:D8:4E

# bind an RFCOMM serial device (channel 1)
sudo rfcomm bind 0 78:1C:3C:A9:D8:4E 1
sudo chmod 666 /dev/rfcomm0
```

If a stale DLC state causes "Device or resource busy", restart Bluetooth to
clear it:

```bash
sudo systemctl restart bluetooth
```

### Windows

1. Flash the firmware (ESP-IDF command prompt, `idf.py -p COMx flash`).
2. Pair the ESP32 from **Bluetooth settings** (it advertises as `RIPBridge`).
   It is an SPP server, so Windows may expose it as an incoming COM port
   instantly, or you may need to add it manually:
   - **Bluetooth & devices → More Bluetooth options → COM Ports → Add**,
     choose **"Outgoing"**, select the device, and note the assigned COM port
     (e.g. `COM7`).
3. Build + run (from a developer terminal with `vcpkg` on `PATH`):

   ```powershell
   cd test_bridge
   cmake --preset vcpkg
   cmake --build build
   .\build\Release\bridge_test.exe --port COM7 --steps 10 --torque 0.5
   ```

> On Windows, COM ports `COM10` and above must be passed as `\\.\COM10`.

---

## Troubleshooting

- **`bridge_init` hangs / no timeout** — the bridge currently has no read
  timeout during the handshake; a silent ESP32 blocks indefinitely. Confirm the
  ESP32 is running and the correct serial device is used. (Known issue in
  `bridge/readme.md`.)
- **"Device or resource busy" (Linux)** — stale RFCOMM DLC. Run
  `sudo systemctl restart bluetooth` and re-bind.
- **ESP not discoverable** — verify the firmware serial log shows
  `SPP server started; making device discoverable`.
- **`PortIsClosed` (code 253)** — the serial device couldn't be opened; check
  the port name and permissions (`sudo chmod 666 /dev/rfcomm0`).
