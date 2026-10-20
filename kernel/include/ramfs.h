#ifndef RAMFS_H
#define RAMFS_H

#include "ktypes.h"

#define RAMFS_MAX_FILES 32
#define RAMFS_MAX_NAME  32
#define RAMFS_MAX_SIZE  1024

struct ramfs_file {
    char name[RAMFS_MAX_NAME];
    uint8_t data[RAMFS_MAX_SIZE];
    size_t size;
    bool used;
};

void ramfs_init(void);
int ramfs_create(const char* name);
int ramfs_write(const char* name, const uint8_t* buffer, size_t size);
int ramfs_read(const char* name, uint8_t* buffer, size_t max_size);
int ramfs_delete(const char* name);
void ramfs_list(void);

#endif
