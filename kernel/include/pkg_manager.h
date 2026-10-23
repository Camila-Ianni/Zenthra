#ifndef PKG_MANAGER_H
#define PKG_MANAGER_H

#include "ktypes.h"
#include "shell.h"

#define MAX_PACKAGES 40

struct package_info {
    char id[16];
    char name[32];
    char version[12];
    char description[64];
    enum nova_profile target_profile;
    size_t size_bytes;
    uint32_t expected_crc32; /* Checksum de integridad del paquete */
    bool installed;
};

void pkg_init(void);
void pkg_list_catalog(void);
void pkg_list_installed(void);
void pkg_recommend(enum nova_profile profile);
int pkg_install(const char* pkg_id);
int pkg_uninstall(const char* pkg_id);

/* 
 * Verificación de integridad estándar mediante CRC32 (IEEE 802.3).
 * NOTA: Valida contra corrupción accidental en memoria/transferencia.
 * Para autenticidad criptográfica de autor, la Fase 6 implementará firmas digitales SHA256/Ed25519.
 */
uint32_t pkg_compute_crc32(const uint8_t* data, size_t length);
bool pkg_verify_integrity(const char* pkg_id, const uint8_t* data, size_t length);

/* Aprovisiona e instala automáticamente el lote de apps del perfil seleccionado */
int pkg_provision_profile(enum nova_profile profile);

#endif
