#include "harness.h"

GPU_MESH_TEST(Test_Mesh_ZeroTaskEmit, "test-mesh.edge.zero_task_emit", "Task shader emitting zero mesh tasks via EmitMeshTasksEXT(0, 1, 1)")
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

    Fix->DrawMeshTasks(Fix, 1, 1, 1);

    UINT32 Pixel = Fix->ReadPixel(Fix, 320, 240) & 0x00FFFFFF;
    GPU_ASSERT(Pixel == 0x00112233, "EmitMeshTasksEXT(0, 1, 1) must leave framebuffer untouched");

    return GPU_TEST_PASS();
}

GPU_MESH_TEST(Test_Mesh_ZeroMeshOutputs, "test-mesh.edge.zero_mesh_outputs", "Mesh shader emitting zero vertices and primitives via SetMeshOutputsEXT(0, 0)")
{
    MESH_TEST_MESHLET Meshlet;
    float             Positions[4][4];
    UINT32            Indices[6];

    SetupQuadMeshlet(&Meshlet, &Positions[0][0], Indices, -0.8f, 0.8f, -0.8f, 0.8f, 1.0f, 0.0f, 0.0f, 0, 0);
    Meshlet.vertex_count = 0;
    Meshlet.primitive_count = 0;

    Fix->ClearColor = 0xFF112233;
    Fix->SetTaskShader(Fix, NULL, 0);
    Fix->SetMeshShader(Fix, gMeshOnlyShaderCode, sizeof(gMeshOnlyShaderCode));
    Fix->SetBuffer(Fix, 0, &Meshlet, sizeof(Meshlet));
    Fix->SetBuffer(Fix, 1, Positions, sizeof(Positions));
    Fix->SetBuffer(Fix, 2, Indices, sizeof(Indices));

    Fix->DrawMeshTasks(Fix, 1, 1, 1);

    UINT32 Pixel = Fix->ReadPixel(Fix, 320, 240) & 0x00FFFFFF;
    GPU_ASSERT(Pixel == 0x00112233, "SetMeshOutputsEXT(0, 0) must leave framebuffer untouched");

    return GPU_TEST_PASS();
}

GPU_MESH_TEST(Test_Mesh_ZeroDrawDispatch, "test-mesh.edge.zero_draw_dispatch", "DrawMeshTasks(0, 1, 1) with non-empty meshlet leaves framebuffer untouched")
{
    MESH_TEST_MESHLET Meshlet;
    float             Positions[4][4];
    UINT32            Indices[6];

    SetupQuadMeshlet(&Meshlet, &Positions[0][0], Indices, -0.8f, 0.8f, -0.8f, 0.8f, 1.0f, 0.0f, 0.0f, 0, 0);

    Fix->ClearColor = 0xFF112233;
    Fix->SetTaskShader(Fix, NULL, 0);
    Fix->SetMeshShader(Fix, gMeshOnlyShaderCode, sizeof(gMeshOnlyShaderCode));
    Fix->SetBuffer(Fix, 0, &Meshlet, sizeof(Meshlet));
    Fix->SetBuffer(Fix, 1, Positions, sizeof(Positions));
    Fix->SetBuffer(Fix, 2, Indices, sizeof(Indices));

    Fix->DrawMeshTasks(Fix, 0, 1, 1);

    UINT32 Pixel = Fix->ReadPixel(Fix, 320, 240) & 0x00FFFFFF;
    GPU_ASSERT(Pixel == 0x00112233, "DrawMeshTasks(0, 1, 1) must leave framebuffer untouched");

    return GPU_TEST_PASS();
}
