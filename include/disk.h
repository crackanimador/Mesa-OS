/**
 * @file disk.h
 * @brief Driver de disco ATA/IDE
 */

#ifndef _DISK_H_
#define _DISK_H_

#include "types.h"

#define DISK_SECTOR_SIZE    512
#define DISK_TOTAL_SECTORS  32768  /* Tamaño por defecto si no se detecta disco */

/* Funciones del disco */
void disk_init(void);
int disk_read_sector(uint32_t lba, void *buffer);
int disk_write_sector(uint32_t lba, const void *buffer);
int disk_read_sectors(uint32_t lba, void *buffer, uint32_t count);
int disk_write_sectors(uint32_t lba, const void *buffer, uint32_t count);
uint32_t disk_get_total_sectors(void);

#endif /* _DISK_H_ */