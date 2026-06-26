#include <stdint.h>

#include "test_framework.h"

// Mock hardware state tracking
typedef struct {
    uint8_t call_count;
    uint8_t last_value;
} mock_port_t;

static mock_port_t mock_ports[65536];     // Track all possible ports
static uint8_t inb_return_values[65536];  // Return values for inb calls

// Mock implementation of outb
void outb(uint16_t port, uint8_t val) {
    mock_ports[port].call_count++;
    mock_ports[port].last_value = val;

    // Debug output for important ports
    if (port == 0x20 || port == 0x21 || port == 0x40 || port == 0x43 ||
        port >= 0x3F8 && port <= 0x3FF) {
        test_serial_write_string("[MOCK] outb(0x");
        char hex_str[5];
        uint_to_hex_string(port, hex_str);
        test_serial_write_string(hex_str);
        test_serial_write_string(", 0x");
        uint_to_hex_string(val, hex_str);
        test_serial_write_string(hex_str);
        test_serial_write_string(")\r\n");
    }
}

// Mock implementation of inb
uint8_t inb(uint16_t port) {
    uint8_t value = inb_return_values[port];

    // Debug output for important ports
    if (port == 0x20 || port == 0x21 || port >= 0x3F8 && port <= 0x3FF) {
        test_serial_write_string("[MOCK] inb(0x");
        char hex_str[5];
        uint_to_hex_string(port, hex_str);
        test_serial_write_string(hex_str);
        test_serial_write_string(") = 0x");
        uint_to_hex_string(value, hex_str);
        test_serial_write_string(hex_str);
        test_serial_write_string("\r\n");
    }

    return value;
}

// Mock hardware control functions
void mock_init(void) {
    mock_reset();
}

void mock_reset(void) {
    for (int i = 0; i < 65536; i++) {
        mock_ports[i].call_count = 0;
        mock_ports[i].last_value = 0;
        inb_return_values[i] = 0;
    }

    // Set default return values for common ports
    inb_return_values[0x3FD] = 0x20;  // Serial port ready for transmission
}

uint8_t mock_get_outb_call_count(uint16_t port) {
    return mock_ports[port].call_count;
}

uint8_t mock_get_last_outb_value(uint16_t port) {
    return mock_ports[port].last_value;
}

void mock_set_inb_return_value(uint16_t port, uint8_t value) {
    inb_return_values[port] = value;
}

// Function declarations
void uint_to_hex_string(uint32_t value, char* buffer);
void uint32_to_string(uint32_t value, char* buffer);

// Utility function to convert uint to hex string
void uint_to_hex_string(uint32_t value, char* buffer) {
    const char hex_chars[] = "0123456789ABCDEF";
    buffer[0] = hex_chars[(value >> 12) & 0xF];
    buffer[1] = hex_chars[(value >> 8) & 0xF];
    buffer[2] = hex_chars[(value >> 4) & 0xF];
    buffer[3] = hex_chars[value & 0xF];
    buffer[4] = '\0';
}