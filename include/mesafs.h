/**
 * @file mesafs.h  
 * @brief MesaFS - Sistema de archivos simple para MesaOS
 */

#ifndef _MESAFS_H_
#define _MESAFS_H_

#include "types.h"
#include "vfs.h"

#define MESAFS_MAGIC            0x4D455341  /* "MESA" */
#define MESAFS_VERSION          1
#define MESAFS_BLOCK_SIZE       4096
#define MESAFS_MAX_FILENAME     255
#define MESAFS_MAX_FILES        256
#define MESAFS_MAX_BLOCKS       8192

/* Tipos de archivo */
#define MESAFS_TYPE_FILE        0x01
#define MESAFS_TYPE_DIR         0x02

/* Flags de archivo */
#define MESAFS_FLAG_USED        0x01
#define MESAFS_FLAG_READONLY    0x02

/* Superbloque (1 bloque = 4096 bytes) */
typedef struct PACKED {
    uint32_t magic;           /* Firma mágica "MESA" */
    uint32_t version;         /* Versión del FS */
    uint32_t block_size;      /* Tamaño de bloque */
    uint32_t total_blocks;    /* Total de bloques */
    uint32_t free_blocks;     /* Bloques libres */
    uint32_t total_inodes;    /* Total de inodos */
    uint32_t free_inodes;     /* Inodos libres */
    uint32_t root_inode;      /* Inodo raíz */
    uint8_t  reserved[4068];  /* Reservado para futuro */
} mesafs_superblock_t;

/* Inodo (128 bytes) */
typedef struct PACKED {
    uint32_t inode_num;       /* Número de inodo */
    uint8_t  type;            /* Tipo (archivo/directorio) */
    uint8_t  flags;           /* Flags */
    uint16_t reserved1;
    uint32_t size;            /* Tamaño en bytes */
    uint32_t blocks_used;     /* Bloques usados */
    uint32_t direct_blocks[10]; /* Bloques directos */
    uint32_t indirect_block;  /* Bloque indirecto */
    uint64_t created;         /* Timestamp creación */
    uint64_t modified;        /* Timestamp modificación */
    uint8_t  reserved2[64];
} mesafs_inode_t;

/* Entrada de directorio (64 bytes) */
typedef struct PACKED {
    uint32_t inode;           /* Número de inodo */
    uint8_t  type;            /* Tipo de archivo */
    uint8_t  name_len;        /* Longitud del nombre */
    char     name[58];        /* Nombre del archivo */
} mesafs_dirent_t;

/* Bitmap de bloques/inodos */
typedef struct {
    uint8_t *data;
    uint32_t size_bits;
} mesafs_bitmap_t;

/* Estructura del sistema de archivos en memoria */
typedef struct {
    mesafs_superblock_t sb;
    mesafs_bitmap_t block_bitmap;
    mesafs_bitmap_t inode_bitmap;
    uint32_t partition_lba;
    uint32_t partition_sectors;
} mesafs_t;

/* Funciones del sistema de archivos */
int mesafs_format(uint32_t partition_lba, uint32_t num_sectors);
int mesafs_mount(uint32_t partition_lba, mesafs_t *fs);
int mesafs_unmount(mesafs_t *fs);

/* Operaciones con archivos */
int mesafs_create(mesafs_t *fs, const char *path, uint8_t type);
int mesafs_delete(mesafs_t *fs, const char *path);
int mesafs_read(mesafs_t *fs, uint32_t inode, void *buf, size_t count, uint64_t offset);
int mesafs_write(mesafs_t *fs, uint32_t inode, const void *buf, size_t count, uint64_t offset);
int mesafs_list_dir(mesafs_t *fs, uint32_t dir_inode, mesafs_dirent_t *entries, size_t max_entries);

/* Utilidades */
uint32_t mesafs_alloc_block(mesafs_t *fs);
void mesafs_free_block(mesafs_t *fs, uint32_t block);
uint32_t mesafs_alloc_inode(mesafs_t *fs);
void mesafs_free_inode(mesafs_t *fs, uint32_t inode);

/* Funciones auxiliares */
mesafs_t *mesafs_get_mounted(void);
uint32_t mesafs_find_file(mesafs_t *fs, const char *path);
int mesafs_read_inode(mesafs_t *fs, uint32_t inode_num, mesafs_inode_t *inode);

#endif /* _MESAFS_H_ */