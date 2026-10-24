#ifndef __RASTER_FIXTURE_H__
#define __RASTER_FIXTURE_H__

#include "common_fixture.h"

// ============================================================================
// Math & Vertex Definitions (Analogous to SpirvApp)
// ============================================================================

typedef struct {
    float  x, y, z;
    UINT32 color;
    float  u, v;
} RASTER_VERTEX;

typedef struct {
    UINT32 a, b, c;
} RASTER_INDEX_TRIANGLE;

typedef struct {
    union {
        float m[4][4];
        float elements[16];
    };
} RASTER_MAT4;

typedef struct {
    float data[16 * 16]; // 4 rows x 4 cols x 16 lanes = 256 floats = 1024 bytes
} RASTER_SIMT_MAT4_UBO;

STATIC inline VOID RasterMat4Identity(RASTER_MAT4 *Mat) {
    SetMem(Mat, sizeof(RASTER_MAT4), 0);
    Mat->m[0][0] = 1.0f;
    Mat->m[1][1] = 1.0f;
    Mat->m[2][2] = 1.0f;
    Mat->m[3][3] = 1.0f;
}

STATIC inline VOID RasterMat4ToSimtUBO(CONST RASTER_MAT4 *ScalarMat, RASTER_SIMT_MAT4_UBO *SimtUbo) {
    INT32 BaseOffset = 0;
    for (INT32 Col = 0; Col < 4; Col++) {
        for (INT32 Row = 0; Row < 4; Row++) {
            float Value = ScalarMat->m[Row][Col];
            for (INT32 Lane = 0; Lane < 16; Lane++) {
                SimtUbo->data[BaseOffset + Lane] = Value;
            }
            BaseOffset += 16;
        }
    }
}

// ============================================================================
// Default Shaders (Loaded from res/shaders/)
// ============================================================================

#include "../res/shaders/raster_default_vs.h"
#include "../res/shaders/raster_default_fs.h"

// ============================================================================
// Default Full-Screen / Centered Quad Geometry
// ============================================================================

STATIC CONST RASTER_VERTEX gDefaultQuadVertices[4] = {
    { -0.8f, -0.8f, 0.0f, 0xFF00FF00, 0.0f, 0.0f }, // Bottom-Left: Green
    {  0.8f, -0.8f, 0.0f, 0xFF00FF00, 1.0f, 0.0f }, // Bottom-Right: Green
    {  0.8f,  0.8f, 0.0f, 0xFF00FF00, 1.0f, 1.0f }, // Top-Right: Green
    { -0.8f,  0.8f, 0.0f, 0xFF00FF00, 0.0f, 1.0f }  // Top-Left: Green
};

STATIC CONST RASTER_INDEX_TRIANGLE gDefaultQuadIndices[2] = {
    { 0, 1, 2 },
    { 2, 3, 0 }
};

// ============================================================================
// RASTER_FIXTURE Definition & Operations
// ============================================================================

typedef struct _RASTER_FIXTURE {
    GOP_3D_PROTOCOL *Gop3D;

    VRAMADDR hVBO;
    VRAMADDR hIBO;
    VRAMADDR hUBO;
    VRAMADDR hVS;
    VRAMADDR hFS;

    BOOLEAN  BlendEnable;
    GOP_3D_BLEND_FACTOR BlendSrc;
    GOP_3D_BLEND_FACTOR BlendDst;
    BOOLEAN  DepthWriteEnable;
    UINT32   ClearColor;

    VOID (*SetBlend)(struct _RASTER_FIXTURE *Fix, BOOLEAN Enable, GOP_3D_BLEND_FACTOR Src, GOP_3D_BLEND_FACTOR Dst);
    VOID (*SetDepthWrite)(struct _RASTER_FIXTURE *Fix, BOOLEAN Enable);
    VOID (*SetClearColor)(struct _RASTER_FIXTURE *Fix, UINT32 Color);
    VOID (*DrawQuad)(struct _RASTER_FIXTURE *Fix);
    VOID (*Draw)(struct _RASTER_FIXTURE *Fix, GOP_3D_TOPOLOGY Topology, UINT32 VertexCount);
    UINT32 (*ReadPixel)(struct _RASTER_FIXTURE *Fix, UINT32 X, UINT32 Y);
} RASTER_FIXTURE;

STATIC inline VOID RasterFixtureSetBlend(RASTER_FIXTURE *Fix, BOOLEAN Enable, GOP_3D_BLEND_FACTOR Src, GOP_3D_BLEND_FACTOR Dst) {
    if (Fix != NULL) {
        Fix->BlendEnable = Enable;
        Fix->BlendSrc = Src;
        Fix->BlendDst = Dst;
    }
}

STATIC inline VOID RasterFixtureSetDepthWrite(RASTER_FIXTURE *Fix, BOOLEAN Enable) {
    if (Fix != NULL) {
        Fix->DepthWriteEnable = Enable;
    }
}

STATIC inline VOID RasterFixtureSetClearColor(RASTER_FIXTURE *Fix, UINT32 Color) {
    if (Fix != NULL) {
        Fix->ClearColor = Color;
    }
}

STATIC inline VOID RasterFixtureDraw(RASTER_FIXTURE *Fix, GOP_3D_TOPOLOGY Topology, UINT32 VertexCount) {
    if (Fix == NULL || Fix->Gop3D == NULL) return;

    GOP_3D_PROTOCOL *Gop3D = Fix->Gop3D;

    Gop3D->GpuCmdBegin(Gop3D);
    Gop3D->GpuCmdClearFrame(Gop3D, Fix->ClearColor);

    // Apply Blend & Depth states
    Gop3D->GpuCmdSetBlendState(Gop3D, Fix->BlendEnable, Fix->BlendSrc, Fix->BlendDst);
    Gop3D->GpuCmdSetDepthWrite(Gop3D, Fix->DepthWriteEnable);

    // Bind Default Shaders & Buffers
    Gop3D->GpuCmdBindVertShader(Gop3D, Fix->hVS, sizeof(gDefaultRasterVsCode));
    Gop3D->GpuCmdBindFragShader(Gop3D, Fix->hFS, sizeof(gDefaultRasterFsCode));
    Gop3D->GpuCmdBindVBO(Gop3D, Fix->hVBO, 8);
    Gop3D->GpuCmdBindIBO(Gop3D, Fix->hIBO, 12);
    Gop3D->GpuCmdBindUBO(Gop3D, Fix->hUBO, sizeof(RASTER_SIMT_MAT4_UBO));

    // Attributes: Location 0 = Pos (float3), Location 2 = Color (ubyte4 norm)
    Gop3D->GpuCmdVertexAttribPointer(Gop3D, 0, 3, Gop3dAttribFloat, FALSE, sizeof(RASTER_VERTEX), 0);
    Gop3D->GpuCmdEnableVertexAttribArray(Gop3D, 0);
    Gop3D->GpuCmdVertexAttribPointer(Gop3D, 2, 4, Gop3dAttribUbyteNorm, TRUE, sizeof(RASTER_VERTEX), 12);
    Gop3D->GpuCmdEnableVertexAttribArray(Gop3D, 2);
    Gop3D->GpuCmdDisableVertexAttribArray(Gop3D, 1);
    Gop3D->GpuCmdDisableVertexAttribArray(Gop3D, 3);

    // Issue Draw Call
    Gop3D->GpuCmdDraw(Gop3D, Topology, VertexCount);
    Gop3D->GpuCmdEnd(Gop3D);
    Gop3D->GpuPresent(Gop3D);
}

STATIC inline VOID RasterFixtureDrawQuad(RASTER_FIXTURE *Fix) {
    RasterFixtureDraw(Fix, Gop3dTopologyTriangles, 6);
}

STATIC inline UINT32 RasterFixtureReadPixel(RASTER_FIXTURE *Fix, UINT32 X, UINT32 Y) {
    if (Fix == NULL || Fix->Gop3D == NULL) return 0;
    if (X >= 640) X = 639;
    if (Y >= 480) Y = 479;

    UINT32 Pixel = 0;
    VRAMADDR PixelOffset = (Y * 640 + X) * sizeof(UINT32);

    // If PixelOffset is 0, offset slightly to 1 pixel to avoid NULL_ADDR rejection
    if (PixelOffset == 0) {
        PixelOffset = sizeof(UINT32);
    }

    Fix->Gop3D->GpuCmdReadBuffer(Fix->Gop3D, PixelOffset, &Pixel, sizeof(UINT32));
    return Pixel;
}

STATIC inline BOOLEAN HarnessInitRasterFixture(IN GOP_3D_PROTOCOL *Gop3D, OUT RASTER_FIXTURE *Fix) {
    if (Gop3D == NULL || Fix == NULL) return FALSE;

    ZeroMem(Fix, sizeof(RASTER_FIXTURE));
    Fix->Gop3D = Gop3D;
    Fix->ClearColor = 0xFF000000;
    Fix->DepthWriteEnable = TRUE;
    Fix->BlendEnable = FALSE;
    Fix->BlendSrc = Gop3dBlendFactorOne;
    Fix->BlendDst = Gop3dBlendFactorZero;

    // Attach function pointers
    Fix->SetBlend = RasterFixtureSetBlend;
    Fix->SetDepthWrite = RasterFixtureSetDepthWrite;
    Fix->SetClearColor = RasterFixtureSetClearColor;
    Fix->DrawQuad = RasterFixtureDrawQuad;
    Fix->Draw = RasterFixtureDraw;
    Fix->ReadPixel = RasterFixtureReadPixel;

    // Switch GPU to 3D mode
    Gop3D->GpuSetMode(Gop3D, 1);

    // Upload geometry & shaders (Analogous to SpirvApp static asset transfer)
    Gop3D->GpuCmdBegin(Gop3D);
    Gop3D->GpuCmdTransferBuffer(Gop3D, Gop3dBufferTypeVertex, (VOID*)gDefaultQuadVertices, sizeof(gDefaultQuadVertices), &Fix->hVBO);
    Gop3D->GpuCmdTransferBuffer(Gop3D, Gop3dBufferTypeIndex,  (VOID*)gDefaultQuadIndices,  sizeof(gDefaultQuadIndices),  &Fix->hIBO);
    Gop3D->GpuCmdTransferBuffer(Gop3D, Gop3dBufferTypeShaderCode, (VOID*)gDefaultRasterVsCode, sizeof(gDefaultRasterVsCode), &Fix->hVS);
    Gop3D->GpuCmdTransferBuffer(Gop3D, Gop3dBufferTypeShaderCode, (VOID*)gDefaultRasterFsCode, sizeof(gDefaultRasterFsCode), &Fix->hFS);

    // Upload Identity MVP matrix
    RASTER_MAT4 IdentityMat;
    RASTER_SIMT_MAT4_UBO SimtUbo;
    RasterMat4Identity(&IdentityMat);
    RasterMat4ToSimtUBO(&IdentityMat, &SimtUbo);
    Gop3D->GpuCmdTransferBuffer(Gop3D, Gop3dBufferTypeUniform, &SimtUbo, sizeof(RASTER_SIMT_MAT4_UBO), &Fix->hUBO);

    Gop3D->GpuCmdEnd(Gop3D);
    Gop3D->GpuSubmitCmd(Gop3D);

    return TRUE;
}

STATIC inline VOID HarnessDestroyRasterFixture(IN RASTER_FIXTURE *Fix) {
    if (Fix == NULL || Fix->Gop3D == NULL) return;

    GOP_3D_PROTOCOL *Gop3D = Fix->Gop3D;

    if (Fix->hVBO != GPU_NULL_ADDR) Gop3D->GpuFreeBuffer(Gop3D, &Fix->hVBO);
    if (Fix->hIBO != GPU_NULL_ADDR) Gop3D->GpuFreeBuffer(Gop3D, &Fix->hIBO);
    if (Fix->hUBO != GPU_NULL_ADDR) Gop3D->GpuFreeBuffer(Gop3D, &Fix->hUBO);
    if (Fix->hVS  != GPU_NULL_ADDR) Gop3D->GpuFreeBuffer(Gop3D, &Fix->hVS);
    if (Fix->hFS  != GPU_NULL_ADDR) Gop3D->GpuFreeBuffer(Gop3D, &Fix->hFS);

    // Return to GOP mode
    Gop3D->GpuSetMode(Gop3D, 0);
}

#endif // __RASTER_FIXTURE_H__
