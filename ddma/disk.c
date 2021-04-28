#include <ntifs.h>

#include "ata.h"
#include "disk.h"

NTKERNELAPI POBJECT_TYPE *IoDriverObjectType;
NTKERNELAPI NTSTATUS ObReferenceObjectByName(IN PUNICODE_STRING objectName, IN ULONG attributes,
                                             IN PACCESS_STATE passedAccessState,
                                             IN ACCESS_MASK desiredAccess,
                                             IN POBJECT_TYPE objectType,
                                             IN KPROCESSOR_MODE accessMode,
                                             IN OUT PVOID parseContext, OUT PVOID *object);

static NTSTATUS GetDeviceObjectList(IN PDRIVER_OBJECT driverObject, OUT PDEVICE_OBJECT **outDevices,
                                    OUT PULONG outDeviceCount) {

    ULONG count = 0;
    NTSTATUS status = IoEnumerateDeviceObjectList(driverObject, NULL, 0, &count);

    if (status != STATUS_BUFFER_TOO_SMALL) {
        return status;
    }

    ULONG size = count * sizeof(PDEVICE_OBJECT);
    PDEVICE_OBJECT *devices = ExAllocatePool(NonPagedPoolNx, size);
    if (devices) {
        *outDeviceCount = count;

        status = IoEnumerateDeviceObjectList(driverObject, devices, size, &count);
        if (NT_SUCCESS(status)) {
            *outDevices = devices;
        } else {
            ExFreePool(devices);
        }
    } else {
        status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return status;
}

NTSTATUS DiskFind(OUT PDISK *outDisk) {
    PDISK disk = ExAllocatePool(NonPagedPoolNx, sizeof(DISK));
    if (!disk) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    UNICODE_STRING diskStr = RTL_CONSTANT_STRING(L"\\Driver\\Disk");
    PDRIVER_OBJECT diskObject;

    NTSTATUS status = ObReferenceObjectByName(&diskStr, OBJ_CASE_INSENSITIVE, NULL, 0,
                                              *IoDriverObjectType, KernelMode, NULL, &diskObject);

    if (NT_SUCCESS(status)) {
        PDEVICE_OBJECT *devices;
        ULONG deviceCount;

        status = GetDeviceObjectList(diskObject, &devices, &deviceCount);

        if (NT_SUCCESS(status)) {
            status = STATUS_NOT_FOUND;

            for (ULONG i = 0; i < deviceCount; ++i) {
                PDEVICE_OBJECT device = devices[i];

                if (status == STATUS_NOT_FOUND && NT_SUCCESS(AtaReadPage(device, disk->Buffer))) {
                    disk->Device = device;
                    status = STATUS_SUCCESS;
                    continue;
                }

                ObDereferenceObject(device);
            }

            ExFreePool(devices);
        }

        ObDereferenceObject(diskObject);
    }

    if (NT_SUCCESS(status)) {
        *outDisk = disk;
    } else {
        ExFreePool(disk);
    }

    return status;
}

NTSTATUS DiskCopy(IN PDISK disk, IN PVOID dest, IN PVOID src) {
    // Read from src by writing to disk
    NTSTATUS status = AtaWritePage(disk->Device, src);
    if (NT_SUCCESS(status)) {
        // Write to dest by reading from disk
        status = AtaReadPage(disk->Device, dest);

        // Restore original sectors
        AtaWritePage(disk->Device, disk->Buffer);
    }

    return status;
}

VOID DiskFree(IN PDISK disk) {
    ObDereferenceObject(disk->Device);
    ExFreePool(disk);
}