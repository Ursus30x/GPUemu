#include "memAlloc.h"


BOOLEAN CanAlloc(
    IN UINT32 startPage, 
    IN UINT32 pagesCount){

    if (startPage + pagesCount > memAllocator.pageCount) {
        return FALSE;
    }

    // Iterate thorugh pages to check if pages are free
    for (UINT32 i = 0; i < pagesCount; i++) {
        if (memAllocator.pageStatus[startPage + i] == TRUE) {
            return FALSE;
        }
    }

    return TRUE;
}

VOID SetStatusPage(IN CONST UINT32 FromPage,
    IN CONST UINT32 ToPage, 
    IN CONST BOOLEAN value){

    for(UINT32 i = FromPage; i < ToPage; i++){
        memAllocator.pageStatus[i] = value;
    }
}

/* ----------------------------- Exposed functions ----------------------------- */

EFI_STATUS EFIAPI InitMemoryAllocator(
    IN EFI_PCI_IO_PROTOCOL *PciIo, 
    IN UINT32 VRAMsize, 
    IN VRAMADDR baseAddr){
    // Basic size info 
    memAllocator.pageCount      = VRAMsize/PAGE_SIZE;
    memAllocator.totalMemSize   = VRAMsize;

    // Allocate aloccation tracker arrays and zero them out
    memAllocator.pageStatus         = AllocateZeroPool(memAllocator.pageCount * sizeof(BOOLEAN));
    memAllocator.allocationSizes    = AllocateZeroPool(memAllocator.pageCount * sizeof(UINT32));

    if(!memAllocator.pageStatus || !memAllocator.allocationSizes) return EFI_OUT_OF_RESOURCES; 

    memAllocator.baseAddr = baseAddr;

    // PCI IO init
    memAllocator.PciIo          = PciIo;
    memAllocator.VramBarIndex   = 1; // Hardcode bar index

    return EFI_SUCCESS;
}

VRAMADDR AllocateMem(IN UINT32 bytesToAlloc){

    UINT32 pageCounter = 0;
    UINT32 pagesToAlloc = (bytesToAlloc + PAGE_SIZE - 1) / PAGE_SIZE;

    while(pageCounter < memAllocator.pageCount){
        if(memAllocator.pageStatus[pageCounter] == FALSE){
            if(CanAlloc(pageCounter,pagesToAlloc)){
                SetStatusPage(pageCounter, pageCounter+pagesToAlloc, TRUE);
                memAllocator.allocationSizes[pageCounter] = pagesToAlloc;

                return pageCounter * PAGE_SIZE;
            }
        }
        else if(memAllocator.allocationSizes[pageCounter] != 0){
            pageCounter += memAllocator.allocationSizes[pageCounter];
            continue;
        }

        pageCounter++;
    }

    return 0xFFFFFFFF;
}

BOOLEAN AllocateMemAt(IN UINT32 bytesToAlloc, IN VRAMADDR addr){
    if (addr % PAGE_SIZE != 0) return FALSE;

    UINT32 page = addr / PAGE_SIZE;
    UINT32 pagesToAlloc = (bytesToAlloc + PAGE_SIZE - 1) / PAGE_SIZE;


    if(CanAlloc(page, pagesToAlloc)){
        SetStatusPage(page, page+pagesToAlloc, TRUE);
        memAllocator.allocationSizes[page] = pagesToAlloc;

        return TRUE;
    }
    
    return FALSE;
}

BOOLEAN FreeMem(IN VRAMADDR addr){
    if (addr % PAGE_SIZE != 0) return FALSE;

    UINT32 page = addr / PAGE_SIZE;

    if(memAllocator.allocationSizes[page] == 0){
        return FALSE;
    }

    SetStatusPage(page, page + memAllocator.allocationSizes[page], FALSE);
    memAllocator.allocationSizes[page] = 0;

    return TRUE;
}

