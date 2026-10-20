/*
 * Firmware oficial receptor NOVA-Link para Arduino (UNO / Nano / Mega)
 * Tesis: NOVA OS - Octubre 2026
 *
 * Implementa el protocolo NOVA-Link sobre Serial USB a 38400 baudios (8N1).
 */

#define NOVA_LINK_MAGIC 0xAA

// Definición de pines por defecto
const int PIN_LED = 13;      // LED integrado o actuador
const int PIN_SENSOR_TEMP = A0; // Sensor analógico (LM35 / NTC / potenciómetro de prueba)

// Códigos de comando
#define CMD_PING        0x01
#define CMD_GET_STATUS  0x02
#define CMD_LED_ON      0x10
#define CMD_LED_OFF     0x11
#define CMD_GET_TEMP    0x20
#define CMD_GET_ANALOG  0x21

// Códigos de respuesta
#define RESP_OK         0x80
#define RESP_PONG       0x81
#define RESP_DATA_TEMP  0x90
#define RESP_DATA_ANALOG 0x91
#define RESP_ERR_BAD_CMD 0xE0
#define RESP_ERR_CKSUM  0xE1
#define RESP_ERR_HW     0xE2

void send_packet(uint8_t resp_id, const uint8_t* payload, uint8_t len) {
    uint8_t cksum = resp_id ^ len;
    for (uint8_t i = 0; i < len; i++) {
        cksum ^= payload[i];
    }
    
    Serial.write(NOVA_LINK_MAGIC);
    Serial.write(resp_id);
    Serial.write(len);
    for (uint8_t i = 0; i < len; i++) {
        Serial.write(payload[i]);
    }
    Serial.write(cksum);
}

void setup() {
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);
    
    // Iniciar puerto serie a la misma velocidad configurada en el kernel
    Serial.begin(38400);
}

void loop() {
    if (Serial.available() > 0) {
        uint8_t magic = Serial.read();
        if (magic != NOVA_LINK_MAGIC) {
            return; // Esperar byte de sincronización
        }
        
        // Esperar CMD y LEN
        while (Serial.available() < 2);
        uint8_t cmd = Serial.read();
        uint8_t len = Serial.read();
        
        uint8_t payload[32];
        for (uint8_t i = 0; i < len; i++) {
            while (Serial.available() < 1);
            payload[i] = Serial.read();
        }
        
        while (Serial.available() < 1);
        uint8_t received_cksum = Serial.read();
        
        // Verificar Checksum (XOR)
        uint8_t calc_cksum = cmd ^ len;
        for (uint8_t i = 0; i < len; i++) {
            calc_cksum ^= payload[i];
        }
        
        if (calc_cksum != received_cksum) {
            send_packet(RESP_ERR_CKSUM, NULL, 0);
            return;
        }
        
        // Procesar comando válido
        switch (cmd) {
            case CMD_PING: {
                send_packet(RESP_PONG, NULL, 0);
                break;
            }
            case CMD_LED_ON: {
                digitalWrite(PIN_LED, HIGH);
                send_packet(RESP_OK, NULL, 0);
                break;
            }
            case CMD_LED_OFF: {
                digitalWrite(PIN_LED, LOW);
                send_packet(RESP_OK, NULL, 0);
                break;
            }
            case CMD_GET_TEMP: {
                // Lectura analógica de ejemplo (LM35 o simulado)
                int raw = analogRead(PIN_SENSOR_TEMP);
                // Conversión aproximada: 10mV/C en ADC de 5V (1024 pasos)
                // (raw * 5000 / 1024) / 10 = raw * 500 / 1024
                uint16_t temp_c_x10 = (uint16_t)((raw * 5000L) / 1024L); // en decimas de grado
                uint8_t resp_data[2];
                resp_data[0] = (temp_c_x10 >> 8) & 0xFF;
                resp_data[1] = temp_c_x10 & 0xFF;
                send_packet(RESP_DATA_TEMP, resp_data, 2);
                break;
            }
            case CMD_GET_STATUS: {
                uint8_t status_data[2];
                status_data[0] = digitalRead(PIN_LED); // Estado del LED
                status_data[1] = 0x01;                // Sensor activo
                send_packet(RESP_OK, status_data, 2);
                break;
            }
            default: {
                send_packet(RESP_ERR_BAD_CMD, NULL, 0);
                break;
            }
        }
    }
}
