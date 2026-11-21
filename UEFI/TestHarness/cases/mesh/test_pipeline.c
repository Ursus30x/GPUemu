#include "harness.h"

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
