#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h> // gBS
#include "gpu_memory.h"
#include "sync.h"

// Definition of the global allocator instance
struct GpuMemoryAllocator gpuMemAllocator;

/* -------------------------------------------------------------------------
 * Internal Helper Functions
 * ------------------------------------------------------------------------- */

BOOLEAN CanAlloc(
    IN UINT32 startPage,
    IN UINT32 pagesCount)
{
    if (startPage + pagesCount > gpuMemAllocator.pageCount) {
        return FALSE;
    }

    // Iterate through pages to check if pages are free
    for (UINT32 i = 0; i < pagesCount; i++) {
        if (gpuMemAllocator.pageStatus[startPage + i] == TRUE) {
            return FALSE;
        }
    }

    return TRUE;
}

VOID SetStatusPage(
    IN CONST UINT32 FromPage,
    IN CONST UINT32 ToPage,
    IN CONST BOOLEAN value)
{
    for(UINT32 i = FromPage; i < ToPage; i++){
        gpuMemAllocator.pageStatus[i] = value;
    }
}

/* -------------------------------------------------------------------------
 * Memory Allocation Functions
 * ------------------------------------------------------------------------- */

EFI_STATUS EFIAPI GpuMemoryAllocatorInit(
    IN EFI_PCI_IO_PROTOCOL *PciIo,
    IN UINT32 VRAMsize,
    IN VRAMADDR baseAddr,
    IN GPU_DMA_FENCE *Fence)
{
    // Basic size info
    gpuMemAllocator.pageCount      = VRAMsize / PAGE_SIZE;
    gpuMemAllocator.totalMemSize   = VRAMsize;

    // Allocate allocation tracker arrays and zero them out
    gpuMemAllocator.pageStatus      = AllocateZeroPool(gpuMemAllocator.pageCount * sizeof(BOOLEAN));
    gpuMemAllocator.pageAllocationSizes = AllocateZeroPool(gpuMemAllocator.pageCount * sizeof(UINT32));

    // Allocate tags array only in DEBUG mode
#ifdef MEM_DEBUG
    gpuMemAllocator.allocationTags  = AllocateZeroPool(gpuMemAllocator.pageCount * sizeof(CHAR8*));
    if (!gpuMemAllocator.pageStatus || !gpuMemAllocator.pageAllocationSizes || !gpuMemAllocator.allocationTags) {
        return EFI_OUT_OF_RESOURCES;
    }
#else
    if (!gpuMemAllocator.pageStatus || !gpuMemAllocator.pageAllocationSizes) {
        return EFI_OUT_OF_RESOURCES;
    }
#endif

    gpuMemAllocator.baseAddr = baseAddr;

    // PCI IO init
    gpuMemAllocator.PciIo          = PciIo;
    gpuMemAllocator.Fence          = Fence;
    gpuMemAllocator.VramBarIndex   = 1;

    return EFI_SUCCESS;
}

// Implementation with conditional compilation for Tag argument
#ifdef MEM_DEBUG
VRAMADDR GpuAllocateMemImpl(IN UINT32 bytesToAlloc, IN CHAR8 *Tag)
#else
VRAMADDR GpuAllocateMemImpl(IN UINT32 bytesToAlloc)
#endif
{
    UINT32 pageCounter = 0;
    UINT32 pagesToAlloc = (bytesToAlloc + PAGE_SIZE - 1) / PAGE_SIZE;

    while (pageCounter < gpuMemAllocator.pageCount) {
        if (gpuMemAllocator.pageStatus[pageCounter] == FALSE) {
            if (CanAlloc(pageCounter, pagesToAlloc)) {

                SetStatusPage(pageCounter, pageCounter + pagesToAlloc, TRUE);
                gpuMemAllocator.pageAllocationSizes[pageCounter] = pagesToAlloc;

#ifdef MEM_DEBUG
                // Store the tag pointer (assumed to be a string literal or persistent)
                gpuMemAllocator.allocationTags[pageCounter] = Tag;
#endif
                return pageCounter * PAGE_SIZE;
            }
        }
        else if (gpuMemAllocator.pageAllocationSizes[pageCounter] != 0) {
            pageCounter += gpuMemAllocator.pageAllocationSizes[pageCounter];
            continue;
        }

        pageCounter++;
    }

    return GPU_NO_MEM; // Error Code
}

#ifdef MEM_DEBUG
BOOLEAN GpuAllocateMemAtImpl(IN UINT32 bytesToAlloc, IN VRAMADDR addr, IN CHAR8 *Tag)
#else
BOOLEAN GpuAllocateMemAtImpl(IN UINT32 bytesToAlloc, IN VRAMADDR addr)
#endif
{
    if (addr % PAGE_SIZE != 0) return FALSE;

    UINT32 page = addr / PAGE_SIZE;
    UINT32 pagesToAlloc = (bytesToAlloc + PAGE_SIZE - 1) / PAGE_SIZE;

    if (CanAlloc(page, pagesToAlloc)) {
        SetStatusPage(page, page + pagesToAlloc, TRUE);
        gpuMemAllocator.pageAllocationSizes[page] = pagesToAlloc;

#ifdef MEM_DEBUG
        gpuMemAllocator.allocationTags[page] = Tag;
#endif
        return TRUE;
    }

    return FALSE;
}

UINT32 GpuGetAllocatedSize(IN VRAMADDR addr)
{
    UINT32 page = addr / PAGE_SIZE;

    return gpuMemAllocator.pageAllocationSizes[page] * PAGE_SIZE;
}

BOOLEAN GpuFreeMem(IN VRAMADDR addr)
{
    if (addr % PAGE_SIZE != 0) return FALSE;

    UINT32 page = addr / PAGE_SIZE;

    // Safety check for bounds
    if (page >= gpuMemAllocator.pageCount) return FALSE;

    if (gpuMemAllocator.pageAllocationSizes[page] == 0) {
        return FALSE;
    }

    SetStatusPage(page, page + gpuMemAllocator.pageAllocationSizes[page], FALSE);
    gpuMemAllocator.pageAllocationSizes[page] = 0;

#ifdef MEM_DEBUG
    gpuMemAllocator.allocationTags[page] = NULL;
#endif

    return TRUE;
}


/* -------------------------------------------------------------------------
 * Read/Write Helper Functions
 * ------------------------------------------------------------------------- */

EFI_STATUS EFIAPI GpuVramWrite(
    IN VRAMADDR DestAddr,
    IN VOID* SourcePtr,
    IN UINT32 Size)
{
    EFI_STATUS Status;
    UINT32 i = 0;
    UINT8* SourceBytes = (UINT8*)SourcePtr;

    if (gpuMemAllocator.PciIo == NULL) {
        return EFI_NOT_READY;
    }

    // Write in 32-bit chunks
    while (i + sizeof(UINT32) <= Size) {
        Status = gpuMemAllocator.PciIo->Mem.Write(
            gpuMemAllocator.PciIo,
            EfiPciIoWidthUint32,
            gpuMemAllocator.VramBarIndex,
            DestAddr + i,
            1,
            SourceBytes + i
        );

        if (EFI_ERROR(Status)) return Status;
        i += sizeof(UINT32);
    }

    // Handle remaining bytes
    while (i < Size) {
        Status = gpuMemAllocator.PciIo->Mem.Write(
            gpuMemAllocator.PciIo,
            EfiPciIoWidthUint8,
            gpuMemAllocator.VramBarIndex,
            DestAddr + i,
            1,
            SourceBytes + i
        );

        if (EFI_ERROR(Status)) return Status;
        i += sizeof(UINT8);
    }

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI GpuDmaWrite(
    IN VRAMADDR DestAddr,
    IN VOID*    SourcePtr,
    IN UINT32   Size)
{
    EFI_STATUS Status;
    EFI_PCI_IO_PROTOCOL_OPERATION Operation = EfiPciIoOperationBusMasterRead;
    UINTN NumberOfBytes = Size;

    if (gpuMemAllocator.PciIo == NULL || gpuMemAllocator.Fence == NULL) return EFI_NOT_READY;

    // Ensure prevoius DMA completion
    GpuDmaSync();

    // Map requested buffer
    Status = gpuMemAllocator.PciIo->Map(
        gpuMemAllocator.PciIo,
        Operation,
        SourcePtr,
        &NumberOfBytes,
        &gpuMemAllocator.Fence->DeviceAdress,
        &gpuMemAllocator.Fence->MapPtr
    );
    if (EFI_ERROR(Status)) return Status;

    gpuMemAllocator.Fence->DmaBusy = TRUE;

    // Set DMA MMIO registers
    GpuMmioWrite32(REG_DMA_HOST_ADDR, (UINT32)gpuMemAllocator.Fence->DeviceAdress);
    GpuMmioWrite32(REG_DMA_VRAM_ADDR, DestAddr);
    GpuMmioWrite32(REG_DMA_SIZE_ADDR, Size);

    // Kick the DMA transfer
    GpuMmioWrite32(REG_DMA_CMD_ADDR, GPU_DMA_CMD_START | GPU_DMA_CMD_TO_VRAM);

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI GpuVramRead(
    IN VOID* DestPtr,
    IN VRAMADDR SourceAddr,
    IN UINT32 Size)
{
    EFI_STATUS Status;
    UINT32 i = 0;
    UINT8* DestBytes = (UINT8*)DestPtr;

    if (gpuMemAllocator.PciIo == NULL) {
        return EFI_NOT_READY;
    }

    // Read in 32-bit chunks
    while (i + sizeof(UINT32) <= Size) {
        Status = gpuMemAllocator.PciIo->Mem.Read(
            gpuMemAllocator.PciIo,
            EfiPciIoWidthUint32,
            gpuMemAllocator.VramBarIndex,
            SourceAddr + i,
            1,
            DestBytes + i
        );

        if (EFI_ERROR(Status)) return Status;
        i += sizeof(UINT32);
    }

    // Handle remaining bytes
    while (i < Size) {
        Status = gpuMemAllocator.PciIo->Mem.Read(
            gpuMemAllocator.PciIo,
            EfiPciIoWidthUint8,
            gpuMemAllocator.VramBarIndex,
            SourceAddr + i,
            1,
            DestBytes + i
        );

        if (EFI_ERROR(Status)) return Status;
        i += sizeof(UINT8);
    }

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI GpuVramSet(
    IN VRAMADDR DestAddr,
    IN UINT8 Value,
    IN UINT32 Size)
{
    EFI_STATUS Status;
    UINT32 i = 0;

    // Create a 32-bit pattern
    UINT32 Pattern = (Value << 24) | (Value << 16) | (Value << 8) | Value;

    if (gpuMemAllocator.PciIo == NULL) {
        return EFI_NOT_READY;
    }

    // Fill using 32-bit writes
    while (i + sizeof(UINT32) <= Size) {
        Status = gpuMemAllocator.PciIo->Mem.Write(
            gpuMemAllocator.PciIo,
            EfiPciIoWidthUint32,
            gpuMemAllocator.VramBarIndex,
            DestAddr + i,
            1,
            &Pattern
        );

        if (EFI_ERROR(Status)) return Status;
        i += sizeof(UINT32);
    }

    // Fill remaining bytes
    while (i < Size) {
        Status = gpuMemAllocator.PciIo->Mem.Write(
            gpuMemAllocator.PciIo,
            EfiPciIoWidthUint8,
            gpuMemAllocator.VramBarIndex,
            DestAddr + i,
            1,
            &Value
        );

        if (EFI_ERROR(Status)) return Status;
        i += sizeof(UINT8);
    }

    return EFI_SUCCESS;
}

/* -------------------------------------------------------------------------
 * MMIO Register Access Functions
 * ------------------------------------------------------------------------- */

// --- 32-bit Registers ---

EFI_STATUS EFIAPI GpuMmioWrite32(IN UINT32 Offset, IN UINT32 Value)
{
    if (gpuMemAllocator.PciIo == NULL) {
        return EFI_NOT_READY;
    }

    return gpuMemAllocator.PciIo->Mem.Write(
        gpuMemAllocator.PciIo,
        EfiPciIoWidthUint32,
        gpuMemAllocator.MmioBarIndex,
        Offset,
        1,
        &Value
    );
}

UINT32 EFIAPI GpuMmioRead32(IN UINT32 Offset)
{
    UINT32 Value = 0;

    if (gpuMemAllocator.PciIo != NULL) {
        gpuMemAllocator.PciIo->Mem.Read(
            gpuMemAllocator.PciIo,
            EfiPciIoWidthUint32,
            gpuMemAllocator.MmioBarIndex,
            Offset,
            1,
            &Value
        );
    }

    return Value;
}

// --- 16-bit Registers ---

EFI_STATUS EFIAPI GpuMmioWrite16(IN UINT32 Offset, IN UINT16 Value)
{
    if (gpuMemAllocator.PciIo == NULL) {
        return EFI_NOT_READY;
    }

    return gpuMemAllocator.PciIo->Mem.Write(
        gpuMemAllocator.PciIo,
        EfiPciIoWidthUint16,
        gpuMemAllocator.MmioBarIndex,
        Offset,
        1,
        &Value
    );
}

UINT16 EFIAPI GpuMmioRead16(IN UINT32 Offset)
{
    UINT16 Value = 0;

    if (gpuMemAllocator.PciIo != NULL) {
        gpuMemAllocator.PciIo->Mem.Read(
            gpuMemAllocator.PciIo,
            EfiPciIoWidthUint16,
            gpuMemAllocator.MmioBarIndex,
            Offset,
            1,
            &Value
        );
    }

    return Value;
}

// --- 8-bit Registers ---

EFI_STATUS EFIAPI GpuMmioWrite8(IN UINT32 Offset, IN UINT8 Value)
{
    if (gpuMemAllocator.PciIo == NULL) {
        return EFI_NOT_READY;
    }

    return gpuMemAllocator.PciIo->Mem.Write(
        gpuMemAllocator.PciIo,
        EfiPciIoWidthUint8,
        gpuMemAllocator.MmioBarIndex,
        Offset,
        1,
        &Value
    );
}

UINT8 EFIAPI GpuMmioRead8(IN UINT32 Offset)
{
    UINT8 Value = 0;

    if (gpuMemAllocator.PciIo != NULL) {
        gpuMemAllocator.PciIo->Mem.Read(
            gpuMemAllocator.PciIo,
            EfiPciIoWidthUint8,
            gpuMemAllocator.MmioBarIndex,
            Offset,
            1,
            &Value
        );
    }

    return Value;
}

/* -------------------------------------------------------------------------
 * Debug Functions
 * ------------------------------------------------------------------------- */

VOID EFIAPI GpuDebugPrintAllocatorStats(VOID)
{
    UINT32 usedPages = 0;
    UINT32 freePages = 0;
    UINT32 i;

    for (i = 0; i < gpuMemAllocator.pageCount; i++) {
        if (gpuMemAllocator.pageStatus[i] == TRUE) {
            usedPages++;
        } else {
            freePages++;
        }
    }

    UINT32 usedBytes = usedPages * PAGE_SIZE;
    UINT32 freeBytes = freePages * PAGE_SIZE;
    UINT32 totalBytes = gpuMemAllocator.totalMemSize;

    DEBUG ((EFI_D_INFO, "\n================ VRAM ALLOCATOR STATS ================\n"));
    DEBUG ((EFI_D_INFO, "Total Memory:  %d Bytes (%d Pages)\n", totalBytes, gpuMemAllocator.pageCount));
    DEBUG ((EFI_D_INFO, "Used Memory:   %d Bytes (%d Pages)\n", usedBytes, usedPages));
    DEBUG ((EFI_D_INFO, "Free Memory:   %d Bytes (%d Pages)\n", freeBytes, freePages));

    if (totalBytes > 0) {
        UINT32 percent = (usedBytes * 100) / totalBytes;
        DEBUG ((EFI_D_INFO, "Utilization:   %d%%\n", percent));
    }
    DEBUG ((EFI_D_INFO, "======================================================\n\n"));
}
VOID EFIAPI GpuDebugDumpMemoryMap(VOID)
{
    if (gpuMemAllocator.pageCount == 0) return;

    UINT32 startPage = 0;
    BOOLEAN currentStatus = gpuMemAllocator.pageStatus[0];
    UINT32 i;

    DEBUG ((EFI_D_INFO, "---------------------------- VRAM MEMORY MAP ----------------------------\n"));
    DEBUG ((EFI_D_INFO, "| [START ADDR] - [ END ADDR ] : STAT (      SIZE      ) | TAG           |\n"));
    DEBUG ((EFI_D_INFO, "=========================================================================\n"));

    // Iterate through pages + 1 (to handle the last block closure)
    for (i = 1; i <= gpuMemAllocator.pageCount; i++) {

        BOOLEAN nextStatus = (i < gpuMemAllocator.pageCount) ? gpuMemAllocator.pageStatus[i] : !currentStatus;
        BOOLEAN splitBlock = FALSE;

        // Condition 1: Status Changed (Free <-> Used)
        if (nextStatus != currentStatus) {
            splitBlock = TRUE;
        }
        // Condition 2: We are inside a USED block, but we hit a new Allocation Header
        else if (currentStatus == TRUE && i < gpuMemAllocator.pageCount) {
            if (gpuMemAllocator.pageAllocationSizes[i] > 0) {
                splitBlock = TRUE;
            }
        }

        // If block ends or new one starts
        if (splitBlock) {
            UINT32 endPage = i;
            UINT32 count = endPage - startPage;
            UINT32 bytes = count * PAGE_SIZE;

            VRAMADDR startAddr = startPage * PAGE_SIZE;
            VRAMADDR endAddr   = (endPage * PAGE_SIZE) - 1;

            CHAR8 *tagName = "";

#ifdef MEM_DEBUG
            // Handle Tag display safely (handles NULL or missing tags)
            if (currentStatus == TRUE) {
                if (gpuMemAllocator.allocationTags[startPage] != NULL) {
                    tagName = gpuMemAllocator.allocationTags[startPage];
                } else {
                    tagName = "<UNTAGGED>"; // Explicitly mark allocations without tags
                }
            }
#endif

            DEBUG ((EFI_D_INFO, "  [0x%08X] - [0x%08X] : %a (%10d Bytes) | %a\n",
                startAddr,
                endAddr,
                currentStatus ? "USED" : "FREE",
                bytes,
                tagName
            ));

            if (currentStatus == TRUE) {
                UINT32 recordedSize = gpuMemAllocator.pageAllocationSizes[startPage];
                if (recordedSize != count) {
                    DEBUG ((EFI_D_INFO, "    -> WARNING: Header says %d pages, but found %d contiguous pages!\n", recordedSize, count));
                }
            }

            // Reset for next block
            currentStatus = nextStatus;
            startPage = i;
        }
    }
    DEBUG ((EFI_D_INFO, "-------------------------------------------------------------------------\n\n"));
}