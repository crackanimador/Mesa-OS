/**
 * @file disk.c
 * @brief Driver de disco ATA/IDE
 */

#include "../include/disk.h"
#include "../include/console.h"
#include "../include/types.h"
#include "../include/io.h"
#include "../include/kheap.h"

/* Puertos ATA Primary (IDE Master) */
#define ATA_PRIMARY_DATA        0x1F0
#define ATA_PRIMARY_ERROR       0x1F1
#define ATA_PRIMARY_FEATURES    0x1F1
#define ATA_PRIMARY_SECTOR_COUNT 0x1F2
#define ATA_PRIMARY_LBA_LOW     0x1F3
#define ATA_PRIMARY_LBA_MID     0x1F4
#define ATA_PRIMARY_LBA_HIGH    0x1F5
#define ATA_PRIMARY_DRIVE       0x1F6
#define ATA_PRIMARY_STATUS      0x1F7
#define ATA_PRIMARY_COMMAND     0x1F7

/* Comandos ATA */
#define ATA_CMD_READ_SECTORS    0x20
#define ATA_CMD_WRITE_SECTORS   0x30
#define ATA_CMD_IDENTIFY        0xEC

/* Bits de estado */
#define ATA_STATUS_BSY          0x80
#define ATA_STATUS_DRDY         0x40
#define ATA_STATUS_DF           0x20
#define ATA_STATUS_DRQ          0x08
#define ATA_STATUS_ERR          0x01

/* Selección de disco */
#define ATA_DRIVE_MASTER        0xE0
#define ATA_DRIVE_LBA           0x40

static uint32_t disk_total_sectors = 0;
static uint8_t *virtual_disk = NULL;  /* Fallback: disco virtual en RAM */
static bool using_virtual_disk = false;

static void ata_wait_ready(void) {
    uint8_t status;
    int timeout = 100000;
    do {
        status = inb(ATA_PRIMARY_STATUS);
        if (--timeout == 0) {
            return; /* Timeout */
        }
    } while (status & ATA_STATUS_BSY);
}

static void ata_wait_data(void) {
    ata_wait_ready();
    uint8_t status;
    do {
        status = inb(ATA_PRIMARY_STATUS);
    } while (!(status & ATA_STATUS_DRQ));
}

static int ata_identify(void) {
    /* Seleccionar disco master */
    outb(ATA_PRIMARY_DRIVE, ATA_DRIVE_MASTER | ATA_DRIVE_LBA);
    
    /* Esperar un poco para que el controlador esté listo */
    for (volatile int i = 0; i < 10000; i++);
    
    /* Verificar si hay un disco presente */
    uint8_t status = inb(ATA_PRIMARY_STATUS);
    if (status == 0xFF) {
        /* Puerto flotante - no hay controlador */
        return -1;
    }
    
    /* Enviar comando IDENTIFY */
    outb(ATA_PRIMARY_SECTOR_COUNT, 0);
    outb(ATA_PRIMARY_LBA_LOW, 0);
    outb(ATA_PRIMARY_LBA_MID, 0);
    outb(ATA_PRIMARY_LBA_HIGH, 0);
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_IDENTIFY);
    
    /* Esperar respuesta */
    for (volatile int i = 0; i < 100000; i++) {
        status = inb(ATA_PRIMARY_STATUS);
        if (status & ATA_STATUS_ERR) {
            return -1; /* Error del disco */
        }
        if (status & ATA_STATUS_DRQ) {
            break; /* Datos listos */
        }
    }
    
    if (!(status & ATA_STATUS_DRQ)) {
        return -1; /* Timeout */
    }
    
    /* Leer información del disco (solo necesitamos el tamaño) */
    uint16_t identify_data[256];
    for (int i = 0; i < 256; i++) {
        uint16_t value;
        __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(ATA_PRIMARY_DATA));
        identify_data[i] = value;
    }
    
    /* Obtener tamaño del disco en sectores LBA28 */
    uint32_t lba28_sectors = (uint32_t)identify_data[60] | 
                            ((uint32_t)identify_data[61] << 16);
    
    if (lba28_sectors > 0 && lba28_sectors < 0xFFFFFFFF) {
        disk_total_sectors = lba28_sectors;
        return 0;
    }
    
    return -1;
}


void disk_init(void) {
    console_puts("[DISK] Initializing ATA/IDE disk...\n");
    
    /* Esperar un poco para que el disco esté listo */
    for (volatile int i = 0; i < 1000000; i++);
    
    if (ata_identify() == 0) {
        console_puts("       Total sectors: ");
        console_put_dec(disk_total_sectors);
        console_puts(" (");
        console_put_dec((disk_total_sectors * DISK_SECTOR_SIZE) / (1024 * 1024));
        console_puts(" MB)\n");
        console_puts("       Status: ");
        console_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        console_puts("OK\n");
        console_set_color(VGA_WHITE, VGA_BLACK);
    } else {
        /* Fallback: usar disco virtual en RAM cuando no hay disco físico */
        console_puts("       Physical disk not detected\n");
        console_puts("       Using virtual disk (");
        console_put_dec(DISK_TOTAL_SECTORS);
        console_puts(" sectors, ");
        console_put_dec((DISK_TOTAL_SECTORS * DISK_SECTOR_SIZE) / 1024);
        console_puts(" KB)\n");
        
        /* Asignar memoria para el disco virtual */
        size_t disk_size = DISK_SECTOR_SIZE * DISK_TOTAL_SECTORS;
        virtual_disk = (uint8_t *)kmalloc(disk_size);
        
        if (virtual_disk) {
            /* Inicializar disco con ceros */
            for (size_t i = 0; i < disk_size; i++) {
                virtual_disk[i] = 0;
            }
            using_virtual_disk = true;
            disk_total_sectors = DISK_TOTAL_SECTORS;
            console_puts("       Status: ");
            console_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
            console_puts("OK (Virtual)\n");
            console_set_color(VGA_WHITE, VGA_BLACK);
        } else {
            console_puts("       Status: ");
            console_set_color(VGA_LIGHT_RED, VGA_BLACK);
            console_puts("ERROR - Failed to allocate virtual disk\n");
            console_set_color(VGA_WHITE, VGA_BLACK);
            disk_total_sectors = 0;
        }
    }
}

int disk_read_sector(uint32_t lba, void *buffer) {
    if (disk_total_sectors == 0 || lba >= disk_total_sectors) {
        return -1;
    }
    
    /* Si estamos usando disco virtual, leer de RAM */
    if (using_virtual_disk && virtual_disk) {
        uint8_t *src = &virtual_disk[lba * DISK_SECTOR_SIZE];
        uint8_t *dst = (uint8_t *)buffer;
        for (size_t i = 0; i < DISK_SECTOR_SIZE; i++) {
            dst[i] = src[i];
        }
        return 0;
    }
    
    ata_wait_ready();
    
    /* Seleccionar disco master y modo LBA */
    outb(ATA_PRIMARY_DRIVE, ATA_DRIVE_MASTER | ATA_DRIVE_LBA | ((lba >> 24) & 0x0F));
    
    /* Configurar LBA */
    outb(ATA_PRIMARY_SECTOR_COUNT, 1);
    outb(ATA_PRIMARY_LBA_LOW, lba & 0xFF);
    outb(ATA_PRIMARY_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_PRIMARY_LBA_HIGH, (lba >> 16) & 0xFF);
    
    /* Enviar comando de lectura */
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_READ_SECTORS);
    
    ata_wait_data();
    
    /* Leer 256 palabras (512 bytes) */
    uint16_t *dst = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        uint16_t value;
        __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(ATA_PRIMARY_DATA));
        dst[i] = value;
    }
    
    return 0;
}

int disk_write_sector(uint32_t lba, const void *buffer) {
    if (disk_total_sectors == 0 || lba >= disk_total_sectors) {
        return -1;
    }
    
    /* Si estamos usando disco virtual, escribir a RAM */
    if (using_virtual_disk && virtual_disk) {
        uint8_t *dst = &virtual_disk[lba * DISK_SECTOR_SIZE];
        const uint8_t *src = (const uint8_t *)buffer;
        for (size_t i = 0; i < DISK_SECTOR_SIZE; i++) {
            dst[i] = src[i];
        }
        return 0;
    }
    
    ata_wait_ready();
    
    /* Seleccionar disco master y modo LBA */
    outb(ATA_PRIMARY_DRIVE, ATA_DRIVE_MASTER | ATA_DRIVE_LBA | ((lba >> 24) & 0x0F));
    
    /* Configurar LBA */
    outb(ATA_PRIMARY_SECTOR_COUNT, 1);
    outb(ATA_PRIMARY_LBA_LOW, lba & 0xFF);
    outb(ATA_PRIMARY_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_PRIMARY_LBA_HIGH, (lba >> 16) & 0xFF);
    
    /* Enviar comando de escritura */
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_WRITE_SECTORS);
    
    ata_wait_data();
    
    /* Escribir 256 palabras (512 bytes) */
    const uint16_t *src = (const uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        __asm__ volatile ("outw %0, %1" : : "a"(src[i]), "Nd"(ATA_PRIMARY_DATA));
    }
    
    /* Esperar a que termine la escritura */
    ata_wait_ready();
    
    return 0;
}

int disk_read_sectors(uint32_t lba, void *buffer, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        if (disk_read_sector(lba + i, (uint8_t *)buffer + (i * DISK_SECTOR_SIZE)) != 0) {
            return -1;
        }
    }
    return 0;
}

int disk_write_sectors(uint32_t lba, const void *buffer, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        if (disk_write_sector(lba + i, (const uint8_t *)buffer + (i * DISK_SECTOR_SIZE)) != 0) {
            return -1;
        }
    }
    return 0;
}

uint32_t disk_get_total_sectors(void) {
    return disk_total_sectors;
}