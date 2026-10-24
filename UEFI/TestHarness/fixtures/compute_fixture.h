#ifndef __COMPUTE_FIXTURE_H__
#define __COMPUTE_FIXTURE_H__

#include "common_fixture.h"

// ============================================================================
// Default Compute Shader Bytecode (Vector Addition: c[i] = a[i] + b[i])
// ============================================================================

STATIC CONST UINT32 gDefaultComputeShaderCode[] = {
    0x00000550, // Size: 1360 bytes
    0x07230203, 0x00010000, 0x000D000B, 0x0000002C, 0x00000000, 0x00020011, 
    0x00000001, 0x0006000B, 0x00000001, 0x4C534C47, 0x6474732E, 0x3035342E, 
    0x00000000, 0x0003000E, 0x00000000, 0x00000001, 0x0006000F, 0x00000005, 
    0x00000004, 0x6E69616D, 0x00000000, 0x0000000B, 0x00060010, 0x00000004, 
    0x00000011, 0x00000010, 0x00000001, 0x00000001, 0x00030003, 0x00000002, 
    0x000001C2, 0x000A0004, 0x475F4C47, 0x4C474F4F, 0x70635F45, 0x74735F70, 
    0x5F656C79, 0x656E696C, 0x7269645F, 0x69746365, 0x00006576, 0x00080004, 
    0x475F4C47, 0x4C474F4F, 0x6E695F45, 0x64756C63, 0x69645F65, 0x74636572, 
    0x00657669, 0x00040005, 0x00000004, 0x6E69616D, 0x00000000, 0x00030005, 
    0x00000008, 0x00786469, 0x00080005, 0x0000000B, 0x475F6C67, 0x61626F6C, 
    0x766E496C, 0x7461636F, 0x496E6F69, 0x00000044, 0x00040005, 0x00000012, 
    0x7074754F, 0x00437475, 0x00040006, 0x00000012, 0x00000000, 0x00000063, 
    0x00030005, 0x00000014, 0x00000000, 0x00040005, 0x00000019, 0x75706E49, 
    0x00004174, 0x00040006, 0x00000019, 0x00000000, 0x00000061, 0x00030005, 
    0x0000001B, 0x00000000, 0x00040005, 0x00000021, 0x75706E49, 0x00004274, 
    0x00040006, 0x00000021, 0x00000000, 0x00000062, 0x00030005, 0x00000023, 
    0x00000000, 0x00040047, 0x0000000B, 0x0000000B, 0x0000001C, 0x00040047, 
    0x00000011, 0x00000006, 0x00000004, 0x00030047, 0x00000012, 0x00000003, 
    0x00050048, 0x00000012, 0x00000000, 0x00000023, 0x00000000, 0x00040047, 
    0x00000014, 0x00000021, 0x00000002, 0x00040047, 0x00000014, 0x00000022, 
    0x00000000, 0x00040047, 0x00000018, 0x00000006, 0x00000004, 0x00030047, 
    0x00000019, 0x00000003, 0x00050048, 0x00000019, 0x00000000, 0x00000023, 
    0x00000000, 0x00040047, 0x0000001B, 0x00000021, 0x00000000, 0x00040047, 
    0x0000001B, 0x00000022, 0x00000000, 0x00040047, 0x00000020, 0x00000006, 
    0x00000004, 0x00030047, 0x00000021, 0x00000003, 0x00050048, 0x00000021, 
    0x00000000, 0x00000023, 0x00000000, 0x00040047, 0x00000023, 0x00000021, 
    0x00000001, 0x00040047, 0x00000023, 0x00000022, 0x00000000, 0x00040047, 
    0x0000002B, 0x0000000B, 0x00000019, 0x00020013, 0x00000002, 0x00030021, 
    0x00000003, 0x00000002, 0x00040015, 0x00000006, 0x00000020, 0x00000000, 
    0x00040020, 0x00000007, 0x00000007, 0x00000006, 0x00040017, 0x00000009, 
    0x00000006, 0x00000003, 0x00040020, 0x0000000A, 0x00000001, 0x00000009, 
    0x0004003B, 0x0000000A, 0x0000000B, 0x00000001, 0x0004002B, 0x00000006, 
    0x0000000C, 0x00000000, 0x00040020, 0x0000000D, 0x00000001, 0x00000006, 
    0x00030016, 0x00000010, 0x00000020, 0x0003001D, 0x00000011, 0x00000010, 
    0x0003001E, 0x00000012, 0x00000011, 0x00040020, 0x00000013, 0x00000002, 
    0x00000012, 0x0004003B, 0x00000013, 0x00000014, 0x00000002, 0x00040015, 
    0x00000015, 0x00000020, 0x00000001, 0x0004002B, 0x00000015, 0x00000016, 
    0x00000000, 0x0003001D, 0x00000018, 0x00000010, 0x0003001E, 0x00000019, 
    0x00000018, 0x00040020, 0x0000001A, 0x00000002, 0x00000019, 0x0004003B, 
    0x0000001A, 0x0000001B, 0x00000002, 0x00040020, 0x0000001D, 0x00000002, 
    0x00000010, 0x0003001D, 0x00000020, 0x00000010, 0x0003001E, 0x00000021, 
    0x00000020, 0x00040020, 0x00000022, 0x00000002, 0x00000021, 0x0004003B, 
    0x00000022, 0x00000023, 0x00000002, 0x0004002B, 0x00000006, 0x00000029, 
    0x00000010, 0x0004002B, 0x00000006, 0x0000002A, 0x00000001, 0x0006002C, 
    0x00000009, 0x0000002B, 0x00000029, 0x0000002A, 0x0000002A, 0x00050036, 
    0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200F8, 0x00000005, 
    0x0004003B, 0x00000007, 0x00000008, 0x00000007, 0x00050041, 0x0000000D, 
    0x0000000E, 0x0000000B, 0x0000000C, 0x0004003D, 0x00000006, 0x0000000F, 
    0x0000000E, 0x0003003E, 0x00000008, 0x0000000F, 0x0004003D, 0x00000006, 
    0x00000017, 0x00000008, 0x0004003D, 0x00000006, 0x0000001C, 0x00000008, 
    0x00060041, 0x0000001D, 0x0000001E, 0x0000001B, 0x00000016, 0x0000001C, 
    0x0004003D, 0x00000010, 0x0000001F, 0x0000001E, 0x0004003D, 0x00000006, 
    0x00000024, 0x00000008, 0x00060041, 0x0000001D, 0x00000025, 0x00000023, 
    0x00000016, 0x00000024, 0x0004003D, 0x00000010, 0x00000026, 0x00000025, 
    0x00050081, 0x00000010, 0x00000027, 0x0000001F, 0x00000026, 0x00060041, 
    0x0000001D, 0x00000028, 0x00000014, 0x00000016, 0x00000017, 0x0003003E, 
    0x00000028, 0x00000027, 0x000100FD, 0x00010038
};

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
