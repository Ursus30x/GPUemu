#include "harness.h"

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
