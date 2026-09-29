# bridge

`bridge` is the middle hop between the Simulink controller model and the
Furuta pendulum's embedded ESP32 (running SimpleFOC). It is built as a
**shared library** (`libbridge_uart.so`), not a standalone executable — it's
meant to be loaded into Simulink via a `C Caller` block, not run on its own.

```
Simulink (controller) --[C Caller]--> bridge_uart (this lib) --[UART]--> ESP32 (embedded)
Simulink (controller) <--[C Caller]-- bridge_uart (this lib) <--[UART]-- ESP32 (embedded)
```

Simulink computes a torque command each step; `bridge` relays it over UART
to the ESP32 and returns the ESP32's reported state estimate
`[theta1, theta2, theta1_dot, theta2_dot]` back to the model.

## API

Declared in [`include/bridge_uart.h`](include/bridge_uart.h). All functions
are `extern "C"` so they can be called from a `C Caller` block without any
C++ name-mangling issues.

| Function | Purpose |
|---|---|
| `bridge_init(port_name, baud)` | Opens the serial port and performs the HELLO handshake with the ESP32. Must succeed before any other call. |
| `bridge_hello(code)` | Re-handshake / keepalive ping on an already-open connection. |
| `bridge_step(torque_cmd, &theta1, &theta2, &theta1_dot, &theta2_dot)` | Called once per control step: sends `torque_cmd`, writes the received state estimate into the four output pointers. |
| `bridge_terminate()` | Closes the connection. |

All functions return `uint8_t`: `0` (`CONNECTED`) on success, otherwise one
of the `BRIDGEERROR` codes below.

| Code | Meaning |
|---|---|
| `Disconnected` | Called before `bridge_init` succeeded, or after `bridge_terminate` |
| `UnexpectedError` | Internal invariant violated (e.g. null port while marked connected) |
| `PortIsClosed` | The OS-level serial port failed to open or configure |
| `IsAlreadyConnected` | `bridge_init` called twice without an intervening `bridge_terminate` |
| `ConnectionFaild` | An I/O error occurred mid-transaction |
| `HelloRejected` | The ESP32 responded, but not with the expected handshake byte |

**Known issue:** the numeric values of `HelloRejected`/`ConnectionFaild` in
the `BRIDGEERROR` enum don't currently match the `#define HELLO_REJECTED` /
`#define CONNECTION_FAILED` constants above them in the header (they're
swapped). Pick one representation as the source of truth before relying on
either from the Simulink/MATLAB side.

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
   - Header: `include/bridge_uart.h` (for the function prototypes)
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

## Known issues / TODO

- **`bridge_step` is a stub.** It checks connection state but doesn't yet
  write `torque_cmd` or read back a state packet — no framing (sync byte /
  sequence number / CRC) is implemented yet.
- **`bridge_terminate` is a stub.** It doesn't close the port, delete it, or
  reset `is_connected` — calling it currently leaves the connection open.
- **`io_context` lifetime bug.** `boost::asio::io_context io;` is declared
  local to `bridge_init`, but `port` (global, outlives the function) is
  constructed against it. Once `bridge_init` returns, `io` is destroyed and
  `port` holds a dangling reference — undefined behavior on the next
  `bridge_hello`/`bridge_step` call. Needs to become a `static`/global
  alongside `port`.
- **No timeout on the handshake read** in `bridge_init`/`bridge_hello` — a
  silent ESP32 hangs the call indefinitely.
- `seq` is tracked but not yet attached to any outgoing packet — no use
  until `bridge_step` actually implements framing.
