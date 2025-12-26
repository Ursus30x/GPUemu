#include "ringbuffer.h"
#include "gpu_memory.h"
#include "gpu_hw.h" 
#include <Library/UefiBootServicesTableLib.h> // gBS

struct GpuRingBuffer gpuRingBuffer;

EFI_STATUS EFIAPI GpuRingBufferInit(
    IN UINT32   RingSize)
{
    gpuRingBuffer.bufferSize = RingSize;
    
    // Allocate VRAM
    gpuRingBuffer.bufferStartAddr = GpuAllocateMem(RingSize, "RINGBUFFER");
    if (gpuRingBuffer.bufferStartAddr == 0) return EFI_OUT_OF_RESOURCES;
    
    GpuDebugPrintAllocatorStats();
    GpuDebugDumpMemoryMap();


    gpuRingBuffer.bufferEndAddr = gpuRingBuffer.bufferStartAddr + RingSize;

    // Reset pointers
    gpuRingBuffer.ringHead = gpuRingBuffer.bufferStartAddr;
    gpuRingBuffer.ringTail = gpuRingBuffer.bufferStartAddr;

    // Initialize HW registers
    GpuMmioWrite32(REG_GPU_MODE_ADDR, 0); // Disable Command Processor
    GpuMmioWrite32(REG_RING_BUFFER_TAIL, gpuRingBuffer.ringTail);
    GpuMmioWrite32(REG_RING_BUFFER_HEAD, gpuRingBuffer.ringHead);
    GpuMmioWrite32(REG_GPU_MODE_ADDR, 1); // Re-enable
    
    // Allocate Host Staging Buffer
    gpuRingBuffer.cmdBatchBufferPtr = AllocatePool(RingSize); 
    if (gpuRingBuffer.cmdBatchBufferPtr == NULL) {
        GpuFreeMem(gpuRingBuffer.bufferStartAddr);
        return EFI_OUT_OF_RESOURCES;
    }

    gpuRingBuffer.cmdBatchBufferSize = RingSize;
    gpuRingBuffer.cmdBatchCursor = 0;

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI GpuRingBufferDestroy()
{
    if (gpuRingBuffer.bufferStartAddr != 0) {
        GpuFreeMem(gpuRingBuffer.bufferStartAddr);
    }
    if (gpuRingBuffer.cmdBatchBufferPtr != NULL) {
        FreePool(gpuRingBuffer.cmdBatchBufferPtr);
    }
    SetMem(&gpuRingBuffer, sizeof(struct GpuRingBuffer), 0);
    return EFI_SUCCESS;
}

VRAMADDR EFIAPI GpuReadRingTail()
{
    VRAMADDR hwTail = GpuMmioRead32(REG_RING_BUFFER_TAIL);
    gpuRingBuffer.ringTail = hwTail;
    return hwTail;
}

UINT32 EFIAPI GpuRingBufferGetFreeSpace()
{
    // RAM-only calculation (Fast Path)
    VRAMADDR head = gpuRingBuffer.ringHead;
    VRAMADDR tail = gpuRingBuffer.ringTail;
    UINT32 size = gpuRingBuffer.bufferSize;

    if (head >= tail) {
        return (size - (head - tail)) - 4; 
    } else {
        return (tail - head) - 4;
    }
}

EFI_STATUS EFIAPI GpuRingBufferWaitSpace(IN UINT32 bytesNeeded)
{
    // Fast path: Check RAM cache
    if (GpuRingBufferGetFreeSpace() >= bytesNeeded) return EFI_SUCCESS;

    // Slow path: Update cache from HW
    GpuReadRingTail();
    if (GpuRingBufferGetFreeSpace() >= bytesNeeded) return EFI_SUCCESS;

    // Stall path: Wait for GPU
    UINT32 attempts = 0;
    while (GpuRingBufferGetFreeSpace() < bytesNeeded) {
        gBS->Stall(10); 
        GpuReadRingTail();

        if (++attempts > 100000) {
            DEBUG((EFI_D_ERROR, "GPU Stuck! Tail: 0x%X Head: 0x%X\n", gpuRingBuffer.ringTail, gpuRingBuffer.ringHead));
            return EFI_TIMEOUT;
        }
    }
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI GpuRingBufferFlush()
{
    if (gpuRingBuffer.cmdBatchCursor == 0) return EFI_SUCCESS;

    UINT32 bytesToWrite = gpuRingBuffer.cmdBatchCursor;
    
    // Ensure space exists (may stall)
    EFI_STATUS Status = GpuRingBufferWaitSpace(bytesToWrite);
    if (EFI_ERROR(Status)) return Status;

    UINT32 spaceAtEnd = gpuRingBuffer.bufferEndAddr - gpuRingBuffer.ringHead;

    if (bytesToWrite <= spaceAtEnd) {
        // Linear write
        GpuVramWrite(gpuRingBuffer.ringHead, gpuRingBuffer.cmdBatchBufferPtr, bytesToWrite);
        gpuRingBuffer.ringHead += bytesToWrite;
    } else {
        // Wrap-around write
        GpuVramWrite(gpuRingBuffer.ringHead, gpuRingBuffer.cmdBatchBufferPtr, spaceAtEnd);
        
        UINT32 remainder = bytesToWrite - spaceAtEnd;
        GpuVramWrite(gpuRingBuffer.bufferStartAddr, gpuRingBuffer.cmdBatchBufferPtr + spaceAtEnd, remainder);
        
        gpuRingBuffer.ringHead = gpuRingBuffer.bufferStartAddr + remainder;
    }

    // Wrap logic for perfect alignment
    if (gpuRingBuffer.ringHead == gpuRingBuffer.bufferEndAddr) {
        gpuRingBuffer.ringHead = gpuRingBuffer.bufferStartAddr;
    }

    // Kick GPU
    GpuMmioWrite32(REG_RING_BUFFER_HEAD, gpuRingBuffer.ringHead);

    gpuRingBuffer.cmdBatchCursor = 0;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI GpuRingBufferClearCmdBuffer()
{
    gpuRingBuffer.cmdBatchCursor = 0;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI GpuRingBufferAddCmd(IN VOID *CmdData, IN UINT32 Size)
{
    if (gpuRingBuffer.cmdBatchCursor + Size > gpuRingBuffer.cmdBatchBufferSize) {
        GpuRingBufferFlush();
    }

    CopyMem(gpuRingBuffer.cmdBatchBufferPtr + gpuRingBuffer.cmdBatchCursor, CmdData, Size);
    gpuRingBuffer.cmdBatchCursor += Size;
    
    return EFI_SUCCESS;
}

BOOLEAN EFIAPI GpuRingBufferIsIdle()
{
    GpuReadRingTail();
    return (gpuRingBuffer.ringTail == gpuRingBuffer.ringHead);
}

VOID EFIAPI GpuRingBufferPrintState()
{
    DEBUG((EFI_D_INFO, "=== GPU RING BUFFER ===\n"));
    DEBUG((EFI_D_INFO, " Head: 0x%08X\n", gpuRingBuffer.ringHead));
    DEBUG((EFI_D_INFO, " Tail: 0x%08X\n", gpuRingBuffer.ringTail));
    DEBUG((EFI_D_INFO, " Free: %d bytes\n", GpuRingBufferGetFreeSpace()));
    DEBUG((EFI_D_INFO, " Batch: %d bytes\n", gpuRingBuffer.cmdBatchCursor));
}