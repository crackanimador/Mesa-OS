#ifndef _VFS_H
#define _VFS_H

#include "types.h"

#define MAX_FDS         16
#define MAX_FILENAME    256

#define VFS_FILE        0x01
#define VFS_DIRECTORY   0x02
#define VFS_CHARDEVICE  0x03
#define VFS_BLOCKDEVICE 0x04
#define VFS_PIPE        0x05
#define VFS_SYMLINK     0x06

#define O_RDONLY        0x0000
#define O_WRONLY        0x0001
#define O_RDWR          0x0002
#define O_APPEND        0x0008
#define O_CREAT         0x0200
#define O_TRUNC         0x0400

#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

struct vfs_node;
struct file_descriptor;

typedef struct vfs_ops {
    int64_t (*read)(struct vfs_node *node, void *buf, size_t count, uint64_t offset);
    int64_t (*write)(struct vfs_node *node, const void *buf, size_t count, uint64_t offset);
    int     (*open)(struct vfs_node *node, uint32_t flags);
    int     (*close)(struct vfs_node *node);
    int     (*ioctl)(struct vfs_node *node, uint64_t request, void *arg);
} vfs_ops_t;

typedef struct vfs_node {
    char name[MAX_FILENAME];
    uint32_t type;
    uint32_t flags;
    uint64_t size;
    uint32_t inode;
    vfs_ops_t *ops;
    void *private_data;
    struct vfs_node *parent;
    struct vfs_node *children;
    struct vfs_node *next;
} vfs_node_t;

typedef struct file_descriptor {
    vfs_node_t *node;
    uint64_t offset;
    uint32_t flags;
    uint32_t refcount;
} file_descriptor_t;

void vfs_init(void);
int64_t vfs_read(vfs_node_t *node, void *buf, size_t count, uint64_t offset);
int64_t vfs_write(vfs_node_t *node, const void *buf, size_t count, uint64_t offset);
int     vfs_open(vfs_node_t *node, uint32_t flags);
int     vfs_close(vfs_node_t *node);

vfs_node_t *vfs_get_console_in(void);
vfs_node_t *vfs_get_console_out(void);
vfs_node_t *vfs_get_console_err(void);

#endif