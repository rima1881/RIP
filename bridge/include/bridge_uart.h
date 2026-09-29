#ifndef BRIDGE_UART_H
#define BRIDGE_UART_H

#include <cstdint>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

uint8_t bridge_init(const char* port_name, int baud);
uint8_t bridge_step(float torque_cmd,
                       float* theta1, float* theta2,
                       float* theta1_dot, float* theta2_dot);

uint8_t brige_hello(int* code);
uint8_t bridge_terminate(void);

#ifdef __cplusplus
}
#endif

#endif
