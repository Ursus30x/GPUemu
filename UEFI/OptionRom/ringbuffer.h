#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include "oprom.h" 
// Assumes definitions for VRAMADDR, EFI_STATUS, etc.

struct GpuRingBuffer {
    // --- VRAM Ring Buffer State ---
    UINT32   bufferSize;        // Total size of the VRAM ring buffer
    VRAMADDR bufferStartAddr;   // GPU VRAM Offset (Base)
    VRAMADDR bufferEndAddr;     // Base + Size

    VRAMADDR ringHead;          // WRITE Pointer: Offset where CPU writes next
    VRAMADDR ringTail;          // READ Pointer: Last known GPU position
    
    // --- System RAM Staging Batch ---
    UINT8    *cmdBatchBufferPtr; // System RAM buffer for batching
    UINT32   cmdBatchBufferSize; // Total capacity of the batch
    UINT32   cmdBatchCursor;     // Current write position 
};

extern struct GpuRingBuffer gpuRingBuffer;

/*---------------- Ring Buffer Initialization ----------------*/

/**
 * Initializes the ring buffer structure and allocates system memory for the batch.
 */
EFI_STATUS EFIAPI GpuRingBufferInit(
    IN UINT32   RingSize
);

// Destroy ring buffer structure (frees allocated system memory)
EFI_STATUS EFIAPI GpuRingBufferDestroy();

/*---------------- Ring Buffer Command Management ----------------*/

// Flushes stored commands from System RAM batch to VRAM Ring Buffer.
// Uses GpuVramWrite() internally.
EFI_STATUS EFIAPI GpuRingBufferFlush();

// Resets the batch cursor to 0 (effectively clearing the staged commands).
EFI_STATUS EFIAPI GpuRingBufferClearCmdBuffer();

/**
 * Copies a raw command structure into the staging batch.
 * @param CmdData Pointer to the command struct/data.
 * @param Size    Size of the command in bytes.
 */
EFI_STATUS EFIAPI GpuRingBufferAddCmd(
    IN VOID   *CmdData,
    IN UINT32 Size
);

/*---------------- Synchronization & State ----------------*/

// Reads the actual Tail index from GPU MMIO and updates cachedTail.
// Implementation note: Must call a helper like GpuMmioRead32().
VRAMADDR EFIAPI GpuReadRingTail();

// Blocks CPU until 'bytesNeeded' is available in the VRAM Ring Buffer.
// Uses cachedTail first, then polls MMIO if necessary.
EFI_STATUS EFIAPI GpuRingBufferWaitSpace(IN UINT32 bytesNeeded);

// Returns TRUE if Ring Head == Ring Tail (GPU has finished all commands).
BOOLEAN EFIAPI GpuRingBufferIsIdle();

// Returns the amount of free space (in bytes) currently available in the VRAM Ring Buffer.
UINT32 EFIAPI GpuRingBufferGetFreeSpace();

/*---------------- Debugging ----------------*/

// Prints the current state (Head, Tail, CachedTail, Free Space) to Debug Output.
VOID EFIAPI GpuRingBufferPrintState();

#endif