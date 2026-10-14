#include <bridge_serial.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

// Simple end-to-end test for the bridge <-> ESP32 comms link.
//
// The ESP32 runs an SPP server; on the PC you must pair + bind it to a serial
// device (e.g. /dev/rfcomm0) before running. The bridge library then speaks to
// it exactly as it would to a wired UART. See README.md.

#define BAUD_DEFAULT 115200

static const char* g_port = "/dev/rfcomm0";
static float g_torque = 0.0f;
static int g_steps = 10;

static void usage(const char* argv0) {
    std::fprintf(stderr,
        "Usage: %s [--port <device>] [--steps <n>] [--torque <f>]\n"
        "  --port    serial device (default /dev/rfcomm0)\n"
        "  --steps   number of bridge_step calls (default 10)\n"
        "  --torque  torque command sent each step (default 0.0)\n",
        argv0);
}

static int parse_args(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            g_port = argv[++i];
        } else if (std::strcmp(argv[i], "--steps") == 0 && i + 1 < argc) {
            g_steps = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--torque") == 0 && i + 1 < argc) {
            g_torque = static_cast<float>(std::atof(argv[++i]));
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 1;
        } else {
            std::fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    if (parse_args(argc, argv) != 0) {
        return 2;
    }

    std::printf("Connecting to ESP32 on %s (baud %d)...\n", g_port, BAUD_DEFAULT);

    uint8_t rc = bridge_init(g_port, BAUD_DEFAULT);
    if (rc != CONNECTED) {
        std::fprintf(stderr, "bridge_init failed: code %u\n", rc);
        return 1;
    }
    std::printf("Handshake OK (HELLO accepted).\n");

    for (int i = 0; i < g_steps; ++i) {
        float theta1 = 0.f, theta2 = 0.f, theta1_dot = 0.f, theta2_dot = 0.f;

        rc = bridge_step(g_torque, &theta1, &theta2, &theta1_dot, &theta2_dot);
        if (rc != CONNECTED) {
            std::fprintf(stderr, "bridge_step failed at step %d: code %u\n", i, rc);
            bridge_terminate();
            return 1;
        }

        std::printf("step %3d | torque=%+8.3f | state=[%+8.3f, %+8.3f, %+8.3f, %+8.3f]\n",
                    i, g_torque, theta1, theta2, theta1_dot, theta2_dot);
    }

    bridge_terminate();
    std::printf("Done. Terminated cleanly.\n");
    return 0;
}
