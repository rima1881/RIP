#pragma once

#include <cstdint>

// Protocol state machine for the bridge link. Mirrors the framing in
// bridge/src/bridge_serial.cpp:
//
//   Handshake:  rx HELLO(0x50)  ->  tx HELLO_RESPONSE(0x58)
//   Step req:   [0xAA][seq][torque f32 LE][crc8]                       (7B)
//   Step reply: [0xAA][seq][theta1][theta2][theta1_dot][theta2_dot][crc8] (19B)
//   CRC8:       poly 0x07, init 0x00

enum class comms_state_t {
    IDLE,
    WAIT_CONN,
    WAIT_HELLO,
    CONNECTED,
};

struct __attribute__((packed)) step_request_t {
    uint8_t sync;
    uint8_t seq;
    float   torque;
    uint8_t crc;
};

// Initialize transport + spawn the comms task (event-driven, no polling).
void comms_init(void);

// Return the current state of the comms state machine (informational only;
// the actual work happens in the comms task).
comms_state_t comms_run(void);
