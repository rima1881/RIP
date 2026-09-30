#ifndef BRIDGE_UART_H
#define BRIDGE_UART_H

#define BAUD_RATE 115200
#define CHARACTER_SIZE 8
#define SYNC 0xAA
#define HELLO 80
#define HELLO_RESPONSE 88

// ERRORS
#define DISCONNECTED 255
#define UNEXPECTED_ERROR 254
#define PORT_IS_CLOSED 253
#define IS_ALREADY_CONNECTED 252
#define HELLO_REJECTED 251
#define CONNECTION_FAILED 250


enum class BRIDGEERROR {
    Disconnected = 255,
    UnexpectedError = 254,
    PortIsClosed = 253,
    IsAlreadyConnected = 252,
    ConnectionFaild = 251,
    HelloRejected = 250
};

// SUCCESS
#define CONNECTED 0

#include <cstdint>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

uint8_t bridge_init(const char* port_name, int baud);
uint8_t bridge_step(float torque_cmd,
                       float* theta1, float* theta2,
                       float* theta1_dot, float* theta2_dot);

uint8_t bridge_hello(uint8_t code);
uint8_t bridge_terminate(void);

#ifdef __cplusplus
}
#endif

#endif
