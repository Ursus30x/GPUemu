#include "harness.h"

GPU_COMPUTE_TEST(Test_Compute_VecAdd, "test-compute.ssbo.vec_add", "Compute shader parallel vector addition with SSBOs")
{
    float InputA[16];
    float InputB[16];
    float OutputC[16];
    UINT32 i;

    for (i = 0; i < 16; i++) {
        InputA[i] = (float)i * 1.5f;
        InputB[i] = (float)i * 2.5f;
        OutputC[i] = 0.0f;
    }

    // Slot 0: Input A, Slot 1: Input B, Slot 2: Output C (per vec_add.comp bindings)
    Fix->SetBuffer(Fix, 0, InputA, sizeof(InputA));
    Fix->SetBuffer(Fix, 1, InputB, sizeof(InputB));
    Fix->SetBuffer(Fix, 2, OutputC, sizeof(OutputC));

    // Dispatch 1 workgroup (16 invocations)
    Fix->Dispatch(Fix, 1, 1, 1);

    // Read back Output C from Slot 2
    Fix->ReadBuffer(Fix, 2, OutputC, sizeof(OutputC));

    // Assert c[i] == a[i] + b[i]
    for (i = 0; i < 16; i++) {
        float Expected = InputA[i] + InputB[i];
        float Diff = OutputC[i] - Expected;
        if (Diff < 0) Diff = -Diff;
        GPU_ASSERT(Diff < 0.001f, "Vector addition result does not match expected sum");
    }

    return GPU_TEST_PASS();
}
