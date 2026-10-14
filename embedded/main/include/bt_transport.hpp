#pragma once

#include <cstddef>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Transport-agnostic byte interface. All Bluetooth specifics live in
// bt_transport.cpp; nothing else in the firmware includes any esp_bt/
// esp_spp/esp_gap headers.

#ifdef __cplusplus
extern "C" {
#endif

// Bring up the Bluetooth stack and start listening as an SPP server.
void bt_init(void);

// Register the comms task handle. The SPP callback will notify this task
// (direct-to-task notification) whenever data arrives or the connection
// changes, so the comms task can block efficiently and never poll.
void bt_register_comms_task(TaskHandle_t task);

// True once a Classic SPP channel to a client is open.
bool bt_connected(void);

// Non-blocking read: returns up to n bytes currently buffered, or -1 if
// disconnected. Returns 0 when connected but no data is available yet.
int bt_read(uint8_t* buf, size_t n);

// Blocking write of n bytes. Returns number of bytes written or -1 on error.
int bt_write(const uint8_t* buf, size_t n);

#ifdef __cplusplus
}
#endif
