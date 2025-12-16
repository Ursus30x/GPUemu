#ifndef MEMALLOC_H
#define MEMALLOC_H
#include "oprom.h"

#define PAGE_SIZE 32

typedef  UINT32 VRAMADDR;

struct MemoryAllocator{
    // Memory Allocation vars
    UINT32 pageCount;
    UINT32 totalMemSize;

    BOOLEAN *pageStatus;
    UINT32  *allocationSizes;

    VRAMADDR baseAddr;
    // PCI IO vars
    EFI_PCI_IO_PROTOCOL *PciIo; 
    UINT8  VramBarIndex;
} memAllocator;

/*---------------- Memory managment functions ----------------*/

// Initalizes memAllocator and maps out vram buffer
EFI_STATUS EFIAPI InitMemoryAllocator(IN EFI_PCI_IO_PROTOCOL *PciIo, IN UINT32 VRAMsize);

// Allocates a block of memory in gpu, returs address to it
EFI_STATUS EFIAPI AllocateMem(IN UINT32 bytesToAlloc, OUT VRAMADDR addr);

// Allocates a block of memory in gpu AT given address (marks a region as non-usable for allocator)
// Used for static memory areas such as main frame buffer
EFI_STATUS EFIAPI AllocateMemAt(IN UINT32 bytesToAlloc, IN VRAMADDR addr);

// Frees a block of memory at given address
EFI_STATUS EFIAPI FreeMem(IN VRAMADDR addr);

/*---------------- Read/Write helper functions ----------------*/

// Copies data from host to VRAM 
EFI_STATUS EFIAPI VramWrite(IN VRAMADDR destAddr, IN VOID* sourcePtr, IN UINT32 size);

// Copies data from VRAM to host
EFI_STATUS EFIAPI VramRead(IN VOID* destAddr, IN VRAMADDR sourcePtr, IN UINT32 size);

// Clears/Sets given memory area with specified value 
EFI_STATUS EFIAPI VramSet(IN VRAMADDR DestAddr, IN UINT8 Value, IN UINT32 Size);



#endif