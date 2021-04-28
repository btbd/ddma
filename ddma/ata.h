#pragma once

#include <ntddk.h>

#define ATA_IO_TIMEOUT (2)
#define ATA_CMD_READ_SECTORS (0x20)
#define ATA_CMD_WRITE_SECTORS (0x30)
#define ATA_DEVICE_TRANSPORT_LBA (0x40)
#define ATA_SECTOR_SIZE (0x200)

NTSTATUS AtaReadPage(IN PDEVICE_OBJECT device, OUT PVOID dest);
NTSTATUS AtaWritePage(IN PDEVICE_OBJECT device, IN PVOID src);