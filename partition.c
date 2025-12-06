/**
 * @file partition.c
 * @brief Sistema de particiones MBR
 */

#include "../include/partition.h"
#include "../include/disk.h"
#include "../include/console.h"
#include "../include/kheap.h"

partition_info_t *partition_list_head = NULL;

const char *partition_type_name(uint8_t type) {
    switch (type) {
        case PART_TYPE_EMPTY:       return "Empty";
        case PART_TYPE_FAT12:       return "FAT12";
        case PART_TYPE_FAT16_SMALL: return "FAT16 (small)";
        case PART_TYPE_EXTENDED:    return "Extended";
        case PART_TYPE_FAT16:       return "FAT16";
        case PART_TYPE_NTFS:        return "NTFS";
        case PART_TYPE_FAT32:       return "FAT32";
        case PART_TYPE_FAT32_LBA:   return "FAT32 (LBA)";
        case PART_TYPE_LINUX:       return "Linux";
        case PART_TYPE_MESAFS:      return "MesaFS";
        default:                    return "Unknown";
    }
}

int partition_read_mbr(mbr_t *mbr) {
    if (!mbr) return -1;
    
    /* Leer el sector 0 (MBR) */
    if (disk_read_sector(0, mbr) != 0) {
        console_puts("[PART] ERROR: Failed to read MBR\n");
        return -1;
    }
    
    /* Verificar firma */
    if (mbr->signature != MBR_SIGNATURE) {
        console_puts("[PART] ERROR: Invalid MBR signature\n");
        return -1;
    }
    
    return 0;
}

void partition_init(void) {
    console_puts("[PART] Initializing partition table...\n");
    
    mbr_t mbr;
    if (partition_read_mbr(&mbr) != 0) {
        console_puts("[PART] No valid MBR found, creating default...\n");
        
        /* Crear MBR por defecto con una partición MesaFS */
        for (int i = 0; i < 446; i++) mbr.boot_code[i] = 0;
        
        /* Partición 1: MesaFS (8 MB) */
        mbr.partitions[0].status = PARTITION_NON_BOOTABLE;
        mbr.partitions[0].type = PART_TYPE_MESAFS;
        mbr.partitions[0].lba_start = 2048;  /* Empezar en 1 MB */
        mbr.partitions[0].num_sectors = 16384; /* 8 MB */
        
        /* Partición 2: Datos (4 MB) */
        mbr.partitions[1].status = PARTITION_NON_BOOTABLE;
        mbr.partitions[1].type = PART_TYPE_FAT16;
        mbr.partitions[1].lba_start = 18432;  /* Después de la partición 1 */
        mbr.partitions[1].num_sectors = 8192; /* 4 MB */
        
        /* Resto de particiones vacías */
        for (int i = 1; i < MAX_PARTITIONS; i++) {
            mbr.partitions[i].status = PARTITION_NON_BOOTABLE;
            mbr.partitions[i].type = PART_TYPE_EMPTY;
            mbr.partitions[i].lba_start = 0;
            mbr.partitions[i].num_sectors = 0;
        }
        
        mbr.signature = MBR_SIGNATURE;
        
        /* Escribir MBR */
        if (disk_write_sector(0, &mbr) != 0) {
            console_puts("[PART] ERROR: Failed to write MBR\n");
            return;
        }
    }
    
    /* Listar particiones */
    console_puts("[PART] Partition table:\n");
    for (int i = 0; i < MAX_PARTITIONS; i++) {
        if (mbr.partitions[i].type != PART_TYPE_EMPTY) {
            partition_info_t *pinfo = (partition_info_t *)kmalloc(sizeof(partition_info_t));
            if (!pinfo) continue;
            
            pinfo->index = i;
            pinfo->type = mbr.partitions[i].type;
            pinfo->bootable = (mbr.partitions[i].status == PARTITION_BOOTABLE);
            pinfo->lba_start = mbr.partitions[i].lba_start;
            pinfo->num_sectors = mbr.partitions[i].num_sectors;
            pinfo->size_bytes = (uint64_t)pinfo->num_sectors * DISK_SECTOR_SIZE;
            pinfo->next = partition_list_head;
            partition_list_head = pinfo;
            
            console_puts("       [");
            console_put_dec(i);
            console_puts("] ");
            console_puts(partition_type_name(pinfo->type));
            console_puts(" - ");
            console_put_dec(pinfo->size_bytes / 1024 / 1024);
            console_puts(" MB (LBA: ");
            console_put_dec(pinfo->lba_start);
            console_puts(")\n");
        }
    }
    
    console_puts("       Status: ");
    console_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    console_puts("OK\n");
    console_set_color(VGA_WHITE, VGA_BLACK);
}

partition_info_t *partition_get(uint8_t index) {
    partition_info_t *p = partition_list_head;
    while (p) {
        if (p->index == index) return p;
        p = p->next;
    }
    return NULL;
}

int partition_list(partition_info_t **list) {
    *list = partition_list_head;
    return 0;
}

void partition_print_info(void) {
    console_puts("\n[PART] Partition Space Report:\n");
    console_puts("       ========================================\n");
    
    partition_info_t *p = partition_list_head;
    uint64_t total_used = 0;
    
    if (!p) {
        console_puts("       No partitions found\n");
        return;
    }
    
    while (p) {
        console_puts("       Partition ");
        console_put_dec(p->index);
        console_puts(": ");
        console_puts(partition_type_name(p->type));
        console_puts("\n");
        
        console_puts("         Total Size: ");
        console_put_dec(p->size_bytes / 1024);
        console_puts(" KB (");
        console_put_dec(p->num_sectors);
        console_puts(" sectors)\n");
        
        console_puts("         LBA Range:  ");
        console_put_dec(p->lba_start);
        console_puts(" - ");
        console_put_dec(p->lba_start + p->num_sectors - 1);
        console_puts("\n");
        
        total_used += p->size_bytes;
        p = p->next;
    }
    
    /* Calcular espacio total del disco */
    uint64_t total_disk = (uint64_t)disk_get_total_sectors() * DISK_SECTOR_SIZE;
    uint64_t free_space = total_disk - total_used;
    
    console_puts("       ========================================\n");
    console_puts("       Total Disk:  ");
    console_put_dec(total_disk / 1024);
    console_puts(" KB\n");
    
    console_puts("       Used Space:  ");
    console_put_dec(total_used / 1024);
    console_puts(" KB\n");
    
    console_puts("       Free Space:  ");
    console_put_dec(free_space / 1024);
    console_puts(" KB\n");
    console_puts("       ========================================\n");
}