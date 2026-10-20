#include "nova_iot.h"
#include "serial.h"
#include "klib.h"
#include "io.h"

static struct arduino_state g_arduino_state;

void nova_iot_init(void) {
    k_memset(&g_arduino_state, 0, sizeof(struct arduino_state));
}

const struct arduino_state* nova_iot_get_state(void) {
    return &g_arduino_state;
}

/* Envía una trama con protocolo NOVA-Link */
static void send_nova_packet(uint8_t cmd, const uint8_t* payload, uint8_t len) {
    uint8_t cksum = cmd ^ len;
    for (uint8_t i = 0; i < len; i++) {
        cksum ^= payload[i];
    }

    serial_write_char((char)NOVA_LINK_MAGIC);
    serial_write_char((char)cmd);
    serial_write_char((char)len);
    for (uint8_t i = 0; i < len; i++) {
        serial_write_char((char)payload[i]);
    }
    serial_write_char((char)cksum);

    g_arduino_state.packets_sent++;
}

/* Recibe y valida una trama de respuesta con timeout */
static bool receive_nova_packet(uint8_t* out_resp, uint8_t* out_payload, uint8_t* out_len) {
    const uint32_t TIMEOUT_CYCLES = 1000000;
    uint32_t timeout = 0;

    // 1. Sincronizar con byte MAGIC
    while (!serial_received()) {
        if (++timeout > TIMEOUT_CYCLES) return false;
    }
    uint8_t magic = (uint8_t)serial_read();
    if (magic != NOVA_LINK_MAGIC) return false;

    // 2. Leer RESP y LEN
    timeout = 0;
    while (!serial_received()) {
        if (++timeout > TIMEOUT_CYCLES) return false;
    }
    uint8_t resp = (uint8_t)serial_read();

    timeout = 0;
    while (!serial_received()) {
        if (++timeout > TIMEOUT_CYCLES) return false;
    }
    uint8_t len = (uint8_t)serial_read();

    if (len > 32) return false;

    // 3. Leer payload
    uint8_t calc_cksum = resp ^ len;
    for (uint8_t i = 0; i < len; i++) {
        timeout = 0;
        while (!serial_received()) {
            if (++timeout > TIMEOUT_CYCLES) return false;
        }
        out_payload[i] = (uint8_t)serial_read();
        calc_cksum ^= out_payload[i];
    }

    // 4. Leer y verificar Checksum
    timeout = 0;
    while (!serial_received()) {
        if (++timeout > TIMEOUT_CYCLES) return false;
    }
    uint8_t rx_cksum = (uint8_t)serial_read();

    if (calc_cksum != rx_cksum) {
        g_arduino_state.errors_count++;
        return false;
    }

    *out_resp = resp;
    *out_len = len;
    g_arduino_state.packets_received++;
    return true;
}

bool nova_iot_ping(void) {
    send_nova_packet(CMD_PING, NULL, 0);

    uint8_t resp = 0;
    uint8_t payload[8];
    uint8_t len = 0;

    if (receive_nova_packet(&resp, payload, &len)) {
        if (resp == RESP_PONG) {
            g_arduino_state.connected = true;
            return true;
        }
    }
    g_arduino_state.connected = false;
    return false;
}

bool nova_iot_set_led(bool state) {
    uint8_t cmd = state ? CMD_LED_ON : CMD_LED_OFF;
    send_nova_packet(cmd, NULL, 0);

    uint8_t resp = 0;
    uint8_t payload[8];
    uint8_t len = 0;

    if (receive_nova_packet(&resp, payload, &len)) {
        if (resp == RESP_OK) {
            g_arduino_state.led_state = state;
            return true;
        }
    }
    return false;
}

bool nova_iot_get_temp(uint16_t* out_temp) {
    send_nova_packet(CMD_GET_TEMP, NULL, 0);

    uint8_t resp = 0;
    uint8_t payload[8];
    uint8_t len = 0;

    if (receive_nova_packet(&resp, payload, &len)) {
        if (resp == RESP_DATA_TEMP && len >= 2) {
            uint16_t temp = ((uint16_t)payload[0] << 8) | payload[1];
            g_arduino_state.last_temp_decicelsius = temp;
            if (out_temp) *out_temp = temp;
            return true;
        }
    }
    return false;
}
