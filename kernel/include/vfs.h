#ifndef VFS_H
#define VFS_H

#include "ktypes.h"

#define VFS_PATH_MAX      64
#define VFS_MAX_NODES     32
#define VFS_MAX_OPEN_FILES 16

#define VFS_MODE_REG 0x01
#define VFS_MODE_CHR 0x02
#define VFS_MODE_DIR 0x04

#define VFS_O_RDONLY 0x00
#define VFS_O_WRONLY 0x01
#define VFS_O_RDWR   0x02

struct inode;
struct file;

/* Tabla de operaciones de archivo (file_operations canónicas tipo Linux VFS) */
struct file_operations {
    int (*open)(struct inode* inode, struct file* filp);
    int32_t (*read)(struct file* filp, char* buf, uint32_t count);
    int32_t (*write)(struct file* filp, const char* buf, uint32_t count);
    int (*ioctl)(struct file* filp, uint32_t cmd, uint32_t arg);
    int (*close)(struct inode* inode, struct file* filp);
};

struct inode {
    uint32_t ino;
    char name[VFS_PATH_MAX];
    uint32_t mode;
    uint32_t size;
    const struct file_operations* f_ops;
    void* private_data;
};

struct file {
    struct inode* inode;
    uint32_t f_pos;
    uint32_t flags;
    const struct file_operations* f_ops;
    void* private_data;
    bool in_use;
};

/* Inicializar el subsistema VFS y registrar dispositivos pseudo /dev y /proc */
void vfs_init(void);

/* Registrar un nodo en el espacio de nombres VFS */
int vfs_register_node(const char* path, uint32_t mode, const struct file_operations* fops, void* private_data);

/* Operaciones de usuario/llamadas al sistema */
int vfs_open(const char* path, uint32_t flags);
int32_t vfs_read(int fd, char* buf, uint32_t count);
int32_t vfs_write(int fd, const char* buf, uint32_t count);
int vfs_ioctl(int fd, uint32_t cmd, uint32_t arg);
int vfs_close(int fd);

/* Consultas */
bool vfs_exists(const char* path);
size_t vfs_get_node_count(void);

#endif
