/*
 * NOVA-Link Protocol Header
 * Protocolo de tramas binarias/texto para comunicación segura entre NOVA OS y Arduino.
 */

#ifndef NOVA_LINK_H
#define NOVA_LINK_H

#include "ktypes.h"

#define NOVA_LINK_MAGIC 0xAA  /* Byte sincronizador de inicio de trama */

/* Códigos de comando (NOVA OS -> Arduino) */
enum nova_cmd {
    CMD_PING        = 0x01,  /* Comprobar conexión activa */
    CMD_GET_STATUS  = 0x02,  /* Obtener estado general de placa y sensores */
    CMD_LED_ON      = 0x10,  /* Activar LED / actuador digital */
    CMD_LED_OFF     = 0x11,  /* Desactivar LED / actuador digital */
    CMD_GET_TEMP    = 0x20,  /* Solicitar lectura de sensor de temperatura */
    CMD_GET_ANALOG  = 0x21,  /* Leer pin analógico específico */
};

/* Códigos de respuesta (Arduino -> NOVA OS) */
enum nova_resp {
    RESP_OK         = 0x80,  /* Operación completada con éxito */
    RESP_PONG       = 0x81,  /* Respuesta a PING */
    RESP_DATA_TEMP  = 0x90,  /* Trama con lectura de temperatura (ej. centésimas de grado) */
    RESP_DATA_ANALOG= 0x91,  /* Trama con valor analógico leído (0-1023) */
    RESP_ERR_BAD_CMD= 0xE0,  /* Comando desconocido o no soportado */
    RESP_ERR_CKSUM  = 0xE1,  /* Fallo de verificación de integridad (Checksum) */
    RESP_ERR_HW     = 0xE2,  /* Fallo físico en sensor o hardware */
};

/* Formato de la trama NOVA-Link:
 * [0]   MAGIC (0xAA)
 * [1]   CMD / RESP ID
 * [2]   PAYLOAD LENGTH (N)
 * [3..N+2] DATA PAYLOAD
 * [N+3] CHECKSUM (XOR simple de CMD + LEN + DATA[0..N-1])
 */

#endif
