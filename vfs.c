#include "../include/vfs.h"
#include "../include/console.h"
#include "../include/keyboard.h"

static int64_t console_read(vfs_node_t *node, void *buf, size_t count, uint64_t offset) {
    (void)node;
    (void)offset;
    char *dst = (char *)buf;
    size_t read = 0;

    while (read < count) {
        char c;
        while (!keyboard_buffer_pop(&c)) {
            __asm__ volatile ("hlt");
        }

        /* Manejar teclas de flecha */
        if ((uint8_t)c == KEYCODE_UP) {
            extern void console_scroll_up(void);
            console_scroll_up();
            continue;
        } else if ((uint8_t)c == KEYCODE_DOWN) {
            extern void console_scroll_down(void);
            console_scroll_down();
            continue;
        } else if ((uint8_t)c == KEYCODE_LEFT || (uint8_t)c == KEYCODE_RIGHT) {
            /* Por ahora ignoramos izquierda/derecha */
            continue;
        }

        dst[read++] = c;
        if (c == '\n') break;
    }

    return (int64_t)read;
}

static int64_t console_write(vfs_node_t *node, const void *buf, size_t count, uint64_t offset) {
    (void)node;
    (void)offset;
    const char *src = (const char *)buf;
    for (size_t i = 0; i < count; i++) {
        console_putchar(src[i]);
    }
    return (int64_t)count;
}

static int console_open(vfs_node_t *node, uint32_t flags) {
    (void)node;
    (void)flags;
    return 0;
}

static int console_close(vfs_node_t *node) {
    (void)node;
    return 0;
}

static vfs_ops_t console_in_ops = {
    .read  = console_read,
    .write = NULL,
    .open  = console_open,
    .close = console_close,
    .ioctl = NULL
};

static vfs_ops_t console_out_ops = {
    .read  = NULL,
    .write = console_write,
    .open  = console_open,
    .close = console_close,
    .ioctl = NULL
};

static vfs_node_t console_in_node = {
    .name = "stdin",
    .type = VFS_CHARDEVICE,
    .flags = O_RDONLY,
    .size = 0,
    .inode = 0,
    .ops = &console_in_ops,
    .private_data = NULL,
    .parent = NULL,
    .children = NULL,
    .next = NULL
};

static vfs_node_t console_out_node = {
    .name = "stdout",
    .type = VFS_CHARDEVICE,
    .flags = O_WRONLY,
    .size = 0,
    .inode = 1,
    .ops = &console_out_ops,
    .private_data = NULL,
    .parent = NULL,
    .children = NULL,
    .next = NULL
};

static vfs_node_t console_err_node = {
    .name = "stderr",
    .type = VFS_CHARDEVICE,
    .flags = O_WRONLY,
    .size = 0,
    .inode = 2,
    .ops = &console_out_ops,
    .private_data = NULL,
    .parent = NULL,
    .children = NULL,
    .next = NULL
};

void vfs_init(void) {
    console_puts("[VFS] Initializing VFS (stdin/stdout/stderr)...\n");
    console_puts("       Status: ");
    console_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    console_puts("OK\n");
    console_set_color(VGA_WHITE, VGA_BLACK);
}

int64_t vfs_read(vfs_node_t *node, void *buf, size_t count, uint64_t offset) {
    if (!node || !node->ops || !node->ops->read) return -1;
    return node->ops->read(node, buf, count, offset);
}

int64_t vfs_write(vfs_node_t *node, const void *buf, size_t count, uint64_t offset) {
    if (!node || !node->ops || !node->ops->write) return -1;
    return node->ops->write(node, buf, count, offset);
}

int vfs_open(vfs_node_t *node, uint32_t flags) {
    if (!node || !node->ops || !node->ops->open) return -1;
    return node->ops->open(node, flags);
}

int vfs_close(vfs_node_t *node) {
    if (!node || !node->ops || !node->ops->close) return -1;
    return node->ops->close(node);
}

vfs_node_t *vfs_get_console_in(void)  { return &console_in_node;  }
vfs_node_t *vfs_get_console_out(void) { return &console_out_node; }
vfs_node_t *vfs_get_console_err(void) { return &console_err_node; }