#include "harness.h"

STATIC VOID SetupQuadMeshlet(
    OUT MESH_TEST_MESHLET *Meshlet,
    OUT float             *Positions,
    OUT UINT32            *Indices,
    IN  float             MinX,
    IN  float             MaxX,
    IN  float             MinY,
    IN  float             MaxY,
    IN  float             R,
    IN  float             G,
    IN  float             B,
    IN  UINT32            VertexOffset,
    IN  UINT32            PrimitiveOffset
    )
{
    ZeroMem(Meshlet, sizeof(MESH_TEST_MESHLET));
    Meshlet->bounding_sphere[3] = 1.0f;
    Meshlet->cone[0] = R;
    Meshlet->cone[1] = G;
    Meshlet->cone[2] = B;
    Meshlet->cone[3] = 0.0f;
    Meshlet->vertex_offset = VertexOffset;
    Meshlet->vertex_count = 4;
    Meshlet->primitive_offset = PrimitiveOffset;
    Meshlet->primitive_count = 2;

    Positions[0]  = MinX; Positions[1]  = MinY; Positions[2]  = 0.5f; Positions[3]  = 1.0f;
    Positions[4]  = MaxX; Positions[5]  = MinY; Positions[6]  = 0.5f; Positions[7]  = 1.0f;
    Positions[8]  = MaxX; Positions[9]  = MaxY; Positions[10] = 0.5f; Positions[11] = 1.0f;
    Positions[12] = MinX; Positions[13] = MaxY; Positions[14] = 0.5f; Positions[15] = 1.0f;

    Indices[0] = 0; Indices[1] = 1; Indices[2] = 2;
    Indices[3] = 0; Indices[4] = 2; Indices[5] = 3;
}

GPU_MESH_TEST(Test_Mesh_TaskCullAndRaster, "test-mesh.pipeline.task_cull_raster", "Task shader meshlet culling and mesh shader triangle rasterization")
{
    MESH_TEST_MESHLET Meshlets[16];
    float Positions[12][4] = {
        // Meshlet 0 (culled by Task Shader: bounding_sphere.w = -1.0f): Left side triangle
        { -0.9f, -0.5f, 0.5f, 1.0f },
        { -0.3f, -0.5f, 0.5f, 1.0f },
        { -0.6f,  0.5f, 0.5f, 1.0f },

        // Meshlet 1 (visible in Task Shader: bounding_sphere.w = 1.0f): Centered triangle covering (320, 240)
        { -0.5f, -0.5f, 0.5f, 1.0f },
        {  0.5f, -0.5f, 0.5f, 1.0f },
        {  0.0f,  0.5f, 0.5f, 1.0f },

        // Meshlet 2 (visible in Task Shader, but primitive culled via gl_CullPrimitiveEXT): Right side triangle
        {  0.3f, -0.5f, 0.5f, 1.0f },
        {  0.9f, -0.5f, 0.5f, 1.0f },
        {  0.6f,  0.5f, 0.5f, 1.0f },

        // Padding vertices
        {  0.0f,  0.0f, 0.5f, 1.0f },
        {  0.0f,  0.0f, 0.5f, 1.0f },
        {  0.0f,  0.0f, 0.5f, 1.0f }
    };

    UINT32 PackedTriangles[4] = {
        0u | (1u << 8) | (2u << 16) | (0u << 24), // Meshlet 0: not primitive-culled, but task-culled
        0u | (1u << 8) | (2u << 16) | (0u << 24), // Meshlet 1: visible primitive
        0u | (1u << 8) | (2u << 16) | (1u << 24), // Meshlet 2: culled primitive (flag bit 0 = 1 -> gl_CullPrimitiveEXT = true)
        0u
    };

    ZeroMem(Meshlets, sizeof(Meshlets));
    for (UINT32 i = 0; i < 16; i++) {
        Meshlets[i].bounding_sphere[3] = -1.0f; // Default culled
        Meshlets[i].cone[3] = 0.0f;
    }

    // Meshlet 0: Task-culled (bounding_sphere.w = -1.0f)
    Meshlets[0].bounding_sphere[3] = -1.0f;
    Meshlets[0].vertex_offset = 0;
    Meshlets[0].vertex_count = 3;
    Meshlets[0].primitive_offset = 0;
    Meshlets[0].primitive_count = 1;

    // Meshlet 1: Visible (bounding_sphere.w = 1.0f, cone.w = 0.0f)
    Meshlets[1].bounding_sphere[3] = 1.0f;
    Meshlets[1].vertex_offset = 3;
    Meshlets[1].vertex_count = 3;
    Meshlets[1].primitive_offset = 1;
    Meshlets[1].primitive_count = 1;

    // Meshlet 2: Visible at Task stage, primitive-culled at Mesh stage
    Meshlets[2].bounding_sphere[3] = 1.0f;
    Meshlets[2].vertex_offset = 6;
    Meshlets[2].vertex_count = 3;
    Meshlets[2].primitive_offset = 2;
    Meshlets[2].primitive_count = 1;

    Fix->SetBuffer(Fix, 0, Meshlets, sizeof(Meshlets));
    Fix->SetBuffer(Fix, 1, Positions, sizeof(Positions));
    Fix->SetBuffer(Fix, 2, PackedTriangles, sizeof(PackedTriangles));

    Fix->DrawMeshTasks(Fix, 1, 1, 1);

    // Center pixel (320, 240) is inside Meshlet 1's visible triangle (meshlet_id = 1 -> R = 255, G > 100, B = 255)
    UINT32 CenterPixel = Fix->ReadPixel(Fix, 320, 240);
    UINT32 CenterR = (CenterPixel >> 16) & 0xFF;
    UINT32 CenterG = (CenterPixel >> 8) & 0xFF;
    UINT32 CenterB = CenterPixel & 0xFF;
    GPU_ASSERT(CenterR == 255 && CenterG > 100 && CenterB == 255, "Visible meshlet triangle must render at screen center");

    // Left pixel (128, 240) is inside Task-culled Meshlet 0 -> must remain background black
    UINT32 LeftPixel = Fix->ReadPixel(Fix, 128, 240);
    GPU_ASSERT((LeftPixel & 0x00FFFFFF) == 0x00000000, "Task-culled meshlet must not be rasterized");

    // Right pixel (512, 240) is inside Primitive-culled Meshlet 2 -> must remain background black
    UINT32 RightPixel = Fix->ReadPixel(Fix, 512, 240);
    GPU_ASSERT((RightPixel & 0x00FFFFFF) == 0x00000000, "Primitive-culled triangle (gl_CullPrimitiveEXT) must not be rasterized");

    return GPU_TEST_PASS();
}

GPU_MESH_TEST(Test_Mesh_MeshOnly, "test-mesh.pipeline.mesh_only", "Mesh-only pipeline without a Task shader bound")
{
    MESH_TEST_MESHLET Meshlets[2];
    float             Positions[8][4];
    UINT32            Indices[12];

    // Meshlet 0: Left quad [-0.8, -0.2] x [-0.5, 0.5], Red (1, 0, 0)
    SetupQuadMeshlet(&Meshlets[0], &Positions[0][0], &Indices[0], -0.8f, -0.2f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0, 0);
    // Meshlet 1: Right quad [0.2, 0.8] x [-0.5, 0.5], Green (0, 1, 0)
    SetupQuadMeshlet(&Meshlets[1], &Positions[4][0], &Indices[6],  0.2f,  0.8f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 4, 2);

    // Unbind Task Shader (ts_addr = 0) and bind Mesh-Only shader
    Fix->SetTaskShader(Fix, NULL, 0);
    Fix->SetMeshShader(Fix, gMeshOnlyShaderCode, sizeof(gMeshOnlyShaderCode));

    Fix->SetBuffer(Fix, 0, Meshlets, sizeof(Meshlets));
    Fix->SetBuffer(Fix, 1, Positions, sizeof(Positions));
    Fix->SetBuffer(Fix, 2, Indices, sizeof(Indices));

    // Dispatch 2 Mesh Shader workgroups directly
    Fix->DrawMeshTasks(Fix, 2, 1, 1);

    UINT32 LeftPixel   = Fix->ReadPixel(Fix, 160, 240) & 0x00FFFFFF;
    UINT32 CenterPixel = Fix->ReadPixel(Fix, 320, 240) & 0x00FFFFFF;
    UINT32 RightPixel  = Fix->ReadPixel(Fix, 480, 240) & 0x00FFFFFF;

    GPU_ASSERT(LeftPixel == 0x00FF0000, "Workgroup 0 must rasterize left red quad in mesh-only mode");
    GPU_ASSERT(RightPixel == 0x0000FF00, "Workgroup 1 must rasterize right green quad in mesh-only mode");
    GPU_ASSERT(CenterPixel == 0x00000000, "Gap between workgroup quads must remain clear color");

    return GPU_TEST_PASS();
}

GPU_MESH_TEST(Test_Mesh_IndirectDispatch, "test-mesh.dispatch.indirect", "Indirect mesh task dispatch from VRAM buffer offset")
{
    MESH_TEST_MESHLET Meshlets[2];
    float             Positions[8][4];
    UINT32            Indices[12];

    // Meshlet 0: Left quad, Red (1, 0, 0)
    SetupQuadMeshlet(&Meshlets[0], &Positions[0][0], &Indices[0], -0.8f, -0.2f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0, 0);
    // Meshlet 1: Right quad, Blue (0, 0, 1)
    SetupQuadMeshlet(&Meshlets[1], &Positions[4][0], &Indices[6],  0.2f,  0.8f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 4, 2);

    // Indirect buffer with non-zero byte offset (offset 16 bytes holds {2, 1, 1})
    UINT32 IndirectBuffer[8] = {
        0, 0, 0, 0, // Offset 0: unused padding
        2, 1, 1, 0  // Offset 16: GroupCountX=2, GroupCountY=1, GroupCountZ=1
    };

    Fix->SetTaskShader(Fix, NULL, 0);
    Fix->SetMeshShader(Fix, gMeshOnlyShaderCode, sizeof(gMeshOnlyShaderCode));

    Fix->SetBuffer(Fix, 0, Meshlets, sizeof(Meshlets));
    Fix->SetBuffer(Fix, 1, Positions, sizeof(Positions));
    Fix->SetBuffer(Fix, 2, Indices, sizeof(Indices));
    Fix->SetBuffer(Fix, 4, IndirectBuffer, sizeof(IndirectBuffer));

    Fix->DrawMeshTasksIndirect(Fix, 4, 16);

    UINT32 LeftPixel  = Fix->ReadPixel(Fix, 160, 240) & 0x00FFFFFF;
    UINT32 RightPixel = Fix->ReadPixel(Fix, 480, 240) & 0x00FFFFFF;

    GPU_ASSERT(LeftPixel == 0x00FF0000, "Indirect dispatch workgroup 0 must rasterize left red quad");
    GPU_ASSERT(RightPixel == 0x000000FF, "Indirect dispatch workgroup 1 must rasterize right blue quad");

    return GPU_TEST_PASS();
}

GPU_MESH_TEST(Test_Mesh_PointsAndLines, "test-mesh.topology.points_and_lines", "Mesh shader OutputPoints (gl_PointSize) and OutputLinesEXT topologies")
{
    // --- Part 1: OutputPoints topology with gl_PointSize = 12.0 ---
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

    // --- Part 2: OutputLinesEXT topology ---
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

GPU_MESH_TEST(Test_Mesh_WorkgroupBarrierShared, "test-mesh.sync.workgroup_barrier_shared", "Multi-workgroup Task and Mesh shader shared memory and OpControlBarrier")
{
    MESH_TEST_MESHLET Meshlets[4];
    float             Positions[16][4];
    UINT32            Indices[24];

    // Meshlet 0: Unused dummy
    SetupQuadMeshlet(&Meshlets[0], &Positions[0][0],  &Indices[0],  -0.1f,  0.1f, -0.1f, 0.1f, 1.0f, 1.0f, 0.0f,  0, 0);
    // Meshlet 1: Left quad, Red (1, 0, 0)
    SetupQuadMeshlet(&Meshlets[1], &Positions[4][0],  &Indices[6],  -0.8f, -0.2f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f,  4, 2);
    // Meshlet 2: Unused dummy
    SetupQuadMeshlet(&Meshlets[2], &Positions[8][0],  &Indices[12], -0.1f,  0.1f, -0.1f, 0.1f, 0.0f, 1.0f, 1.0f,  8, 4);
    // Meshlet 3: Right quad, Green (0, 1, 0)
    SetupQuadMeshlet(&Meshlets[3], &Positions[12][0], &Indices[18],  0.2f,  0.8f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 12, 6);

    // Task shader swaps pairs (remap_ids[wg*2 + 0], remap_ids[wg*2 + 1]) in shared memory across barrier()
    // Workgroup 0 swaps (0, 1) -> payload.meshlet_ids[0] = 1 (Left Red quad)
    // Workgroup 1 swaps (2, 3) -> payload.meshlet_ids[0] = 3 (Right Green quad)
    UINT32 RemapIds[4] = { 0, 1, 2, 3 };

    Fix->SetTaskShader(Fix, gMeshBarrierTaskShaderCode, sizeof(gMeshBarrierTaskShaderCode));
    Fix->SetMeshShader(Fix, gMeshBarrierMeshShaderCode, sizeof(gMeshBarrierMeshShaderCode));

    Fix->SetBuffer(Fix, 0, Meshlets, sizeof(Meshlets));
    Fix->SetBuffer(Fix, 1, Positions, sizeof(Positions));
    Fix->SetBuffer(Fix, 2, Indices, sizeof(Indices));
    Fix->SetBuffer(Fix, 3, RemapIds, sizeof(RemapIds));

    Fix->DrawMeshTasks(Fix, 2, 1, 1);

    UINT32 LeftPixel   = Fix->ReadPixel(Fix, 160, 240) & 0x00FFFFFF;
    UINT32 CenterPixel = Fix->ReadPixel(Fix, 320, 240) & 0x00FFFFFF;
    UINT32 RightPixel  = Fix->ReadPixel(Fix, 480, 240) & 0x00FFFFFF;

    GPU_ASSERT(LeftPixel == 0x00FF0000, "Task/Mesh workgroup 0 shared memory barrier exchange must render left red quad (Meshlet 1)");
    GPU_ASSERT(RightPixel == 0x0000FF00, "Task/Mesh workgroup 1 shared memory barrier exchange must render right green quad (Meshlet 3)");
    GPU_ASSERT(CenterPixel == 0x00000000, "Unselected meshlets 0 and 2 at center must not be rendered");

    return GPU_TEST_PASS();
}

GPU_MESH_TEST(Test_Mesh_ZeroOutputs, "test-mesh.edge.zero_outputs", "Zero mesh tasks (EmitMeshTasksEXT(0,1,1)) and zero mesh outputs (SetMeshOutputsEXT(0,0))")
{
    MESH_TEST_MESHLET Meshlets[16];
    float             Positions[4][4];
    UINT32            Indices[6];

    SetupQuadMeshlet(&Meshlets[0], &Positions[0][0], Indices, -0.8f, 0.8f, -0.8f, 0.8f, 1.0f, 0.0f, 0.0f, 0, 0);
    for (UINT32 i = 0; i < 16; i++) {
        Meshlets[i] = Meshlets[0];
        Meshlets[i].bounding_sphere[3] = -1.0f; // Culled -> Task shader emits 0 tasks
    }

    Fix->ClearColor = 0xFF112233;
    Fix->SetBuffer(Fix, 0, Meshlets, sizeof(Meshlets));
    Fix->SetBuffer(Fix, 1, Positions, sizeof(Positions));
    Fix->SetBuffer(Fix, 2, Indices, sizeof(Indices));

    // Case 1: Task shader calls EmitMeshTasksEXT(0, 1, 1)
    Fix->SetTaskShader(Fix, gDefaultTaskShaderCode, sizeof(gDefaultTaskShaderCode));
    Fix->SetMeshShader(Fix, gDefaultMeshShaderCode, sizeof(gDefaultMeshShaderCode));
    Fix->DrawMeshTasks(Fix, 1, 1, 1);

    UINT32 PixelCase1 = Fix->ReadPixel(Fix, 320, 240) & 0x00FFFFFF;
    GPU_ASSERT(PixelCase1 == 0x00112233, "EmitMeshTasksEXT(0, 1, 1) must leave framebuffer untouched");

    // Case 2: Mesh-only shader calls SetMeshOutputsEXT(0, 0)
    Meshlets[0].vertex_count = 0;
    Meshlets[0].primitive_count = 0;
    Fix->SetBuffer(Fix, 0, Meshlets, sizeof(Meshlets));
    Fix->SetTaskShader(Fix, NULL, 0);
    Fix->SetMeshShader(Fix, gMeshOnlyShaderCode, sizeof(gMeshOnlyShaderCode));
    Fix->DrawMeshTasks(Fix, 1, 1, 1);

    UINT32 PixelCase2 = Fix->ReadPixel(Fix, 320, 240) & 0x00FFFFFF;
    GPU_ASSERT(PixelCase2 == 0x00112233, "SetMeshOutputsEXT(0, 0) must leave framebuffer untouched");

    // Case 3: DrawMeshTasks(0, 1, 1) with non-empty meshlet
    Meshlets[0].vertex_count = 4;
    Meshlets[0].primitive_count = 2;
    Fix->SetBuffer(Fix, 0, Meshlets, sizeof(Meshlets));
    Fix->DrawMeshTasks(Fix, 0, 1, 1);

    UINT32 PixelCase3 = Fix->ReadPixel(Fix, 320, 240) & 0x00FFFFFF;
    GPU_ASSERT(PixelCase3 == 0x00112233, "DrawMeshTasks(0, 1, 1) must leave framebuffer untouched");

    return GPU_TEST_PASS();
}
