#ifndef __MESH_FIXTURE_H__
#define __MESH_FIXTURE_H__

#include "common_fixture.h"
#include "../res/shaders/meshlet_cull_ts.h"
#include "../res/shaders/meshlet_gen_ms.h"
#include "../res/shaders/mesh_only_ms.h"
#include "../res/shaders/mesh_points_ms.h"
#include "../res/shaders/mesh_lines_ms.h"
#include "../res/shaders/mesh_per_prim_ms.h"
#include "../res/shaders/mesh_barrier_ts.h"
#include "../res/shaders/mesh_barrier_ms.h"
#include "../res/shaders/raster_default_fs.h"

#define MESH_MAX_SSBO_SLOTS 8

typedef struct {
    float  bounding_sphere[4];
    float  cone[4];
    UINT32 vertex_offset;
    UINT32 vertex_count;
    UINT32 primitive_offset;
    UINT32 primitive_count;
} MESH_TEST_MESHLET;

typedef struct _MESH_FIXTURE {
    GOP_3D_PROTOCOL *Gop3D;

    VRAMADDR hTS;
    UINT32   TsSize;
    VRAMADDR hMS;
    UINT32   MsSize;
    VRAMADDR hFS;
    UINT32   FsSize;

    VRAMADDR hBuffers[MESH_MAX_SSBO_SLOTS];
    UINT32   BufferSizes[MESH_MAX_SSBO_SLOTS];
    UINT32   ClearColor;

    EFI_STATUS (*SetTaskShader)(struct _MESH_FIXTURE *Fix, CONST VOID *ShaderCode, UINT32 CodeSize);
    EFI_STATUS (*SetMeshShader)(struct _MESH_FIXTURE *Fix, CONST VOID *ShaderCode, UINT32 CodeSize);
    EFI_STATUS (*SetFragShader)(struct _MESH_FIXTURE *Fix, CONST VOID *ShaderCode, UINT32 CodeSize);
    EFI_STATUS (*SetBuffer)(struct _MESH_FIXTURE *Fix, UINT32 Slot, CONST VOID *Data, UINT32 Size);
    EFI_STATUS (*DrawMeshTasks)(struct _MESH_FIXTURE *Fix, UINT32 Gx, UINT32 Gy, UINT32 Gz);
    EFI_STATUS (*DrawMeshTasksIndirect)(struct _MESH_FIXTURE *Fix, UINT32 IndirectSlot, UINT32 ByteOffset);
    UINT32     (*ReadPixel)(struct _MESH_FIXTURE *Fix, UINT32 X, UINT32 Y);
} MESH_FIXTURE;

STATIC inline EFI_STATUS MeshFixtureSetTaskShader(MESH_FIXTURE *Fix, CONST VOID *ShaderCode, UINT32 CodeSize) {
    if (Fix == NULL || Fix->Gop3D == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    GOP_3D_PROTOCOL *Gop3D = Fix->Gop3D;

    if (Fix->hTS != GPU_NULL_ADDR) {
        Gop3D->GpuFreeBuffer(Gop3D, &Fix->hTS);
        Fix->hTS = GPU_NULL_ADDR;
        Fix->TsSize = 0;
    }

    if (ShaderCode == NULL || CodeSize == 0) {
        return EFI_SUCCESS;
    }

    Gop3D->GpuCmdBegin(Gop3D);
    EFI_STATUS Status = Gop3D->GpuCmdTransferBuffer(Gop3D, Gop3dBufferTypeShaderCode, (VOID*)ShaderCode, CodeSize, &Fix->hTS);
    Gop3D->GpuCmdEnd(Gop3D);
    Gop3D->GpuSubmitCmd(Gop3D);

    Fix->TsSize = CodeSize;
    return Status;
}

STATIC inline EFI_STATUS MeshFixtureSetMeshShader(MESH_FIXTURE *Fix, CONST VOID *ShaderCode, UINT32 CodeSize) {
    if (Fix == NULL || Fix->Gop3D == NULL || ShaderCode == NULL || CodeSize == 0) {
        return EFI_INVALID_PARAMETER;
    }

    GOP_3D_PROTOCOL *Gop3D = Fix->Gop3D;

    if (Fix->hMS != GPU_NULL_ADDR) {
        Gop3D->GpuFreeBuffer(Gop3D, &Fix->hMS);
        Fix->hMS = GPU_NULL_ADDR;
    }

    Gop3D->GpuCmdBegin(Gop3D);
    EFI_STATUS Status = Gop3D->GpuCmdTransferBuffer(Gop3D, Gop3dBufferTypeShaderCode, (VOID*)ShaderCode, CodeSize, &Fix->hMS);
    Gop3D->GpuCmdEnd(Gop3D);
    Gop3D->GpuSubmitCmd(Gop3D);

    Fix->MsSize = CodeSize;
    return Status;
}

STATIC inline EFI_STATUS MeshFixtureSetFragShader(MESH_FIXTURE *Fix, CONST VOID *ShaderCode, UINT32 CodeSize) {
    if (Fix == NULL || Fix->Gop3D == NULL || ShaderCode == NULL || CodeSize == 0) {
        return EFI_INVALID_PARAMETER;
    }

    GOP_3D_PROTOCOL *Gop3D = Fix->Gop3D;

    if (Fix->hFS != GPU_NULL_ADDR) {
        Gop3D->GpuFreeBuffer(Gop3D, &Fix->hFS);
        Fix->hFS = GPU_NULL_ADDR;
    }

    Gop3D->GpuCmdBegin(Gop3D);
    EFI_STATUS Status = Gop3D->GpuCmdTransferBuffer(Gop3D, Gop3dBufferTypeShaderCode, (VOID*)ShaderCode, CodeSize, &Fix->hFS);
    Gop3D->GpuCmdEnd(Gop3D);
    Gop3D->GpuSubmitCmd(Gop3D);

    Fix->FsSize = CodeSize;
    return Status;
}

STATIC inline EFI_STATUS MeshFixtureSetBuffer(MESH_FIXTURE *Fix, UINT32 Slot, CONST VOID *Data, UINT32 Size) {
    if (Fix == NULL || Fix->Gop3D == NULL || Slot >= MESH_MAX_SSBO_SLOTS || Data == NULL || Size == 0) {
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

STATIC inline EFI_STATUS MeshFixtureDrawMeshTasks(MESH_FIXTURE *Fix, UINT32 Gx, UINT32 Gy, UINT32 Gz) {
    if (Fix == NULL || Fix->Gop3D == NULL || Fix->hMS == GPU_NULL_ADDR || Fix->hFS == GPU_NULL_ADDR) {
        return EFI_INVALID_PARAMETER;
    }

    GOP_3D_PROTOCOL *Gop3D = Fix->Gop3D;

    Gop3D->GpuCmdBegin(Gop3D);
    Gop3D->GpuCmdClearFrame(Gop3D, Fix->ClearColor);
    Gop3D->GpuCmdSetBlendState(Gop3D, FALSE, Gop3dBlendFactorOne, Gop3dBlendFactorZero);
    Gop3D->GpuCmdSetDepthWrite(Gop3D, TRUE);

    Gop3D->GpuCmdBindTaskShader(Gop3D, Fix->hTS, Fix->TsSize);
    Gop3D->GpuCmdBindMeshShader(Gop3D, Fix->hMS, Fix->MsSize);
    Gop3D->GpuCmdBindFragShader(Gop3D, Fix->hFS, Fix->FsSize);

    for (UINT32 Slot = 0; Slot < MESH_MAX_SSBO_SLOTS; Slot++) {
        if (Fix->hBuffers[Slot] != GPU_NULL_ADDR) {
            Gop3D->GpuCmdBindSSBO(Gop3D, Slot, Fix->hBuffers[Slot], Fix->BufferSizes[Slot]);
        }
    }

    Gop3D->GpuCmdDrawMeshTasks(Gop3D, Gx, Gy, Gz);
    Gop3D->GpuCmdEnd(Gop3D);

    return Gop3D->GpuPresent(Gop3D);
}

STATIC inline EFI_STATUS MeshFixtureDrawMeshTasksIndirect(MESH_FIXTURE *Fix, UINT32 IndirectSlot, UINT32 ByteOffset) {
    if (Fix == NULL || Fix->Gop3D == NULL || Fix->hMS == GPU_NULL_ADDR || Fix->hFS == GPU_NULL_ADDR) {
        return EFI_INVALID_PARAMETER;
    }
    if (IndirectSlot >= MESH_MAX_SSBO_SLOTS || Fix->hBuffers[IndirectSlot] == GPU_NULL_ADDR) {
        return EFI_INVALID_PARAMETER;
    }

    GOP_3D_PROTOCOL *Gop3D = Fix->Gop3D;

    Gop3D->GpuCmdBegin(Gop3D);
    Gop3D->GpuCmdClearFrame(Gop3D, Fix->ClearColor);
    Gop3D->GpuCmdSetBlendState(Gop3D, FALSE, Gop3dBlendFactorOne, Gop3dBlendFactorZero);
    Gop3D->GpuCmdSetDepthWrite(Gop3D, TRUE);

    Gop3D->GpuCmdBindTaskShader(Gop3D, Fix->hTS, Fix->TsSize);
    Gop3D->GpuCmdBindMeshShader(Gop3D, Fix->hMS, Fix->MsSize);
    Gop3D->GpuCmdBindFragShader(Gop3D, Fix->hFS, Fix->FsSize);

    for (UINT32 Slot = 0; Slot < MESH_MAX_SSBO_SLOTS; Slot++) {
        if (Fix->hBuffers[Slot] != GPU_NULL_ADDR) {
            Gop3D->GpuCmdBindSSBO(Gop3D, Slot, Fix->hBuffers[Slot], Fix->BufferSizes[Slot]);
        }
    }

    Gop3D->GpuCmdDrawMeshTasksIndirect(Gop3D, Fix->hBuffers[IndirectSlot] + ByteOffset);
    Gop3D->GpuCmdEnd(Gop3D);

    return Gop3D->GpuPresent(Gop3D);
}

STATIC inline UINT32 MeshFixtureReadPixel(MESH_FIXTURE *Fix, UINT32 X, UINT32 Y) {
    if (Fix == NULL || Fix->Gop3D == NULL) return 0;
    if (X >= 640) X = 639;
    if (Y >= 480) Y = 479;

    UINT32 Pixel = 0;
    VRAMADDR PixelOffset = (Y * 640 + X) * sizeof(UINT32);
    if (PixelOffset == 0) {
        PixelOffset = sizeof(UINT32);
    }

    Fix->Gop3D->GpuCmdReadBuffer(Fix->Gop3D, PixelOffset, &Pixel, sizeof(UINT32));
    return Pixel;
}

STATIC inline BOOLEAN HarnessInitMeshFixture(IN GOP_3D_PROTOCOL *Gop3D, OUT MESH_FIXTURE *Fix) {
    if (Gop3D == NULL || Fix == NULL) return FALSE;

    ZeroMem(Fix, sizeof(MESH_FIXTURE));
    Fix->Gop3D = Gop3D;
    Fix->ClearColor = 0xFF000000;

    Fix->SetTaskShader = MeshFixtureSetTaskShader;
    Fix->SetMeshShader = MeshFixtureSetMeshShader;
    Fix->SetFragShader = MeshFixtureSetFragShader;
    Fix->SetBuffer             = MeshFixtureSetBuffer;
    Fix->DrawMeshTasks         = MeshFixtureDrawMeshTasks;
    Fix->DrawMeshTasksIndirect = MeshFixtureDrawMeshTasksIndirect;
    Fix->ReadPixel             = MeshFixtureReadPixel;

    Gop3D->GpuSetMode(Gop3D, 1);

    MeshFixtureSetTaskShader(Fix, gDefaultTaskShaderCode, sizeof(gDefaultTaskShaderCode));
    MeshFixtureSetMeshShader(Fix, gDefaultMeshShaderCode, sizeof(gDefaultMeshShaderCode));
    MeshFixtureSetFragShader(Fix, gDefaultRasterFsCode, sizeof(gDefaultRasterFsCode));

    return TRUE;
}

STATIC inline VOID HarnessDestroyMeshFixture(IN MESH_FIXTURE *Fix) {
    if (Fix == NULL || Fix->Gop3D == NULL) return;

    GOP_3D_PROTOCOL *Gop3D = Fix->Gop3D;

    for (UINT32 Slot = 0; Slot < MESH_MAX_SSBO_SLOTS; Slot++) {
        if (Fix->hBuffers[Slot] != GPU_NULL_ADDR) {
            Gop3D->GpuFreeBuffer(Gop3D, &Fix->hBuffers[Slot]);
        }
    }

    if (Fix->hTS != GPU_NULL_ADDR) Gop3D->GpuFreeBuffer(Gop3D, &Fix->hTS);
    if (Fix->hMS != GPU_NULL_ADDR) Gop3D->GpuFreeBuffer(Gop3D, &Fix->hMS);
    if (Fix->hFS != GPU_NULL_ADDR) Gop3D->GpuFreeBuffer(Gop3D, &Fix->hFS);

    Gop3D->GpuSetMode(Gop3D, 0);
}

#endif // __MESH_FIXTURE_H__
