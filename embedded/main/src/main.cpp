#include "comms.hpp"

extern "C" void app_main(void) {
    // Bring up Bluetooth + spawn the event-driven comms task. app_main has
    // nothing left to do; the comms task waits on BT notifications.
    comms_init();
}
