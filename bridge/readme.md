# bridge

`bridge` is the middle hop between the Simulink controller model and the
Furuta pendulum's embedded ESP32 (running SimpleFOC). It is built as a
**shared library** (`libbridge_uart.so`), not a standalone executable — it's
meant to be loaded into Simulink via a `C Caller` block, not run on its own.

```
Simulink (controller) --[C Caller]--> bridge_serial (this lib) --[serial link]--> ESP32 (embedded)
Simulink (controller) <--[C Caller]-- bridge_serial (this lib) <--[serial link]-- ESP32 (embedded)
```

Simulink computes a torque command each step; `bridge` relays it to the
ESP32 and returns the ESP32's reported state estimate
`[theta1, theta2, theta1_dot, theta2_dot]` back to the model. "Serial link"
here is deliberately generic — the same `boost::asio::serial_port`-based code
works unchanged over a wired UART (`/dev/ttyUSB0` / `COM3`) or a paired
Classic Bluetooth SPP connection (`/dev/rfcomm0` / `COM7`), since both
present themselves to the OS as an ordinary serial device. Only the
`port_name` string passed to `bridge_init` changes.

## API

Declared in [`include/bridge_serial.h`](include/bridge_serial.h). All
functions are `extern "C"` so they can be called from a `C Caller` block
without any C++ name-mangling issues.

| Function | Purpose |
|---|---|
| `bridge_init(port_name, baud)` | Opens the serial port and performs the HELLO handshake with the ESP32. Must succeed before any other call. |
| `bridge_hello(code)` | Re-handshake / keepalive ping on an already-open connection. |
| `bridge_step(torque_cmd, &theta1, &theta2, &theta1_dot, &theta2_dot)` | Called once per control step: sends `torque_cmd`, writes the received state estimate into the four output pointers. |
| `bridge_terminate()` | Closes the port and resets connection state. |

All functions return `uint8_t`: `0` (`CONNECTED`) on success, otherwise one
of the `BRIDGEERROR` codes below.

| Code | Meaning |
|---|---|
| `Disconnected` | Called before `bridge_init` succeeded, or after `bridge_terminate` |
| `UnexpectedError` | Internal invariant violated, or a received packet failed sync/CRC validation |
| `PortIsClosed` | The OS-level serial port failed to open or configure |
| `IsAlreadyConnected` | `bridge_init` called twice without an intervening `bridge_terminate` |
| `ConnectionFaild` | An I/O error occurred mid-transaction |
| `HelloRejected` | The ESP32 responded, but not with the expected handshake byte |

**Known issue:** the numeric values of `HelloRejected`/`ConnectionFaild` in
the `BRIDGEERROR` enum don't currently match the `#define HELLO_REJECTED` /
`#define CONNECTION_FAILED` constants above them in the header (they're
swapped). Pick one representation as the source of truth before relying on
either from the Simulink/MATLAB side.

## Protocol

Two phases: a one-time handshake in `bridge_init`/`bridge_hello`, then a
framed request/reply exchange on every `bridge_step` call.

### Handshake

`bridge_init` and `bridge_hello` send a single byte and expect a single byte
back:

```
tx: [HELLO]            (0x50)
rx: [HELLO_RESPONSE]   (0x58)
```

Any other response byte is treated as `HelloRejected` and tears down the
connection (see Known issues below — this also applies to unrelated single
corrupted bytes, not just genuine handshake failures).

### Step packets

Every `bridge_step` call is one request, one reply, both framed the same
way: a sync byte, a sequence number, a fixed-size payload, then a CRC8 over
everything before it.

**Request (`bridge` → ESP32, 7 bytes):**

| Byte(s) | Field |
|---|---|
| `0` | `SYNC` (`0xAA`) |
| `1` | `seq` |
| `2..5` | `torque_cmd` as raw `float` (4 bytes, native endianness) |
| `6` | CRC8 over bytes `0..5` |

**Reply (ESP32 → `bridge`, 19 bytes):**

| Byte(s) | Field |
|---|---|
| `0` | `SYNC` (`0xAA`) |
| `1` | `seq` |
| `2..5` | `theta1` (`float`) |
| `6..9` | `theta2` (`float`) |
| `10..13` | `theta1_dot` (`float`) |
| `14..17` | `theta2_dot` (`float`) |
| `18` | CRC8 over bytes `0..17` |

Floats are serialized with a raw `memcpy` of their 4 bytes — no explicit
endianness conversion, since both ends are little-endian (x86/x64 on the PC
side, Xtensa on the ESP32 side). `seq` increments by one on every successful
`bridge_step` round-trip; it isn't currently cross-checked against the
reply's own `seq` byte (see Known issues).

**CRC8** uses polynomial `0x07`, init `0x00` (implemented as `crc8()` in
[`src/bridge_serial.cpp`](src/bridge_serial.cpp)) — the same algorithm needs
implementing on the ESP32 side for the reply to validate.

**On failure** (wrong sync byte, or CRC mismatch on the reply): `bridge_step`
throws `UnexpectedError`, which — like every other error path in this file —
closes the port and marks the connection disconnected. A single corrupted
byte in one packet currently costs you the whole session, not just that one
step; `bridge_init` has to be called again to recover. Whether that's the
right behavior long-term is worth revisiting once you've seen how often it
actually triggers on real hardware.

## Build

```bash
cmake --preset vcpkg
cmake --build build
```

Produces `build/libbridge_uart.so`. This is a `SHARED` CMake target — see
[`CMakeLists.txt`](CMakeLists.txt) — built entirely with your own toolchain,
independent of MATLAB's compiler.

## Adding to Simulink

Use the `C Caller` block in **precompiled library** mode — this links
against the already-built `.so` instead of asking Simulink to compile the
source itself, so MATLAB's toolchain never has to deal with Boost.

1. Drop a `C Caller` block into the model.
2. In the block dialog, switch to "precompiled library" and point it at:
   - Header: `include/bridge_serial.h` (for the function prototypes)
   - Library: `build/libbridge_uart.so`
3. Assign the four functions to the block's lifecycle hooks:
   - **Initialize** → `bridge_init`
   - **Output/Update** → `bridge_step`
   - **Terminate** → `bridge_terminate`
   (`bridge_hello` isn't part of the per-step loop — call it manually if you
   need to re-handshake without a full re-init.)
4. Wire the block's input port to the controller's torque output, and its
   four output ports into whatever consumes `theta1, theta2, theta1_dot,
   theta2_dot` (estimator/LQR subsystem).

**Runtime gotcha:** MATLAB has to be able to find `libbridge_uart.so` when
the model *runs*, not just when it's configured. Either put `build/` on
`LD_LIBRARY_PATH` before launching MATLAB, set an rpath on the library, or
copy the `.so` next to the model.

**On Windows**, `port_name` is just a COM port string (e.g. `"COM7"`) —
same code, same API, `boost::asio::serial_port` wraps the Win32 COM port API
underneath. Ports `COM10` and above need the `\\.\COM10` form instead of the
bare name.

## Known issues / TODO

- **No timeout on the handshake read** in `bridge_init`/`bridge_hello` — a
  silent ESP32 hangs the call indefinitely. `bridge_step`'s framed reads have
  the same exposure.
- **Any single bad packet tears down the whole connection**, not just that
  one step — see Protocol section above. Worth deciding if a corrupted
  `bridge_step` reply should instead just return an error for that step and
  let the caller retry, keeping the connection alive.
- **`seq` isn't cross-checked** — it increments locally but the reply's own
  `seq` byte isn't compared against what was expected, so a reordered or
  stale reply wouldn't currently be detected by sequence number alone (CRC
  would still likely catch outright corruption).
- **ESP32 firmware doesn't implement this protocol yet.** Nothing in
  `Furuta-Pendulum/src/Firmware` currently speaks HELLO/HELLO_RESPONSE or the
  framed step packets — this library's counterpart on the embedded side
  still needs writing.
