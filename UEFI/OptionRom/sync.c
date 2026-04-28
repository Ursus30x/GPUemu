#include "sync.h"
#include "gpu_memory.h"
#include "ringbuffer.h"
#include "gpu_hw.h"
#include <Library/UefiBootServicesTableLib.h> // gBS
#include <Library/DebugLib.h>

EFI_STATUS EFIAPI GpuDmaSync(VOID)
{
    if (gpuMemAllocator.Fence == NULL || !gpuMemAllocator.Fence->DmaBusy) {
        return EFI_SUCCESS;
    }

    // Check if GPU is finished with DMA
    while (!(GpuMmioRead32(REG_INT_STATUS_ADDR) & GPU_INT_DMA_DONE)) {
        gBS->Stall(10);
    }

    // ACK interrupt
    GpuMmioWrite32(REG_INT_ACK_ADDR, GPU_INT_DMA_DONE);

    // Cleanup mapping
    gpuMemAllocator.PciIo->Unmap(gpuMemAllocator.PciIo, gpuMemAllocator.Fence->MapPtr);

    gpuMemAllocator.Fence->DmaBusy = FALSE;
    gpuMemAllocator.Fence->MapPtr = NULL;

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI GpuCmdSync(VOID)
{
    if (gpuMemAllocator.Fence == NULL || !gpuMemAllocator.Fence->CmdBusy) {
        return EFI_SUCCESS;
    }

    // Check if GPU is finished drawing
    while (!(GpuMmioRead32(REG_INT_STATUS_ADDR) & GPU_INT_CMD_DONE)) {
        if (GpuRingBufferIsIdle()) break;
        gBS->Stall(10);
    }

    // ACK interrupt
    GpuMmioWrite32(REG_INT_ACK_ADDR, GPU_INT_CMD_DONE);

    gpuMemAllocator.Fence->CmdBusy = FALSE;

    return EFI_SUCCESS;
}
