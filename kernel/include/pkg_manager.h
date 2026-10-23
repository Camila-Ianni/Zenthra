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
 * NOTA DE ARQUITECTURA (Auditoría v12):
 * El catálogo actual modela la estructura de aprovisionamiento de perfiles de usuario
 * mediante METADATOS DE DEMOSTRACIÓN (simulada en RAM). No representa ejecutables comerciales
 * reales (VS Code, Docker, Office, etc.). La instalación de binarios ejecutables reales
 * requerirá el formato formal de paquetes .zpkg, firmas criptográficas (SHA256/Ed25519)
 * y almacenamiento persistente en disco (Fase 6 y 7).
 *
 * Verificación de integridad estándar mediante CRC32 (IEEE 802.3):
 * Valida contra corrupción accidental en memoria/transferencia. No sustituye una firma criptográfica.
 */
uint32_t pkg_compute_crc32(const uint8_t* data, size_t length);
bool pkg_verify_integrity(const char* pkg_id, const uint8_t* data, size_t length);

/* Aprovisiona e instala automáticamente el lote de apps del perfil seleccionado */
int pkg_provision_profile(enum nova_profile profile);

#endif
