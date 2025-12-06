/**
 * @file mesafs.c
 * @brief MesaFS - Sistema de archivos simple para MesaOS
 */

#include "../include/mesafs.h"
#include "../include/disk.h"
#include "../include/partition.h"
#include "../include/console.h"
#include "../include/kheap.h"

/* Estructura global del sistema de archivos montado */
static mesafs_t *mounted_fs = NULL;

/* Leer un bloque del disco */
static int read_block(mesafs_t *fs, uint32_t block_num, void *buffer) {
    if (!fs || block_num >= fs->sb.total_blocks) {
        return -1;
    }
    
    /* Calcular LBA: partition_lba + 1 (superbloque) + block_num */
    uint32_t lba = fs->partition_lba + 1 + block_num;
    
    /* Leer 8 sectores (4096 bytes = 8 * 512) */
    uint8_t temp_buffer[4096];
    for (int i = 0; i < 8; i++) {
        if (disk_read_sector(lba + i, temp_buffer + (i * 512)) != 0) {
            return -1;
        }
    }
    
    /* Copiar a buffer */
    uint8_t *dst = (uint8_t *)buffer;
    for (int i = 0; i < 4096; i++) {
        dst[i] = temp_buffer[i];
    }
    
    return 0;
}

/* Escribir un bloque al disco */
static int write_block(mesafs_t *fs, uint32_t block_num, const void *buffer) {
    if (!fs || block_num >= fs->sb.total_blocks) {
        return -1;
    }
    
    /* Calcular LBA: partition_lba + 1 (superbloque) + block_num */
    uint32_t lba = fs->partition_lba + 1 + block_num;
    
    /* Escribir 8 sectores (4096 bytes = 8 * 512) */
    const uint8_t *src = (const uint8_t *)buffer;
    uint8_t temp_buffer[512];
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 512; j++) {
            temp_buffer[j] = src[(i * 512) + j];
        }
        if (disk_write_sector(lba + i, temp_buffer) != 0) {
            return -1;
        }
    }
    
    return 0;
}

/* Leer inodo desde disco */
int mesafs_read_inode(mesafs_t *fs, uint32_t inode_num, mesafs_inode_t *inode) {
    if (!fs || !inode || inode_num >= fs->sb.total_inodes) {
        return -1;
    }
    
    /* Calcular bloque de inodos: bloque 1 + (inode_num * 128) / 4096 */
    uint32_t inode_block = 1 + (inode_num * 128) / 4096;
    uint32_t inode_offset = (inode_num * 128) % 4096;
    
    uint8_t block_buffer[4096];
    if (read_block(fs, inode_block, block_buffer) != 0) {
        return -1;
    }
    
    /* Copiar inodo */
    mesafs_inode_t *inodes = (mesafs_inode_t *)block_buffer;
    uint32_t inode_idx = inode_offset / 128;
    *inode = inodes[inode_idx];
    
    return 0;
}

/* Función interna para leer inodo (usada por otras funciones internas) */
static int read_inode(mesafs_t *fs, uint32_t inode_num, mesafs_inode_t *inode) {
    return mesafs_read_inode(fs, inode_num, inode);
}

/* Escribir inodo al disco */
static int write_inode(mesafs_t *fs, uint32_t inode_num, const mesafs_inode_t *inode) {
    if (!fs || !inode || inode_num >= fs->sb.total_inodes) {
        return -1;
    }
    
    /* Calcular bloque de inodos */
    uint32_t inode_block = 1 + (inode_num * 128) / 4096;
    uint32_t inode_offset = (inode_num * 128) % 4096;
    
    uint8_t block_buffer[4096];
    if (read_block(fs, inode_block, block_buffer) != 0) {
        return -1;
    }
    
    /* Actualizar inodo */
    mesafs_inode_t *inodes = (mesafs_inode_t *)block_buffer;
    uint32_t inode_idx = inode_offset / 128;
    inodes[inode_idx] = *inode;
    
    /* Escribir bloque de vuelta */
    return write_block(fs, inode_block, block_buffer);
}

int mesafs_format(uint32_t partition_lba, uint32_t num_sectors) {
    console_puts("[MesaFS] Formatting partition...\n");
    
    /* Calcular número de bloques (cada bloque = 8 sectores) */
    uint32_t total_blocks = num_sectors / 8;
    if (total_blocks < 10) {
        console_puts("[MesaFS] Partition too small\n");
        return -1;
    }
    
    /* Inicializar superbloque */
    mesafs_superblock_t sb;
    sb.magic = MESAFS_MAGIC;
    sb.version = MESAFS_VERSION;
    sb.block_size = MESAFS_BLOCK_SIZE;
    sb.total_blocks = total_blocks;
    sb.free_blocks = total_blocks - 10; /* Reservar 10 bloques para metadatos */
    sb.total_inodes = 256;
    sb.free_inodes = 255; /* Reservar inodo 0 */
    sb.root_inode = 1;
    
    /* Escribir superbloque (sector 0 de la partición) */
    uint8_t sb_buffer[512];
    for (int i = 0; i < 512; i++) {
        sb_buffer[i] = ((uint8_t *)&sb)[i];
    }
    if (disk_write_sector(partition_lba, sb_buffer) != 0) {
        return -1;
    }
    
    /* Inicializar inodo raíz */
    mesafs_inode_t root_inode;
    root_inode.inode_num = 1;
    root_inode.type = MESAFS_TYPE_DIR;
    root_inode.flags = MESAFS_FLAG_USED;
    root_inode.size = 0;
    root_inode.blocks_used = 1;
    root_inode.direct_blocks[0] = 2; /* Bloque de datos del directorio */
    for (int i = 1; i < 10; i++) {
        root_inode.direct_blocks[i] = 0;
    }
    root_inode.indirect_block = 0;
    root_inode.created = 0;
    root_inode.modified = 0;
    
    /* Escribir inodo raíz */
    uint8_t inode_block[4096];
    for (int i = 0; i < 4096; i++) inode_block[i] = 0;
    mesafs_inode_t *inodes = (mesafs_inode_t *)inode_block;
    inodes[0] = root_inode;
    
    /* Bloque 1 contiene inodos */
    uint32_t inode_block_lba = partition_lba + 1;
    for (int i = 0; i < 8; i++) {
        if (disk_write_sector(inode_block_lba + i, inode_block + (i * 512)) != 0) {
            return -1;
        }
    }
    
    /* Inicializar bloque de directorio raíz vacío */
    uint8_t dir_block[4096];
    for (int i = 0; i < 4096; i++) dir_block[i] = 0;
    
    /* Bloque 2 es el directorio raíz */
    uint32_t dir_block_lba = partition_lba + 1 + 1; /* +1 por bloque de inodos */
    for (int i = 0; i < 8; i++) {
        if (disk_write_sector(dir_block_lba + i, dir_block + (i * 512)) != 0) {
            return -1;
        }
    }
    
    console_puts("[MesaFS] Format complete\n");
    return 0;
}

int mesafs_mount(uint32_t partition_lba, mesafs_t *fs) {
    if (!fs) {
        return -1;
    }
    
    console_puts("[MesaFS] Mounting filesystem...\n");
    
    /* Leer superbloque */
    uint8_t sb_buffer[512];
    if (disk_read_sector(partition_lba, sb_buffer) != 0) {
        console_puts("[MesaFS] Failed to read superblock\n");
        return -1;
    }
    
    mesafs_superblock_t *sb = (mesafs_superblock_t *)sb_buffer;
    
    /* Verificar magic */
    if (sb->magic != MESAFS_MAGIC) {
        console_puts("[MesaFS] Invalid filesystem (not MesaFS)\n");
        return -1;
    }
    
    /* Copiar superbloque */
    fs->sb = *sb;
    fs->partition_lba = partition_lba;
    
    /* Calcular sectores de la partición */
    /* Buscar partición por LBA en la lista */
    extern partition_info_t *partition_list_head;
    partition_info_t *p = partition_list_head;
    while (p) {
        if (p->lba_start == partition_lba) {
            fs->partition_sectors = p->num_sectors;
            break;
        }
        p = p->next;
    }
    
    if (!p) {
        /* Si no encontramos la partición, calcular desde total_blocks */
        fs->partition_sectors = fs->sb.total_blocks * 8;
    }
    
    /* Inicializar bitmaps (simplificado - por ahora no los usamos) */
    fs->block_bitmap.data = NULL;
    fs->block_bitmap.size_bits = 0;
    fs->inode_bitmap.data = NULL;
    fs->inode_bitmap.size_bits = 0;
    
    mounted_fs = fs;
    
    console_puts("       Total blocks: ");
    console_put_dec(fs->sb.total_blocks);
    console_puts("\n       Root inode: ");
    console_put_dec(fs->sb.root_inode);
    console_puts("\n       Status: ");
    console_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    console_puts("OK\n");
    console_set_color(VGA_WHITE, VGA_BLACK);
    
    return 0;
}

int mesafs_unmount(mesafs_t *fs) {
    (void)fs;
    mounted_fs = NULL;
    return 0;
}

/* Buscar archivo en directorio por nombre */
static uint32_t find_in_dir(mesafs_t *fs, uint32_t dir_inode, const char *name) {
    mesafs_inode_t dir_inode_data;
    if (mesafs_read_inode(fs, dir_inode, &dir_inode_data) != 0) {
        return 0;
    }
    
    if (dir_inode_data.type != MESAFS_TYPE_DIR) {
        return 0;
    }
    
    /* Leer bloque de directorio */
    uint8_t dir_block[4096];
    if (read_block(fs, dir_inode_data.direct_blocks[0], dir_block) != 0) {
        return 0;
    }
    
    /* Buscar entrada */
    mesafs_dirent_t *entries = (mesafs_dirent_t *)dir_block;
    for (int i = 0; i < 64; i++) { /* 64 entradas por bloque (4096/64) */
        if (entries[i].inode != 0 && entries[i].name_len > 0) {
            /* Comparar nombres */
            int match = 1;
            for (int j = 0; j < entries[i].name_len && j < 58; j++) {
                if (entries[i].name[j] != name[j]) {
                    match = 0;
                    break;
                }
            }
            if (match && name[entries[i].name_len] == '\0') {
                return entries[i].inode;
            }
        }
    }
    
    return 0;
}

/* Resolver path a inodo */
static uint32_t resolve_path(mesafs_t *fs, const char *path) {
    if (!fs || !path) {
        return 0;
    }
    
    /* Si es path absoluto, empezar desde raíz */
    uint32_t current_inode = fs->sb.root_inode;
    
    /* Si es path relativo, también empezar desde raíz por ahora */
    if (path[0] == '/') {
        path++; /* Saltar '/' */
    }
    
    /* Si path está vacío, retornar raíz */
    if (path[0] == '\0') {
        return current_inode;
    }
    
    /* Dividir path en componentes */
    const char *start = path;
    const char *end = path;
    
    while (*end != '\0') {
        /* Buscar siguiente '/' */
        while (*end != '/' && *end != '\0') {
            end++;
        }
        
        /* Extraer nombre de componente */
        char component[64];
        int len = 0;
        while (start < end && len < 63) {
            component[len++] = *start++;
        }
        component[len] = '\0';
        
        /* Buscar componente en directorio actual */
        current_inode = find_in_dir(fs, current_inode, component);
        if (current_inode == 0) {
            return 0; /* No encontrado */
        }
        
        /* Saltar '/' si existe */
        if (*end == '/') {
            end++;
            start = end;
        }
    }
    
    return current_inode;
}

int mesafs_read(mesafs_t *fs, uint32_t inode, void *buf, size_t count, uint64_t offset) {
    if (!fs || !buf || inode == 0) {
        return -1;
    }
    
    mesafs_inode_t inode_data;
    if (read_inode(fs, inode, &inode_data) != 0) {
        return -1;
    }
    
    if (inode_data.type != MESAFS_TYPE_FILE) {
        return -1;
    }
    
    if (offset >= inode_data.size) {
        return 0; /* EOF */
    }
    
    /* Limitar count al tamaño restante */
    if (offset + count > inode_data.size) {
        count = inode_data.size - offset;
    }
    
    /* Calcular bloque inicial */
    uint32_t start_block = offset / MESAFS_BLOCK_SIZE;
    uint32_t block_offset = offset % MESAFS_BLOCK_SIZE;
    
    uint8_t *dst = (uint8_t *)buf;
    size_t remaining = count;
    uint32_t block_idx = start_block;
    
    while (remaining > 0 && block_idx < inode_data.blocks_used) {
        if (block_idx >= 10) {
            break; /* Solo soportamos bloques directos por ahora */
        }
        
        uint32_t block_num = inode_data.direct_blocks[block_idx];
        if (block_num == 0) {
            break;
        }
        
        uint8_t block_buffer[4096];
        if (read_block(fs, block_num, block_buffer) != 0) {
            return -1;
        }
        
        size_t to_copy = MESAFS_BLOCK_SIZE - block_offset;
        if (to_copy > remaining) {
            to_copy = remaining;
        }
        
        for (size_t i = 0; i < to_copy; i++) {
            dst[i] = block_buffer[block_offset + i];
        }
        
        dst += to_copy;
        remaining -= to_copy;
        block_offset = 0;
        block_idx++;
    }
    
    return (int)(count - remaining);
}

int mesafs_write(mesafs_t *fs, uint32_t inode, const void *buf, size_t count, uint64_t offset) {
    (void)fs; (void)inode; (void)buf; (void)count; (void)offset;
    /* TODO: Implementar escritura */
    return -1;
}

int mesafs_list_dir(mesafs_t *fs, uint32_t dir_inode, mesafs_dirent_t *entries, size_t max_entries) {
    if (!fs || !entries || dir_inode == 0) {
        return -1;
    }
    
    mesafs_inode_t dir_inode_data;
    if (mesafs_read_inode(fs, dir_inode, &dir_inode_data) != 0) {
        return -1;
    }
    
    if (dir_inode_data.type != MESAFS_TYPE_DIR) {
        return -1;
    }
    
    /* Leer bloque de directorio */
    uint8_t dir_block[4096];
    if (read_block(fs, dir_inode_data.direct_blocks[0], dir_block) != 0) {
        return -1;
    }
    
    /* Copiar entradas válidas */
    mesafs_dirent_t *dir_entries = (mesafs_dirent_t *)dir_block;
    int count = 0;
    
    for (int i = 0; i < 64 && count < (int)max_entries; i++) {
        if (dir_entries[i].inode != 0 && dir_entries[i].name_len > 0) {
            entries[count] = dir_entries[i];
            count++;
        }
    }
    
    return count;
}

/* Funciones auxiliares */
uint32_t mesafs_alloc_block(mesafs_t *fs) {
    (void)fs;
    /* TODO: Implementar con bitmap */
    return 0;
}

void mesafs_free_block(mesafs_t *fs, uint32_t block) {
    (void)fs; (void)block;
    /* TODO: Implementar con bitmap */
}

uint32_t mesafs_alloc_inode(mesafs_t *fs) {
    (void)fs;
    /* TODO: Implementar con bitmap */
    return 0;
}

void mesafs_free_inode(mesafs_t *fs, uint32_t inode) {
    (void)fs; (void)inode;
    /* TODO: Implementar con bitmap */
}

/* Función para obtener el FS montado */
mesafs_t *mesafs_get_mounted(void) {
    return mounted_fs;
}

/* Función para buscar archivo por path */
uint32_t mesafs_find_file(mesafs_t *fs, const char *path) {
    return resolve_path(fs, path);
}

