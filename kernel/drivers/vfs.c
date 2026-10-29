#include "vfs.h"
#include "klib.h"
#include "serial.h"
#include "pmm.h"
#include "kheap.h"

static struct inode vfs_nodes[VFS_MAX_NODES];
static size_t vfs_node_count = 0;
static struct file open_files[VFS_MAX_OPEN_FILES];
static bool vfs_initialized = false;

/* --- Driver /dev/null --- */
static int dev_null_open(struct inode* inode, struct file* filp) {
    (void)inode; (void)filp;
    return 0;
}
static int32_t dev_null_read(struct file* filp, char* buf, uint32_t count) {
    (void)filp; (void)buf; (void)count;
    return 0; // EOF inmediato
}
static int32_t dev_null_write(struct file* filp, const char* buf, uint32_t count) {
    (void)filp; (void)buf;
    return (int32_t)count; // Descarta todo con éxito
}
static int dev_null_close(struct inode* inode, struct file* filp) {
    (void)inode; (void)filp;
    return 0;
}
static const struct file_operations dev_null_fops = {
    .open = dev_null_open,
    .read = dev_null_read,
    .write = dev_null_write,
    .ioctl = NULL,
    .close = dev_null_close
};

/* --- Driver /dev/zero --- */
static int dev_zero_open(struct inode* inode, struct file* filp) {
    (void)inode; (void)filp;
    return 0;
}
static int32_t dev_zero_read(struct file* filp, char* buf, uint32_t count) {
    (void)filp;
    if (buf && count > 0) {
        k_memset(buf, 0, count);
        return (int32_t)count;
    }
    return 0;
}
static int32_t dev_zero_write(struct file* filp, const char* buf, uint32_t count) {
    (void)filp; (void)buf;
    return (int32_t)count;
}
static int dev_zero_close(struct inode* inode, struct file* filp) {
    (void)inode; (void)filp;
    return 0;
}
static const struct file_operations dev_zero_fops = {
    .open = dev_zero_open,
    .read = dev_zero_read,
    .write = dev_zero_write,
    .ioctl = NULL,
    .close = dev_zero_close
};

/* --- Driver /dev/ttyS0 (UART COM1) --- */
static int dev_tty_open(struct inode* inode, struct file* filp) {
    (void)inode; (void)filp;
    return 0;
}
static int32_t dev_tty_read(struct file* filp, char* buf, uint32_t count) {
    (void)filp;
    if (!buf || count == 0) return 0;
    uint32_t bytes_read = 0;
    while (bytes_read < count && serial_received()) {
        buf[bytes_read++] = serial_read();
    }
    return (int32_t)bytes_read;
}
static int32_t dev_tty_write(struct file* filp, const char* buf, uint32_t count) {
    (void)filp;
    if (!buf || count == 0) return 0;
    for (uint32_t i = 0; i < count; i++) {
        serial_write_char(buf[i]);
    }
    return (int32_t)count;
}
static int dev_tty_close(struct inode* inode, struct file* filp) {
    (void)inode; (void)filp;
    return 0;
}
static const struct file_operations dev_tty_fops = {
    .open = dev_tty_open,
    .read = dev_tty_read,
    .write = dev_tty_write,
    .ioctl = NULL,
    .close = dev_tty_close
};

/* --- Pseudo-archivo /proc/version --- */
static const char* proc_version_text = "Zenthra OS v20.0 (i686 Protected Mode, Baremetal CRT0, VFS Architecture)\n";
static int proc_version_open(struct inode* inode, struct file* filp) {
    (void)inode;
    if (filp) filp->f_pos = 0;
    return 0;
}
static int32_t proc_version_read(struct file* filp, char* buf, uint32_t count) {
    if (!filp || !buf || count == 0) return 0;
    size_t total_len = k_strlen(proc_version_text);
    if (filp->f_pos >= total_len) return 0;

    size_t avail = total_len - filp->f_pos;
    size_t to_copy = (count < avail) ? count : avail;
    k_memcpy(buf, proc_version_text + filp->f_pos, to_copy);
    filp->f_pos += to_copy;
    return (int32_t)to_copy;
}
static int proc_version_close(struct inode* inode, struct file* filp) {
    (void)inode; (void)filp;
    return 0;
}
static const struct file_operations proc_version_fops = {
    .open = proc_version_open,
    .read = proc_version_read,
    .write = NULL,
    .ioctl = NULL,
    .close = proc_version_close
};

/* --- Pseudo-archivo /proc/meminfo --- */
static int proc_meminfo_open(struct inode* inode, struct file* filp) {
    (void)inode;
    if (filp) filp->f_pos = 0;
    return 0;
}
static int32_t proc_meminfo_read(struct file* filp, char* buf, uint32_t count) {
    if (!filp || !buf || count == 0) return 0;

    char meminfo_buf[256];
    char num_buf[32];
    size_t pos = 0;

    k_memcpy(meminfo_buf + pos, "MemTotal: ", 10); pos += 10;
    uitoa((uint32_t)pmm_get_total_memory_kib(), num_buf, 10);
    size_t len = k_strlen(num_buf);
    k_memcpy(meminfo_buf + pos, num_buf, len); pos += len;
    k_memcpy(meminfo_buf + pos, " kB\nMemFree:  ", 14); pos += 14;

    uitoa((uint32_t)pmm_get_free_memory_kib(), num_buf, 10);
    len = k_strlen(num_buf);
    k_memcpy(meminfo_buf + pos, num_buf, len); pos += len;
    k_memcpy(meminfo_buf + pos, " kB\nHeapFree: ", 14); pos += 14;

    uitoa((uint32_t)(kheap_get_free() / 1024), num_buf, 10);
    len = k_strlen(num_buf);
    k_memcpy(meminfo_buf + pos, num_buf, len); pos += len;
    k_memcpy(meminfo_buf + pos, " kB\n", 4); pos += 4;
    meminfo_buf[pos] = '\0';

    if (filp->f_pos >= pos) return 0;
    size_t avail = pos - filp->f_pos;
    size_t to_copy = (count < avail) ? count : avail;
    k_memcpy(buf, meminfo_buf + filp->f_pos, to_copy);
    filp->f_pos += to_copy;
    return (int32_t)to_copy;
}
static int proc_meminfo_close(struct inode* inode, struct file* filp) {
    (void)inode; (void)filp;
    return 0;
}
static const struct file_operations proc_meminfo_fops = {
    .open = proc_meminfo_open,
    .read = proc_meminfo_read,
    .write = NULL,
    .ioctl = NULL,
    .close = proc_meminfo_close
};

/* --- Inicialización y Operaciones Principales VFS --- */
void vfs_init(void) {
    if (vfs_initialized) return;

    vfs_node_count = 0;
    for (size_t i = 0; i < VFS_MAX_NODES; i++) {
        vfs_nodes[i].ino = 0;
        vfs_nodes[i].name[0] = '\0';
        vfs_nodes[i].f_ops = NULL;
        vfs_nodes[i].mode = 0;
        vfs_nodes[i].size = 0;
        vfs_nodes[i].private_data = NULL;
    }
    for (size_t i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        open_files[i].in_use = false;
        open_files[i].inode = NULL;
        open_files[i].f_pos = 0;
        open_files[i].flags = 0;
        open_files[i].f_ops = NULL;
    }

    // Registrar dispositivos estándar del sistema
    vfs_register_node("/dev/null", VFS_MODE_CHR, &dev_null_fops, NULL);
    vfs_register_node("/dev/zero", VFS_MODE_CHR, &dev_zero_fops, NULL);
    vfs_register_node("/dev/ttyS0", VFS_MODE_CHR, &dev_tty_fops, NULL);
    vfs_register_node("/proc/version", VFS_MODE_REG, &proc_version_fops, NULL);
    vfs_register_node("/proc/meminfo", VFS_MODE_REG, &proc_meminfo_fops, NULL);

    vfs_initialized = true;
}

int vfs_register_node(const char* path, uint32_t mode, const struct file_operations* fops, void* private_data) {
    if (!path || !fops || vfs_node_count >= VFS_MAX_NODES) {
        return -1;
    }

    // Verificar si ya existe
    for (size_t i = 0; i < vfs_node_count; i++) {
        if (k_strcmp(vfs_nodes[i].name, path) == 0) {
            return -1;
        }
    }

    struct inode* node = &vfs_nodes[vfs_node_count];
    node->ino = (uint32_t)(vfs_node_count + 1);
    size_t plen = k_strlen(path);
    if (plen >= VFS_PATH_MAX) plen = VFS_PATH_MAX - 1;
    k_memcpy(node->name, path, plen);
    node->name[plen] = '\0';
    node->mode = mode;
    node->size = 0;
    node->f_ops = fops;
    node->private_data = private_data;

    vfs_node_count++;
    return 0;
}

int vfs_open(const char* path, uint32_t flags) {
    if (!path) return -1;
    if (!vfs_initialized) vfs_init();

    // 1. Buscar el nodo correspondiente en el VFS
    struct inode* target = NULL;
    for (size_t i = 0; i < vfs_node_count; i++) {
        if (k_strcmp(vfs_nodes[i].name, path) == 0) {
            target = &vfs_nodes[i];
            break;
        }
    }
    if (!target || !target->f_ops) return -1;

    // 2. Buscar un descriptor de archivo libre
    int fd = -1;
    for (size_t i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        if (!open_files[i].in_use) {
            fd = (int)i;
            break;
        }
    }
    if (fd == -1) return -1;

    struct file* filp = &open_files[fd];
    filp->inode = target;
    filp->f_ops = target->f_ops;
    filp->flags = flags;
    filp->f_pos = 0;
    filp->private_data = target->private_data;
    filp->in_use = true;

    if (filp->f_ops->open) {
        int res = filp->f_ops->open(target, filp);
        if (res < 0) {
            filp->in_use = false;
            return res;
        }
    }

    return fd;
}

int32_t vfs_read(int fd, char* buf, uint32_t count) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || !buf) return -1;
    struct file* filp = &open_files[fd];
    if (!filp->in_use || !filp->f_ops || !filp->f_ops->read) return -1;

    return filp->f_ops->read(filp, buf, count);
}

int32_t vfs_write(int fd, const char* buf, uint32_t count) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || !buf) return -1;
    struct file* filp = &open_files[fd];
    if (!filp->in_use || !filp->f_ops || !filp->f_ops->write) return -1;

    return filp->f_ops->write(filp, buf, count);
}

int vfs_ioctl(int fd, uint32_t cmd, uint32_t arg) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES) return -1;
    struct file* filp = &open_files[fd];
    if (!filp->in_use || !filp->f_ops || !filp->f_ops->ioctl) return -1;

    return filp->f_ops->ioctl(filp, cmd, arg);
}

int vfs_close(int fd) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES) return -1;
    struct file* filp = &open_files[fd];
    if (!filp->in_use) return -1;

    int res = 0;
    if (filp->f_ops && filp->f_ops->close) {
        res = filp->f_ops->close(filp->inode, filp);
    }
    filp->in_use = false;
    filp->inode = NULL;
    filp->f_ops = NULL;
    filp->f_pos = 0;
    return res;
}

bool vfs_exists(const char* path) {
    if (!path) return false;
    if (!vfs_initialized) vfs_init();
    for (size_t i = 0; i < vfs_node_count; i++) {
        if (k_strcmp(vfs_nodes[i].name, path) == 0) {
            return true;
        }
    }
    return false;
}

size_t vfs_get_node_count(void) {
    if (!vfs_initialized) vfs_init();
    return vfs_node_count;
}
