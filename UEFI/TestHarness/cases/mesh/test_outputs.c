#include "harness.h"

GPU_MESH_TEST(Test_Mesh_TopologyPoints, "test-mesh.topology.points", "Mesh shader OutputPoints topology with gl_PointSize")
{
    float PointPositions[2][4] = {
        { -0.5f, 0.0f, 0.5f, 1.0f }, // Screen (160, 240)
        {  0.5f, 0.0f, 0.5f, 1.0f }  // Screen (480, 240)
    };
    float PointColors[2][4] = {
        { 1.0f, 0.0f, 0.0f, 1.0f },  // Red
        { 0.0f, 1.0f, 0.0f, 1.0f }   // Green
    };

    Fix->SetTaskShader(Fix, NULL, 0);
    Fix->SetMeshShader(Fix, gMeshPointsShaderCode, sizeof(gMeshPointsShaderCode));
    Fix->SetBuffer(Fix, 0, PointColors, sizeof(PointColors));
    Fix->SetBuffer(Fix, 1, PointPositions, sizeof(PointPositions));

    Fix->DrawMeshTasks(Fix, 1, 1, 1);

    UINT32 PtLeft   = Fix->ReadPixel(Fix, 160, 240) & 0x00FFFFFF;
    UINT32 PtCenter = Fix->ReadPixel(Fix, 320, 240) & 0x00FFFFFF;
    UINT32 PtRight  = Fix->ReadPixel(Fix, 480, 240) & 0x00FFFFFF;

    GPU_ASSERT(PtLeft == 0x00FF0000, "Point 0 must rasterize red disc at (160, 240) using gl_PointSize");
    GPU_ASSERT(PtRight == 0x0000FF00, "Point 1 must rasterize green disc at (480, 240) using gl_PointSize");
    GPU_ASSERT(PtCenter == 0x00000000, "Midpoint between points must remain clear color");

    return GPU_TEST_PASS();
}

GPU_MESH_TEST(Test_Mesh_TopologyLines, "test-mesh.topology.lines", "Mesh shader OutputLinesEXT topology")
{
    // Map exact pixel centers py=120.5 and py=360.5 to NDC Y
    float NdcYTop = 1.0f - 2.0f * (120.5f / 480.0f);
    float NdcYBot = 1.0f - 2.0f * (360.5f / 480.0f);

    float LinePositions[4][4] = {
        { -0.5f, NdcYTop, 0.5f, 1.0f },
        {  0.5f, NdcYTop, 0.5f, 1.0f },
        { -0.5f, NdcYBot, 0.5f, 1.0f },
        {  0.5f, NdcYBot, 0.5f, 1.0f }
    };
    float LineColors[4][4] = {
        { 1.0f, 0.0f, 0.0f, 1.0f }, // Line 0: Red
        { 1.0f, 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f, 1.0f }, // Line 1: Blue
        { 0.0f, 0.0f, 1.0f, 1.0f }
    };

    Fix->SetTaskShader(Fix, NULL, 0);
    Fix->SetMeshShader(Fix, gMeshLinesShaderCode, sizeof(gMeshLinesShaderCode));
    Fix->SetBuffer(Fix, 0, LineColors, sizeof(LineColors));
    Fix->SetBuffer(Fix, 1, LinePositions, sizeof(LinePositions));

    Fix->DrawMeshTasks(Fix, 1, 1, 1);

    UINT32 LineTop    = Fix->ReadPixel(Fix, 320, 120) & 0x00FFFFFF;
    UINT32 LineMiddle = Fix->ReadPixel(Fix, 320, 240) & 0x00FFFFFF;
    UINT32 LineBottom = Fix->ReadPixel(Fix, 320, 360) & 0x00FFFFFF;

    GPU_ASSERT(LineTop == 0x00FF0000, "Line 0 must rasterize red segment across (320, 120)");
    GPU_ASSERT(LineBottom == 0x000000FF, "Line 1 must rasterize blue segment across (320, 360)");
    GPU_ASSERT(LineMiddle == 0x00000000, "Center between lines must remain clear color");

    return GPU_TEST_PASS();
}

GPU_MESH_TEST(Test_Mesh_PerPrimitiveVarying, "test-mesh.varying.per_primitive", "Per-primitive varying attribute (perprimitiveEXT) across shared vertices")
{
    // Quad spanning [-0.6, 0.6] x [-0.6, 0.6] (screen [128, 512] x [96, 384])
    // Triangle 0 (0, 1, 2) and Triangle 1 (0, 2, 3) share vertices 0 and 2
    float Positions[4][4] = {
        { -0.6f, -0.6f, 0.5f, 1.0f }, // 0: Bottom-Left  (128, 384)
        {  0.6f, -0.6f, 0.5f, 1.0f }, // 1: Bottom-Right (512, 384)
        {  0.6f,  0.6f, 0.5f, 1.0f }, // 2: Top-Right    (512,  96)
        { -0.6f,  0.6f, 0.5f, 1.0f }  // 3: Top-Left     (128,  96)
    };
    UINT32 Indices[6] = {
        0, 1, 2, // Triangle 0: Bottom-right half of quad (centroid 384, 288)
        0, 2, 3  // Triangle 1: Top-left half of quad     (centroid 256, 192)
    };
    float PrimColors[2][4] = {
        { 1.0f, 0.0f, 0.0f, 1.0f }, // Triangle 0: flat Red
        { 0.0f, 1.0f, 0.0f, 1.0f }  // Triangle 1: flat Green
    };

    Fix->SetTaskShader(Fix, NULL, 0);
    Fix->SetMeshShader(Fix, gMeshPerPrimShaderCode, sizeof(gMeshPerPrimShaderCode));
    Fix->SetBuffer(Fix, 0, PrimColors, sizeof(PrimColors));
    Fix->SetBuffer(Fix, 1, Positions, sizeof(Positions));
    Fix->SetBuffer(Fix, 2, Indices, sizeof(Indices));

    Fix->DrawMeshTasks(Fix, 1, 1, 1);

    UINT32 Tri0Pixel = Fix->ReadPixel(Fix, 384, 288) & 0x00FFFFFF;
    UINT32 Tri1Pixel = Fix->ReadPixel(Fix, 256, 192) & 0x00FFFFFF;

    GPU_ASSERT(Tri0Pixel == 0x00FF0000, "Triangle 0 must receive flat red perprimitiveEXT varying");
    GPU_ASSERT(Tri1Pixel == 0x0000FF00, "Triangle 1 must receive flat green perprimitiveEXT varying without bleeding");

    return GPU_TEST_PASS();
}
