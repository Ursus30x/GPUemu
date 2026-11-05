#include "harness.h"

GPU_RASTER_TEST(Test_Raster_Triangles, "test-raster.primitive.triangles", "Rasterizes basic solid triangle")
{
    // Draw default green quad
    Fix->DrawQuad(Fix);

    // Verify center pixel color rendered green (0x00FF00)
    UINT32 CenterPixel = Fix->ReadPixel(Fix, 320, 240);
    GPU_ASSERT((CenterPixel & 0x00FFFFFF) == 0x0000FF00, "Center pixel must match rendered quad color (green)");

    return GPU_TEST_PASS();
}
