#include <boost/asio/serial_port.hpp>
#include <bridge_serial.h>
#include <cstdint>
#include <cstring>
#include <boost/asio.hpp>

bool is_connected = false;
uint8_t seq = 0;
boost::asio::serial_port* port;
boost::asio::io_context io;

static uint8_t crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
        }
    }
    return crc;
}

uint8_t bridge_init(const char* port_name, int baud){

    try{
        if (is_connected) {
            throw BRIDGEERROR::IsAlreadyConnected;
        }
        port = new boost::asio::serial_port(io, port_name);

        port->set_option(boost::asio::serial_port_base::baud_rate(baud));
        port->set_option(boost::asio::serial_port_base::character_size(CHARACTER_SIZE));
        port->set_option(boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none));
        port->set_option(boost::asio::serial_port_base::stop_bits(boost::asio::serial_port_base::stop_bits::one));
        port->set_option(boost::asio::serial_port_base::flow_control(boost::asio::serial_port_base::flow_control::none));

        uint8_t hello = HELLO;
        boost::asio::write(*port, boost::asio::buffer(&hello, sizeof(hello)));

        uint8_t response;
        boost::asio::read(*port, boost::asio::buffer(&response, sizeof(response)));

        if (response != HELLO_RESPONSE) {
            throw BRIDGEERROR::HelloRejected;
        }

    } catch (const boost::system::system_error&) {
        if (port != nullptr) {
            delete port;
            port = nullptr;
        }
        is_connected = false;

        return static_cast<uint8_t>(BRIDGEERROR::PortIsClosed);
    } catch (BRIDGEERROR e) {
        if (port != nullptr) {
            delete port;
            port = nullptr;
        }
        is_connected = false;
        return static_cast<uint8_t>(e);
    }

    is_connected = true;
    seq = 0;
    return CONNECTED;
}

uint8_t bridge_hello(uint8_t code){
    try{
        if (!is_connected) {
            throw BRIDGEERROR::Disconnected;
        }

        // unexpected
        if (port == nullptr) {
            throw BRIDGEERROR::UnexpectedError;
        }

        boost::asio::write(*port, boost::asio::buffer(&code, sizeof(code)));
        uint8_t response;
        boost::asio::read(*port, boost::asio::buffer(&response, sizeof(response)));

        // TODO: disconnect
        if (response != HELLO_RESPONSE) {
            throw  BRIDGEERROR::HelloRejected;
        }
    } catch (const boost::system::system_error&) {
        if (port != nullptr) {
            delete port;
            port = nullptr;
        }
        is_connected = false;
        return static_cast<uint8_t>(BRIDGEERROR::ConnectionFaild);
    } catch (BRIDGEERROR e) {
        if (port != nullptr) {
            delete port;
            port = nullptr;
        }
        is_connected = false;
        return static_cast<uint8_t>(e);
    }
    seq = 0;
    return CONNECTED;
}

uint8_t bridge_step(
    float torque_cmd,
    float* theta1,
    float* theta2,
    float* theta1_dot,
    float* theta2_dot){


    try{
        if (!is_connected) {
            throw BRIDGEERROR::Disconnected;
        }

        if (port == nullptr){
            throw BRIDGEERROR::UnexpectedError;
        }

        // outgoing: [sync][seq][torque f32][crc]  (7 bytes)
        uint8_t tx[7];
        tx[0] = SYNC;
        tx[1] = seq;
        std::memcpy(&tx[2], &torque_cmd, sizeof(float));
        tx[6] = crc8(tx, 6);

        boost::asio::write(*port, boost::asio::buffer(tx, sizeof(tx)));

        // incoming: [sync][seq][theta1][theta2][theta1_dot][theta2_dot][crc]  (19 bytes)
        uint8_t rx[19];
        boost::asio::read(*port, boost::asio::buffer(rx, sizeof(rx)));

        if (rx[0] != SYNC) {
            throw BRIDGEERROR::UnexpectedError;
        }
        if (crc8(rx, 18) != rx[18]) {
            throw BRIDGEERROR::UnexpectedError;
        }

        std::memcpy(theta1,     &rx[2],  sizeof(float));
        std::memcpy(theta2,     &rx[6],  sizeof(float));
        std::memcpy(theta1_dot, &rx[10], sizeof(float));
        std::memcpy(theta2_dot, &rx[14], sizeof(float));

        seq++;

    } catch (const boost::system::system_error&) {
        if (port != nullptr) {
            delete port;
            port = nullptr;
        }
        is_connected = false;
        return static_cast<uint8_t>(BRIDGEERROR::ConnectionFaild);
    } catch(BRIDGEERROR e) {
        if (port != nullptr) {
            delete port;
            port = nullptr;
        }
        is_connected = false;
        return static_cast<uint8_t>(e);
    }

    return CONNECTED;
}

uint8_t bridge_terminate(void){
    if (!is_connected) {
        return static_cast<uint8_t>(BRIDGEERROR::Disconnected);
    }

    if (port != nullptr) {
        boost::system::error_code ec;
        port->close(ec);
        delete port;
        port = nullptr;
    }

    is_connected = false;
    seq = 0;

    return CONNECTED;
}
