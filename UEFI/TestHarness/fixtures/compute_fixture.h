#ifndef __COMPUTE_FIXTURE_H__
#define __COMPUTE_FIXTURE_H__

#include "common_fixture.h"

// ============================================================================
// Default Compute Shader (Loaded from res/shaders/)
// ============================================================================

#include "../res/shaders/compute_default_cs.h"

// ============================================================================
// COMPUTE_FIXTURE Definition & Operations (Analogous to SpirvApp)
// ============================================================================

#define COMPUTE_MAX_SSBO_SLOTS 8

typedef struct _COMPUTE_FIXTURE {
    GOP_3D_PROTOCOL *Gop3D;

    VRAMADDR hCS;
    UINT32   CsSize;

    VRAMADDR hBuffers[COMPUTE_MAX_SSBO_SLOTS];
    UINT32   BufferSizes[COMPUTE_MAX_SSBO_SLOTS];

    EFI_STATUS (*SetShader)(struct _COMPUTE_FIXTURE *Fix, CONST VOID *ShaderCode, UINT32 CodeSize);
    EFI_STATUS (*SetBuffer)(struct _COMPUTE_FIXTURE *Fix, UINT32 Slot, CONST VOID *Data, UINT32 Size);
    EFI_STATUS (*Dispatch)(struct _COMPUTE_FIXTURE *Fix, UINT32 Gx, UINT32 Gy, UINT32 Gz);
    EFI_STATUS (*ReadBuffer)(struct _COMPUTE_FIXTURE *Fix, UINT32 Slot, VOID *HostData, UINT32 Size);
} COMPUTE_FIXTURE;

STATIC inline EFI_STATUS ComputeFixtureSetShader(COMPUTE_FIXTURE *Fix, CONST VOID *ShaderCode, UINT32 CodeSize) {
    if (Fix == NULL || Fix->Gop3D == NULL || ShaderCode == NULL || CodeSize == 0) {
        return EFI_INVALID_PARAMETER;
    }

    GOP_3D_PROTOCOL *Gop3D = Fix->Gop3D;

    // Free previous shader if uploaded
    if (Fix->hCS != GPU_NULL_ADDR) {
        Gop3D->GpuFreeBuffer(Gop3D, &Fix->hCS);
        Fix->hCS = GPU_NULL_ADDR;
    }

    Gop3D->GpuCmdBegin(Gop3D);
    EFI_STATUS Status = Gop3D->GpuCmdTransferBuffer(Gop3D, Gop3dBufferTypeShaderCode, (VOID*)ShaderCode, CodeSize, &Fix->hCS);
    Gop3D->GpuCmdEnd(Gop3D);
    Gop3D->GpuSubmitCmd(Gop3D);

    Fix->CsSize = CodeSize;
    return Status;
}

STATIC inline EFI_STATUS ComputeFixtureSetBuffer(COMPUTE_FIXTURE *Fix, UINT32 Slot, CONST VOID *Data, UINT32 Size) {
    if (Fix == NULL || Fix->Gop3D == NULL || Slot >= COMPUTE_MAX_SSBO_SLOTS || Data == NULL || Size == 0) {
        return EFI_INVALID_PARAMETER;
    }

    GOP_3D_PROTOCOL *Gop3D = Fix->Gop3D;
    EFI_STATUS Status;

    Gop3D->GpuCmdBegin(Gop3D);
    if (Fix->hBuffers[Slot] == GPU_NULL_ADDR) {
        Status = Gop3D->GpuCmdTransferBuffer(Gop3D, Gop3dBufferTypeSSBO, (VOID*)Data, Size, &Fix->hBuffers[Slot]);
    } else {
        Status = Gop3D->GpuCmdUpdateBuffer(Gop3D, Gop3dBufferTypeSSBO, (VOID*)Data, Size, &Fix->hBuffers[Slot]);
    }
    Gop3D->GpuCmdEnd(Gop3D);
    Gop3D->GpuSubmitCmd(Gop3D);

    if (!EFI_ERROR(Status)) {
        Fix->BufferSizes[Slot] = Size;
    }
    return Status;
}

STATIC inline EFI_STATUS ComputeFixtureDispatch(COMPUTE_FIXTURE *Fix, UINT32 Gx, UINT32 Gy, UINT32 Gz) {
    if (Fix == NULL || Fix->Gop3D == NULL || Fix->hCS == GPU_NULL_ADDR) {
        return EFI_INVALID_PARAMETER;
    }

    GOP_3D_PROTOCOL *Gop3D = Fix->Gop3D;

    Gop3D->GpuCmdBegin(Gop3D);
    Gop3D->GpuCmdBindCompShader(Gop3D, Fix->hCS, Fix->CsSize);

    // Bind all active SSBO buffers
    for (UINT32 Slot = 0; Slot < COMPUTE_MAX_SSBO_SLOTS; Slot++) {
        if (Fix->hBuffers[Slot] != GPU_NULL_ADDR) {
            Gop3D->GpuCmdBindSSBO(Gop3D, Slot, Fix->hBuffers[Slot], Fix->BufferSizes[Slot]);
        }
    }

    Gop3D->GpuCmdDispatchCompute(Gop3D, (Gx > 0) ? Gx : 1, (Gy > 0) ? Gy : 1, (Gz > 0) ? Gz : 1);
    Gop3D->GpuCmdEnd(Gop3D);

    // Present flushes command execution and waits for completion
    return Gop3D->GpuPresent(Gop3D);
}

STATIC inline EFI_STATUS ComputeFixtureReadBuffer(COMPUTE_FIXTURE *Fix, UINT32 Slot, VOID *HostData, UINT32 Size) {
    if (Fix == NULL || Fix->Gop3D == NULL || Slot >= COMPUTE_MAX_SSBO_SLOTS ||
        Fix->hBuffers[Slot] == GPU_NULL_ADDR || HostData == NULL || Size == 0) {
        return EFI_INVALID_PARAMETER;
    }

    return Fix->Gop3D->GpuCmdReadBuffer(Fix->Gop3D, Fix->hBuffers[Slot], HostData, Size);
}

STATIC inline BOOLEAN HarnessInitComputeFixture(IN GOP_3D_PROTOCOL *Gop3D, OUT COMPUTE_FIXTURE *Fix) {
    if (Gop3D == NULL || Fix == NULL) return FALSE;

    ZeroMem(Fix, sizeof(COMPUTE_FIXTURE));
    Fix->Gop3D = Gop3D;

    // Attach function pointers
    Fix->SetShader  = ComputeFixtureSetShader;
    Fix->SetBuffer  = ComputeFixtureSetBuffer;
    Fix->Dispatch   = ComputeFixtureDispatch;
    Fix->ReadBuffer = ComputeFixtureReadBuffer;

    // Switch GPU to 3D/Compute mode
    Gop3D->GpuSetMode(Gop3D, 1);

    // Upload default vector addition compute shader
    ComputeFixtureSetShader(Fix, gDefaultComputeShaderCode, sizeof(gDefaultComputeShaderCode));

    return TRUE;
}

STATIC inline VOID HarnessDestroyComputeFixture(IN COMPUTE_FIXTURE *Fix) {
    if (Fix == NULL || Fix->Gop3D == NULL) return;

    GOP_3D_PROTOCOL *Gop3D = Fix->Gop3D;

    // Free all allocated SSBO buffers
    for (UINT32 Slot = 0; Slot < COMPUTE_MAX_SSBO_SLOTS; Slot++) {
        if (Fix->hBuffers[Slot] != GPU_NULL_ADDR) {
            Gop3D->GpuFreeBuffer(Gop3D, &Fix->hBuffers[Slot]);
        }
    }

    // Free compute shader
    if (Fix->hCS != GPU_NULL_ADDR) {
        Gop3D->GpuFreeBuffer(Gop3D, &Fix->hCS);
    }

    // Return to GOP mode
    Gop3D->GpuSetMode(Gop3D, 0);
}

#endif // __COMPUTE_FIXTURE_H__
