#include "comms.hpp"

#include "bt_transport.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstring>

namespace {

constexpr uint8_t SYNC = 0xAA;
constexpr uint8_t HELLO = 0x50;
constexpr uint8_t HELLO_RESPONSE = 0x58;

constexpr size_t STEP_REQ_LEN = 7;
constexpr size_t STEP_REPLY_LEN = 19;

comms_state_t s_state = comms_state_t::IDLE;

// Stub motor state: monotonically increasing counter so the bridge can see
// the four channels flow without a real motor.
uint32_t s_stub_counter = 0;

uint8_t crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
        }
    }
    return crc;
}

// Copy any available bytes into dst. Returns bytes copied.
size_t drain(uint8_t* dst, size_t want) {
    size_t got = 0;
    while (got < want) {
        int n = bt_read(dst + got, want - got);
        if (n <= 0) {
            break;
        }
        got += static_cast<size_t>(n);
    }
    return got;
}

// Read exactly `len` bytes, blocking across notifications until complete.
bool read_exact(uint8_t* dst, size_t len) {
    size_t got = 0;
    while (got < len) {
        if (!bt_connected()) {
            return false;
        }
        got += drain(dst + got, len - got);
        if (got < len) {
            // Not enough data; block until the BT callback wakes us again.
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }
    }
    return true;
}

void write_state_reply(uint8_t seq) {
    float theta1 = static_cast<float>(s_stub_counter);
    float theta2 = theta1 * 2.0f;
    float theta1_dot = theta1 / 4.0f;
    float theta2_dot = theta1_dot * 2.0f;
    ++s_stub_counter;

    uint8_t pkt[STEP_REPLY_LEN];
    pkt[0] = SYNC;
    pkt[1] = seq;
    std::memcpy(&pkt[2], &theta1, sizeof(float));
    std::memcpy(&pkt[6], &theta2, sizeof(float));
    std::memcpy(&pkt[10], &theta1_dot, sizeof(float));
    std::memcpy(&pkt[14], &theta2_dot, sizeof(float));
    pkt[STEP_REPLY_LEN - 1] = crc8(pkt, STEP_REPLY_LEN - 1);

    bt_write(pkt, sizeof(pkt));
}

// The comms state machine runs entirely in this task. It is awakened by the
// SPP callback via a direct-to-task notification whenever bytes arrive or the
// connection changes, so it never polls.
void comms_task(void*) {
    while (true) {
        // Wait for a connection.
        while (!bt_connected()) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }
        s_state = comms_state_t::WAIT_HELLO;

        // Handshake.
        uint8_t b = 0;
        if (!read_exact(&b, 1)) {
            continue; // disconnected
        }
        if (b != HELLO) {
            continue; // wrong byte; wait for a fresh connection (simplest)
        }
        uint8_t resp = HELLO_RESPONSE;
        bt_write(&resp, 1);
        s_state = comms_state_t::CONNECTED;

        // Step request/reply loop.
        while (true) {
            uint8_t raw[STEP_REQ_LEN];
            if (!read_exact(raw, STEP_REQ_LEN)) {
                break; // disconnected
            }
            if (raw[0] != SYNC || crc8(raw, STEP_REQ_LEN - 1) != raw[STEP_REQ_LEN - 1]) {
                // Corrupt frame: drop it, keep the connection alive.
                continue;
            }
            write_state_reply(raw[1]);
        }
    }
}

} // namespace

void comms_init(void) {
    bt_init();
    s_stub_counter = 0;
    s_state = comms_state_t::IDLE;

    // Spawn a high-priority comms task and register it with the transport so
    // the SPP callback can notify it on incoming data.
    TaskHandle_t task = nullptr;
    xTaskCreate(comms_task, "comms", 4096, nullptr, 5, &task);
    bt_register_comms_task(task);
}

comms_state_t comms_run(void) {
    return s_state;
}
