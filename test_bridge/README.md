# test_bridge

End-to-end test of the `bridge` library against the ESP32 firmware.

The ESP32 (in `../embedded`) runs a Bluetooth Classic **SPP server**. On the
PC, Bluetooth SPP is exposed as an ordinary serial device, so the bridge talks
to it exactly as if it were a wired UART — no bridge code changes are needed.

## Prerequisites

1. Build + flash the ESP32 firmware (see `../embedded/`).
2. Pair your PC to the ESP32 (device name `RIPBridge`) and bind an RFCOMM
   serial channel to it:

   ```bash
   # pair (no PIN)
   bluetoothctl --agent NoInputNoOutput --default-agent
   # inside bluetoothctl:
   #   scan on            (find the ESP32's MAC, e.g. 78:1c:3c:a9:d8:4e)
   #   pair <MAC>
   #   trust <MAC>
   #   connect <MAC>
   #   exit

   # bind an RFCOMM serial device
   sudo rfcomm bind 0 <MAC> 1
   # -> creates /dev/rfcomm0
   ```

3. Confirm the serial device exists: `ls -l /dev/rfcomm0`.

## Build

```bash
cmake --preset vcpkg
cmake --build build
```

Produces `build/bridge_test` and `build/libbridge_uart.so`.

## Run

```bash
./build/bridge_test --port /dev/rfcomm0 --steps 10 --torque 0.5
```

Expected output shows the handshake succeeding, then each step's returned
state estimate advancing (the ESP32 currently returns a stub counter, so the
four floats increment each step).

## Notes

- The bridge library source is shared from `../bridge/src/bridge_serial.cpp`;
  this test does not duplicate it.
- If the ESP32 is silent or the port is wrong, `bridge_init` reports a
  `PortIsClosed`/`HelloRejected` code (the bridge currently has no read
  timeout during the handshake — see `../bridge/readme.md`).
