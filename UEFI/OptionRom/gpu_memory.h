#include "oprom.h"

#ifndef GPUMEMORY_H
#define GPUMEMORY_H

#define PAGE_SIZE 64
#define MEM_DEBUG

typedef UINT32 VRAMADDR;

struct GpuMemoryAllocator {
    // Memory Allocation vars
    UINT32 pageCount;
    UINT32 totalMemSize;

    BOOLEAN *pageStatus;
    UINT32  *allocationSizes;

#ifdef MEM_DEBUG
    // Only present in DEBUG builds to store tag strings
    CHAR8 **allocationTags;
#endif

    VRAMADDR baseAddr;
    
    // PCI IO vars
    EFI_PCI_IO_PROTOCOL *PciIo; 
    UINT8  VramBarIndex;
};

// Declare the global instance
extern struct GpuMemoryAllocator gpuMemAllocator;


/*---------------- Memory Management Macros & Declarations ----------------*/

// Initalizes gpuMemAllocator and maps out vram buffer
EFI_STATUS EFIAPI GpuInitMemoryAllocator(IN EFI_PCI_IO_PROTOCOL *PciIo, IN UINT32 VRAMsize, IN VRAMADDR baseAddr);

#ifdef MEM_DEBUG
    // DEBUG VERSION: Passes 'Tag' to implementation
    #define GpuAllocateMem(Size, Tag)         GpuAllocateMemImpl(Size, Tag)
    #define GpuAllocateMemAt(Size, Addr, Tag) GpuAllocateMemAtImpl(Size, Addr, Tag)

    VRAMADDR GpuAllocateMemImpl(IN UINT32 bytesToAlloc, IN CHAR8 *Tag);
    BOOLEAN  GpuAllocateMemAtImpl(IN UINT32 bytesToAlloc, IN VRAMADDR addr, IN CHAR8 *Tag);
#else
    // RELEASE VERSION: Removes 'Tag' argument
    #define GpuAllocateMem(Size, Tag)         GpuAllocateMemImpl(Size)
    #define GpuAllocateMemAt(Size, Addr, Tag) GpuAllocateMemAtImpl(Size, Addr)

    VRAMADDR GpuAllocateMemImpl(IN UINT32 bytesToAlloc);
    BOOLEAN  GpuAllocateMemAtImpl(IN UINT32 bytesToAlloc, IN VRAMADDR addr);
#endif

// Frees a block of memory at given address
BOOLEAN GpuFreeMem(IN VRAMADDR addr);


/*---------------- Read/Write Helper Functions ----------------*/

// Copies data from host to VRAM 
EFI_STATUS EFIAPI GpuVramWrite(IN VRAMADDR destAddr, IN VOID* sourcePtr, IN UINT32 size);

// Copies data from VRAM to host
EFI_STATUS EFIAPI GpuVramRead(IN VOID* destAddr, IN VRAMADDR sourcePtr, IN UINT32 size);

// Clears/Sets given memory area with specified value 
EFI_STATUS EFIAPI GpuVramSet(IN VRAMADDR DestAddr, IN UINT8 Value, IN UINT32 Size);


/*---------------- Debug Functions ----------------*/

// Prints total usage, free space, and fragmentation info
VOID EFIAPI GpuDebugPrintAllocatorStats(VOID);

// Prints a detailed map of continuous memory blocks (Used vs Free ranges)
VOID EFIAPI GpuDebugDumpMemoryMap(VOID);

#endif