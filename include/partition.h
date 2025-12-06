/**
 * @file partition.h
 * @brief Sistema de particiones MBR
 */

#ifndef _PARTITION_H_
#define _PARTITION_H_

#include "types.h"

#define PARTITION_BOOTABLE      0x80
#define PARTITION_NON_BOOTABLE  0x00

/* Tipos de partición comunes */
#define PART_TYPE_EMPTY         0x00
#define PART_TYPE_FAT12         0x01
#define PART_TYPE_FAT16_SMALL   0x04
#define PART_TYPE_EXTENDED      0x05
#define PART_TYPE_FAT16         0x06
#define PART_TYPE_NTFS          0x07
#define PART_TYPE_FAT32         0x0B
#define PART_TYPE_FAT32_LBA     0x0C
#define PART_TYPE_LINUX         0x83
#define PART_TYPE_MESAFS        0xA0  /* Nuestro FS personalizado */

#define MBR_SIGNATURE           0xAA55
#define MAX_PARTITIONS          4

/* Entrada de partición MBR (16 bytes) */
typedef struct PACKED {
    uint8_t  status;          /* 0x80 = bootable, 0x00 = no bootable */
    uint8_t  first_chs[3];    /* CHS del primer sector */
    uint8_t  type;            /* Tipo de partición */
    uint8_t  last_chs[3];     /* CHS del último sector */
    uint32_t lba_start;       /* LBA del primer sector */
    uint32_t num_sectors;     /* Número de sectores */
} mbr_partition_t;

/* Master Boot Record (512 bytes) */
typedef struct PACKED {
    uint8_t          boot_code[446];
    mbr_partition_t  partitions[MAX_PARTITIONS];
    uint16_t         signature;  /* Debe ser 0xAA55 */
} mbr_t;

/* Información de partición en memoria */
typedef struct partition_info {
    uint8_t  index;           /* Índice de partición (0-3) */
    uint8_t  type;            /* Tipo de partición */
    uint8_t  bootable;        /* ¿Es bootable? */
    uint32_t lba_start;       /* Sector inicial (LBA) */
    uint32_t num_sectors;     /* Número de sectores */
    uint64_t size_bytes;      /* Tamaño en bytes */
    struct partition_info *next;
} partition_info_t;

/* Variables globales */
extern partition_info_t *partition_list_head;

/* Funciones de particiones */
void partition_init(void);
int partition_read_mbr(mbr_t *mbr);
int partition_list(partition_info_t **list);
partition_info_t *partition_get(uint8_t index);
const char *partition_type_name(uint8_t type);
void partition_print_info(void);

#endif /* _PARTITION_H_ */