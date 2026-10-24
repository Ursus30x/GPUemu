#ifndef __FIXTURES_H__
#define __FIXTURES_H__

#include "common_fixture.h"
#include "raster_fixture.h"
#include "compute_fixture.h"

// ============================================================================
// Fixture Test Declaration Macros
// ============================================================================

// Raster Fixture Test: Injects RASTER_FIXTURE *Fix and guarantees VRAM cleanup
#define GPU_RASTER_TEST(FuncName, PathStr, DescStr) \
    STATIC TEST_STATUS FuncName##_Body(RASTER_FIXTURE *Fix); \
    TEST_STATUS FuncName(IN GOP_3D_PROTOCOL *Gop3D) { \
        RASTER_FIXTURE Fix; \
        TEST_STATUS Status; \
        if (!HarnessInitRasterFixture(Gop3D, &Fix)) { \
            return GPU_TEST_FAIL("Failed to initialize RASTER_FIXTURE"); \
        } \
        Status = FuncName##_Body(&Fix); \
        HarnessDestroyRasterFixture(&Fix); \
        return Status; \
    } \
    STATIC TEST_STATUS FuncName##_Body(RASTER_FIXTURE *Fix)

// Compute Fixture Test: Injects COMPUTE_FIXTURE *Fix and guarantees VRAM cleanup
#define GPU_COMPUTE_TEST(FuncName, PathStr, DescStr) \
    STATIC TEST_STATUS FuncName##_Body(COMPUTE_FIXTURE *Fix); \
    TEST_STATUS FuncName(IN GOP_3D_PROTOCOL *Gop3D) { \
        COMPUTE_FIXTURE Fix; \
        TEST_STATUS Status; \
        if (!HarnessInitComputeFixture(Gop3D, &Fix)) { \
            return GPU_TEST_FAIL("Failed to initialize COMPUTE_FIXTURE"); \
        } \
        Status = FuncName##_Body(&Fix); \
        HarnessDestroyComputeFixture(&Fix); \
        return Status; \
    } \
    STATIC TEST_STATUS FuncName##_Body(COMPUTE_FIXTURE *Fix)

#endif // __FIXTURES_H__
