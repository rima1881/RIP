#include <boost/asio/serial_port.hpp>
#include <bridge_uart.h>
#include <cstdint>
#include <boost/asio.hpp>
#include <array>

struct Message{
    uint8_t sync = 0xAA;
    uint8_t seq;
    uint8_t* data;
    uint8_t data_len;
    uint8_t crc;
};

bool is_connected = false;
uint8_t seq = 0;
boost::asio::serial_port* port;

uint8_t send(uint8_t* data, uint8_t len) {

    return 0;
}

uint8_t bridge_init(const char* port_name, uint32_t baud){


    boost::asio::io_context io;
    port = new boost::asio::serial_port(io, "/dev/ttyUSB0"); // TODO: change it to input

    std::array<uint8_t, 5> payload;




    return 0;
}

uint8_t bridge_step(
    float torque_cmd,
    float* theta1,
    float* theta2,
    float* theta1_dot,
    float* theta2_dot){

    if (!is_connected) {
        return 1;
    }

    return 0;
}

uint8_t brige_hello(int* code){
    if (!is_connected) {
        return 1;
    }

    return 0;
}

uint8_t bridge_terminate(void){
    if(!is_connected) {
        return 1;
    }

    return 0;
}
