#include "harness.h"

GPU_RASTER_TEST(Test_Raster_BlendAlpha, "test-raster.blend.alpha", "Verifies alpha blending with background clear color")
{
    // Clear background to blue (0x0000FF)
    Fix->SetClearColor(Fix, 0xFF0000FF);

    // Enable standard alpha blending
    Fix->SetBlend(Fix, TRUE, Gop3dBlendFactorSrcAlpha, Gop3dBlendFactorOneMinusSrcAlpha);

    // Draw default green quad
    Fix->DrawQuad(Fix);

    // Verify center pixel color rendered quad color
    UINT32 CenterPixel = Fix->ReadPixel(Fix, 320, 240);
    GPU_ASSERT((CenterPixel & 0x00FFFFFF) == 0x0000FF00, "Center pixel must match rendered quad color");

    // Corner pixel outside quad should remain blue background
    UINT32 CornerPixel = Fix->ReadPixel(Fix, 10, 10);
    GPU_ASSERT((CornerPixel & 0x00FFFFFF) == 0x000000FF, "Corner pixel outside quad must match background clear color (blue)");

    return GPU_TEST_PASS();
}
