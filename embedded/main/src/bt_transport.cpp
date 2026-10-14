#include "bt_transport.hpp"

#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_bt_api.h>
#include <esp_log.h>
#include <esp_spp_api.h>
#include <nvs_flash.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

const char* TAG = "bt_transport";

// RX ring buffer. The SPP data callback (BT task) pushes bytes in; the comms
// task pulls them out. A single producer, single consumer keeps this race-free
// without a lock: the callback only writes s_rx_head / reads s_rx_tail, and the
// comms task only reads s_rx_head / writes s_rx_tail.
constexpr size_t RX_BUF_SIZE = 1024;
uint8_t s_rx_buf[RX_BUF_SIZE];
volatile size_t s_rx_head = 0;   // producer (BT task)
volatile size_t s_rx_tail = 0;   // consumer (comms task)

// Handle of the comms task. Used by the SPP callback to wake it (a task
// notification) whenever data arrives or the connection changes.
TaskHandle_t s_comms_task = nullptr;

// Word-sized shared state; atomic on the 32-bit bus, no lock required.
volatile bool s_connected = false;
volatile uint32_t s_spp_handle = 0;

bool connected() {
    return s_connected;
}

uint32_t handle() {
    return s_spp_handle;
}

void set_connected(uint32_t h, bool c) {
    s_connected = c;
    if (h != 0) {
        s_spp_handle = h;
    }
}

// Wake the comms task when new data / connection state arrives. Called from
// the BT task (a normal task context, not an ISR), so use the task-notification
// API accordingly.
void notify_comms() {
    if (s_comms_task != nullptr) {
        xTaskNotifyGive(s_comms_task);
    }
}

void spp_event_handler(esp_spp_cb_event_t event, esp_spp_cb_param_t* param) {
    switch (event) {
        case ESP_SPP_INIT_EVT:
            if (param->init.status == ESP_SPP_SUCCESS) {
                esp_spp_start_srv(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_SLAVE, 0,
                                  const_cast<char*>("RIPBridge"));
            } else {
                ESP_LOGE(TAG, "SPP init failed: %d", param->init.status);
            }
            break;

        case ESP_SPP_START_EVT:
            // Server is up; now it is safe to advertise + become discoverable.
            if (param->start.status == ESP_SPP_SUCCESS) {
                ESP_LOGI(TAG, "SPP server started; making device discoverable");
                esp_bt_gap_set_device_name("RIPBridge");
                esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
            } else {
                ESP_LOGE(TAG, "SPP server start failed: %d", param->start.status);
            }
            break;

        case ESP_SPP_SRV_OPEN_EVT: {
            // Server-side SPP connection opened (we are the SPP server/slave).
            uint32_t h = param->srv_open.handle;
            set_connected(h, true);
            ESP_LOGI(TAG, "SPP client connected (handle %lu)",
                     static_cast<unsigned long>(h));
            notify_comms();
            break;
        }

        case ESP_SPP_OPEN_EVT:
            // Client-side open; unused in server mode.
            break;

        case ESP_SPP_CLOSE_EVT:
            set_connected(0, false);
            s_rx_head = 0;
            s_rx_tail = 0;
            ESP_LOGI(TAG, "SPP client disconnected");
            notify_comms();
            break;

        case ESP_SPP_DATA_IND_EVT: {
            const uint8_t* data = param->data_ind.data;
            size_t len = param->data_ind.len;
            for (size_t i = 0; i < len; ++i) {
                size_t next = (s_rx_head + 1) % RX_BUF_SIZE;
                if (next == s_rx_tail) {
                    break; // buffer full; drop remainder of this frame
                }
                s_rx_buf[s_rx_head] = data[i];
                s_rx_head = next;
            }
            notify_comms();
            break;
        }

        default:
            break;
    }
}

void gap_event_handler(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) {
    (void)event;
    (void)param;
}

} // namespace

extern "C" void bt_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Release BLE memory; we only need Classic BT.
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);

    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&cfg);
    esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);

    while (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    // Leave SSP enabled; with IOCap = NoInputNoOutput and a SEC_NONE SPP
    // server, connections complete without pairing or user interaction.
    esp_bluedroid_init_with_cfg(&bluedroid_cfg);
    esp_bluedroid_enable();

    while (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    esp_bt_gap_register_callback(gap_event_handler);
    esp_spp_register_callback(spp_event_handler);

    esp_spp_cfg_t spp_cfg = {
        .mode = ESP_SPP_MODE_CB,
        .enable_l2cap_ertm = true,
        .tx_buffer_size = 0,
    };
    esp_spp_enhanced_init(&spp_cfg);

    // Configure legacy pairing: NoInputNoOutput IOCap lets a SEC_NONE SPP
    // server accept links without any user interaction/pairing.
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));

    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
    esp_bt_pin_code_t pin_code = {0};
    esp_bt_gap_set_pin(pin_type, 0, pin_code);

    s_rx_head = 0;
    s_rx_tail = 0;
    s_connected = false;
    s_spp_handle = 0;

    ESP_LOGI(TAG, "Classic BT (SPP server) initialized");
}

extern "C" void bt_register_comms_task(TaskHandle_t task) {
    s_comms_task = task;
}

extern "C" bool bt_connected(void) {
    return connected();
}

// Non-blocking: return however many bytes are currently buffered (up to n).
extern "C" int bt_read(uint8_t* buf, size_t n) {
    if (!connected()) {
        return -1;
    }

    size_t read = 0;
    while (read < n && s_rx_tail != s_rx_head) {
        buf[read++] = s_rx_buf[s_rx_tail];
        s_rx_tail = (s_rx_tail + 1) % RX_BUF_SIZE;
    }
    return static_cast<int>(read);
}

extern "C" int bt_write(const uint8_t* buf, size_t n) {
    if (!connected()) {
        return -1;
    }

    esp_err_t err = esp_spp_write(handle(), n, const_cast<uint8_t*>(buf));
    if (err != ESP_OK) {
        return -1;
    }
    return static_cast<int>(n);
}
