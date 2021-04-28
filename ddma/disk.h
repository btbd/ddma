#pragma once

#include <ntddk.h>

typedef struct _DISK {
    PDEVICE_OBJECT Device;

    // Buffer holding sectors original data
    UINT8 Buffer[PAGE_SIZE];
} DISK, *PDISK;

NTSTATUS DiskFind(OUT PDISK *disk);
NTSTATUS DiskCopy(IN PDISK disk, IN PVOID dest, IN PVOID src);
VOID DiskFree(IN PDISK disk);