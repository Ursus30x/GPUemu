#ifndef GPU_SYNC_H
#define GPU_SYNC_H

#include "oprom.h"

// Wait for the current DMA transfer to finish
EFI_STATUS EFIAPI GpuDmaSync(VOID);

// Wait for the current Command Batch to finish rendering
EFI_STATUS EFIAPI GpuCmdSync(VOID);

#endif
