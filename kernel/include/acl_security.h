#ifndef ACL_SECURITY_H
#define ACL_SECURITY_H

#include "ktypes.h"
#include "shell.h"

/* Operaciones de hardware y sistema sujetas a control de acceso */
enum sec_operation {
    OP_FS_READ       = 0x01,  /* Leer archivos de RAMFS */
    OP_FS_WRITE      = 0x02,  /* Crear o modificar archivos */
    OP_ARDUINO_READ  = 0x04,  /* Leer sensores Arduino (TEMP, ANALOG) */
    OP_ARDUINO_WRITE = 0x08,  /* Controlar actuadores Arduino (LED ON/OFF, Relés) */
    OP_SYSTEM_ADMIN  = 0x10,  /* Instalar paquetes, modificar reglas o memoria */
};

void acl_init(void);
bool acl_check_permission(enum nova_profile profile, enum sec_operation op);
uint8_t acl_get_profile_mask(enum nova_profile profile);
bool acl_can_transition_profile(enum nova_profile from_prof, enum nova_profile to_prof);
uint32_t acl_get_violations_count(void);
void acl_log_violation(enum nova_profile profile, enum sec_operation op, const char* context);
void acl_list_permissions(enum nova_profile profile);
enum nova_profile acl_get_active_profile(void);
void acl_set_active_profile(enum nova_profile profile);

#endif
