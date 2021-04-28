#include <ntifs.h>

#include "ata.h"
#include <ntddscsi.h>

static NTSTATUS AtaIssueCommand(IN PDEVICE_OBJECT device, IN USHORT flag, IN UCHAR command,
                                IN PVOID buffer) {

    KEVENT event;
    KeInitializeEvent(&event, SynchronizationEvent, FALSE);

    ATA_PASS_THROUGH_DIRECT request = {0};
    request.Length = sizeof(request);
    request.AtaFlags = flag | ATA_FLAGS_USE_DMA;
    request.DataTransferLength = PAGE_SIZE;
    request.TimeOutValue = ATA_IO_TIMEOUT;
    request.DataBuffer = buffer;

    // For the sake of brevity this uses the first sectors (unsafe!)
    request.CurrentTaskFile[1] = PAGE_SIZE / ATA_SECTOR_SIZE;
    request.CurrentTaskFile[5] = ATA_DEVICE_TRANSPORT_LBA;
    request.CurrentTaskFile[6] = command;

    IO_STATUS_BLOCK ioStatusBlock;
    PIRP irp = IoBuildDeviceIoControlRequest(IOCTL_ATA_PASS_THROUGH_DIRECT, device, &request,
                                             sizeof(request), &request, sizeof(request), FALSE,
                                             &event, &ioStatusBlock);

    if (!irp) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    NTSTATUS status = IoCallDriver(device, irp);

    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        status = ioStatusBlock.Status;
    }

    return status;
}

NTSTATUS AtaReadPage(IN PDEVICE_OBJECT device, OUT PVOID dest) {
    return AtaIssueCommand(device, ATA_FLAGS_DATA_IN, ATA_CMD_READ_SECTORS, dest);
}

NTSTATUS AtaWritePage(IN PDEVICE_OBJECT device, IN PVOID src) {
    return AtaIssueCommand(device, ATA_FLAGS_DATA_OUT, ATA_CMD_WRITE_SECTORS, src);
}