#include "test.h"
#include <sys/wait.h>
#include <time.h>

TEST(fs_smpl_nearest, "out/sampler.spv", FRAGMENT_SHADER, {
    // 2x2 Texture: Red, Green, Blue, Yellow
    uint8_t tex_data[2 * 2 * 4] = {
        255,   0,   0, 255, // (0,0) = Red
          0, 255,   0, 255, // (1,0) = Green
          0,   0, 255, 255, // (0,1) = Blue
        255, 255,   0, 255  // (1,1) = Yellow
    };
    TextureSamplerDescriptor desc = {
        .data = tex_data,
        .width = 2,
        .height = 2,
        .channels = 4,
        .filter = FILTER_NEAREST,
        .wrap = WRAP_REPEAT
    };
    CREATE_BINDING(1, desc);

    SimtFloat u = {0};
    SimtFloat v = {0};
    SimtVec4 expected = {0};

    for (int i = 0; i < 4; i++) {
        u[i] = 0.25f; v[i] = 0.25f; // (0,0) Red
        expected.elem[0][i] = 1.0f; expected.elem[1][i] = 0.0f; expected.elem[2][i] = 0.0f; expected.elem[3][i] = 1.0f;
    }
    for (int i = 4; i < 8; i++)
     {
        u[i] = 0.75f; v[i] = 0.25f; // (1,0) Green
        expected.elem[0][i] = 0.0f; expected.elem[1][i] = 1.0f; expected.elem[2][i] = 0.0f; expected.elem[3][i] = 1.0f;
    }
    for (int i = 8; i < 12; i++) 
    {
        u[i] = 0.25f; v[i] = 0.75f; // (0,1) Blue
        expected.elem[0][i] = 0.0f; expected.elem[1][i] = 0.0f; expected.elem[2][i] = 1.0f; expected.elem[3][i] = 1.0f;
    }
    for (int i = 12; i < 16; i++)
     {
        u[i] = 0.75f; v[i] = 0.75f; // (1,1) Yellow
        expected.elem[0][i] = 1.0f; expected.elem[1][i] = 1.0f; expected.elem[2][i] = 0.0f; expected.elem[3][i] = 1.0f;
    }

    CREATE_SIMT_VEC2(vTexCoord, u, v);
    BIND_IN_LOCATION(0, vTexCoord);

    CREATE_SIMT_VEC4(outCol, SPLAT(0.0), SPLAT(0.0), SPLAT(0.0), SPLAT(0.0));
    BIND_OUT_LOCATION(0, outCol);

    RUN_JIT();

    PRINT_VEC4("outCol:\n", outCol);
    ASSERT_NEAR_VEC4(outCol, expected, 1e-4f);
})

TEST(fs_smpl_linear, "out/sampler.spv", FRAGMENT_SHADER, {
    // 2x2 Texture: Red, Green, Blue, Yellow
    uint8_t tex_data[2 * 2 * 4] = {
        255,   0,   0, 255, // (0,0) = Red
          0, 255,   0, 255, // (1,0) = Green
          0,   0, 255, 255, // (0,1) = Blue
        255, 255,   0, 255  // (1,1) = Yellow
    };
    TextureSamplerDescriptor desc = {
        .data = tex_data,
        .width = 2,
        .height = 2,
        .channels = 4,
        .filter = FILTER_LINEAR,
        .wrap = WRAP_REPEAT
    };
    CREATE_BINDING(1, desc);

    SimtFloat u = SPLAT(0.5f);
    SimtFloat v = SPLAT(0.5f);
    CREATE_SIMT_VEC2(vTexCoord, u, v);
    BIND_IN_LOCATION(0, vTexCoord);

    CREATE_SIMT_VEC4(outCol, SPLAT(0.0), SPLAT(0.0), SPLAT(0.0), SPLAT(0.0));
    BIND_OUT_LOCATION(0, outCol);

    RUN_JIT();

    PRINT_VEC4("outCol:\n", outCol);
    // Center of 4 texels: (255+0+0+255)/4=127.5/255=0.5, (0+255+0+255)/4=0.5, (0+0+255+0)/4=0.25, A=1.0
    CREATE_SIMT_VEC4(expected, SPLAT(0.5f), SPLAT(0.5f), SPLAT(0.25f), SPLAT(1.0f));
    ASSERT_NEAR_VEC4(outCol, expected, 1e-3f);
})

TEST(fs_smpl_repeat, "out/sampler.spv", FRAGMENT_SHADER, {
    uint8_t tex_data[2 * 2 * 4] = {
        255,   0,   0, 255, // (0,0) = Red
          0, 255,   0, 255, // (1,0) = Green
          0,   0, 255, 255, // (0,1) = Blue
        255, 255,   0, 255  // (1,1) = Yellow
    };
    TextureSamplerDescriptor desc = {
        .data = tex_data,
        .width = 2,
        .height = 2,
        .channels = 4,
        .filter = FILTER_NEAREST,
        .wrap = WRAP_REPEAT
    };
    CREATE_BINDING(1, desc);

    SimtFloat u = {0};
    SimtFloat v = {0};
    SimtVec4 expected = {0};

    for (int i = 0; i < 4; i++) {
        u[i] = 1.25f; v[i] = 1.25f; // wraps to (0.25, 0.25) -> Red
        expected.elem[0][i] = 1.0f; expected.elem[1][i] = 0.0f; expected.elem[2][i] = 0.0f; expected.elem[3][i] = 1.0f;
    }
    for (int i = 4; i < 8; i++) {
        u[i] = -0.25f; v[i] = 0.25f; // wraps to (0.75, 0.25) -> Green
        expected.elem[0][i] = 0.0f; expected.elem[1][i] = 1.0f; expected.elem[2][i] = 0.0f; expected.elem[3][i] = 1.0f;
    }
    for (int i = 8; i < 12; i++) {
        u[i] = 2.25f; v[i] = -0.25f; // wraps to (0.25, 0.75) -> Blue
        expected.elem[0][i] = 0.0f; expected.elem[1][i] = 0.0f; expected.elem[2][i] = 1.0f; expected.elem[3][i] = 1.0f;
    }
    for (int i = 12; i < 16; i++) {
        u[i] = -1.25f; v[i] = -1.25f; // wraps to (0.75, 0.75) -> Yellow
        expected.elem[0][i] = 1.0f; expected.elem[1][i] = 1.0f; expected.elem[2][i] = 0.0f; expected.elem[3][i] = 1.0f;
    }

    CREATE_SIMT_VEC2(vTexCoord, u, v);
    BIND_IN_LOCATION(0, vTexCoord);

    CREATE_SIMT_VEC4(outCol, SPLAT(0.0), SPLAT(0.0), SPLAT(0.0), SPLAT(0.0));
    BIND_OUT_LOCATION(0, outCol);

    RUN_JIT();

    PRINT_VEC4("outCol:\n", outCol);
    ASSERT_NEAR_VEC4(outCol, expected, 1e-4f);
})

TEST(fs_smpl_clamp, "out/sampler.spv", FRAGMENT_SHADER, {
    uint8_t tex_data[2 * 2 * 4] = {
        255,   0,   0, 255, // (0,0) = Red
          0, 255,   0, 255, // (1,0) = Green
          0,   0, 255, 255, // (0,1) = Blue
        255, 255,   0, 255  // (1,1) = Yellow
    };
    TextureSamplerDescriptor desc = {
        .data = tex_data,
        .width = 2,
        .height = 2,
        .channels = 4,
        .filter = FILTER_NEAREST,
        .wrap = WRAP_CLAMP
    };
    CREATE_BINDING(1, desc);

    SimtFloat u = {0};
    SimtFloat v = {0};
    SimtVec4 expected = {0};

    for (int i = 0; i < 8; i++) {
        u[i] = 2.0f; v[i] = 0.0f; // clamps to (1.0, 0.0) -> Green
        expected.elem[0][i] = 0.0f; expected.elem[1][i] = 1.0f; expected.elem[2][i] = 0.0f; expected.elem[3][i] = 1.0f;
    }
    for (int i = 8; i < 16; i++) {
        u[i] = 0.0f; v[i] = 2.0f; // clamps to (0.0, 1.0) -> Blue
        expected.elem[0][i] = 0.0f; expected.elem[1][i] = 0.0f; expected.elem[2][i] = 1.0f; expected.elem[3][i] = 1.0f;
    }

    CREATE_SIMT_VEC2(vTexCoord, u, v);
    BIND_IN_LOCATION(0, vTexCoord);

    CREATE_SIMT_VEC4(outCol, SPLAT(0.0), SPLAT(0.0), SPLAT(0.0), SPLAT(0.0));
    BIND_OUT_LOCATION(0, outCol);

    RUN_JIT();

    PRINT_VEC4("outCol:\n", outCol);
    ASSERT_NEAR_VEC4(outCol, expected, 1e-4f);
})
TEST(fs_art, "out/art.spv", FRAGMENT_SHADER, {
    struct ubo_t {
        SimtFloat time;
    };
    struct ubo_t ubo;
    ubo.time = SPLAT(1.0f);
    CREATE_BINDING(0, ubo);

    SimtFloat x = SPLAT(320.0f);
    SimtFloat y = SPLAT(240.0f);
    CREATE_SIMT_VEC4(glFragCord, x, y, SPLAT(0.0), SPLAT(0.0));
    SET_GL_FRAGCORD(glFragCord);

    CREATE_SIMT_VEC4(outCol, SPLAT(0.0), SPLAT(0.0), SPLAT(0.0), SPLAT(0.0));
    BIND_OUT_LOCATION(0, outCol);

    RUN_JIT();
    PRINT_VEC4("outCol:\n", outCol);
})
TEST(fs_cord, "out/fs_cord.spv", FRAGMENT_SHADER, {
    RAND_FLOAT(x);
    RAND_FLOAT(y);
    CREATE_SIMT_VEC4(glFragCord, x, y, SPLAT(0.0), SPLAT(0.0));
    PRINT_VEC4("glFragCord:\n", glFragCord);
    SET_GL_FRAGCORD(glFragCord);
    CREATE_SIMT_VEC4(outCol, SPLAT(0.0), SPLAT(0.0), SPLAT(0.0), SPLAT(0.0));
    BIND_OUT_LOCATION(0, outCol);
    RUN_JIT();
    PRINT_VEC4("outCol:\n", outCol);
    CREATE_SIMT_VEC4(expected, x, y, SPLAT(0.0), SPLAT(0.0));
    ASSERT_EQ_VEC4(outCol, expected);
})

TEST(simple_fs, "out/simple_fs.spv", FRAGMENT_SHADER, {
    CREATE_SIMT_VEC3(aCol, SPLAT(1.0), SPLAT(0.021), SPLAT(0.12));
    PRINT_VEC3("aCol:\n", aCol);
    BIND_IN_LOCATION(0, aCol);
    CREATE_SIMT_VEC4(outCol, SPLAT(0.0), SPLAT(0.0), SPLAT(0.0), SPLAT(0.0));
    BIND_OUT_LOCATION(0, outCol);
    RUN_JIT();
    PRINT_VEC4("outCol:\n", outCol);
    CREATE_SIMT_VEC4(expected, SPLAT(1.0), SPLAT(0.021), SPLAT(0.12), SPLAT(1.0));
    ASSERT_EQ_VEC4(outCol, expected);
})

TEST(simple_vs, "out/simple_vs.spv", VERTEX_SHADER, {
    SET_VERTEX_SHADER
    CREATE_SIMT_VEC3(aPos, SPLAT(1.0), SPLAT(2.0), SPLAT(3.0));
    PRINT_VEC3("aPos:\n", aPos);
    BIND_IN_LOCATION(0, aPos);
    RUN_JIT();
    GET_GL_POS();
    PRINT_VEC4("gl_Position:\n", glPos);
    CREATE_SIMT_VEC4(expected, SPLAT(1.0), SPLAT(2.0), SPLAT(3.0), SPLAT(1.0));
    ASSERT_EQ_VEC4(glPos, expected);
})

TEST(vec_math, "out/vec_math.spv", VERTEX_SHADER,  {
    struct ubo_t {
        SimtFloat x;
        SimtFloat y;
    };
    struct ubo_t ubo;
    ubo.x = SPLAT(2.0);
    ubo.y = SPLAT(10.0);

    CREATE_BINDING(0, ubo);
    RUN_JIT();
    GET_GL_POS();
    PRINT_VEC4("gl_Position:\n", glPos);
})

SimtVec4 simt_mat4_mul_vec4(SimtMat4 m, SimtVec4 v)
{
    SimtVec4 result;

    SimtFloat r = m.cols[0][0] * v.elem[0];

    r += m.cols[1][0] * v.elem[1];

    r += m.cols[2][0] * v.elem[2];

    r += m.cols[3][0] * v.elem[3];

    result.elem[0] = r;

    r  = m.cols[0][1] * v.elem[0];
    r += m.cols[1][1] * v.elem[1];
    r += m.cols[2][1] * v.elem[2];
    r += m.cols[3][1] * v.elem[3];
    result.elem[1] = r;

    r  = m.cols[0][2] * v.elem[0];
    r += m.cols[1][2] * v.elem[1];
    r += m.cols[2][2] * v.elem[2];
    r += m.cols[3][2] * v.elem[3];
    result.elem[2] = r;

    r  = m.cols[0][3] * v.elem[0];
    r += m.cols[1][3] * v.elem[1];
    r += m.cols[2][3] * v.elem[2];
    r += m.cols[3][3] * v.elem[3];
    result.elem[3] = r;

    return result;
}

TEST(vec_mvp, "out/mvp.spv", VERTEX_SHADER, {
    struct ubo_t {
        SimtMat4 mvp;
    };
    struct ubo_t ubo;

    ubo.mvp = (SimtMat4){
        RAND_SIMT(), RAND_SIMT(), RAND_SIMT(), RAND_SIMT(),
        RAND_SIMT(), RAND_SIMT(), RAND_SIMT(), RAND_SIMT(),
        RAND_SIMT(), RAND_SIMT(), RAND_SIMT(), RAND_SIMT(),
        RAND_SIMT(), RAND_SIMT(), RAND_SIMT(), RAND_SIMT()
    };
    RAND_FLOAT(x)
    RAND_FLOAT(y)
    RAND_FLOAT(z)
    CREATE_SIMT_VEC3(aPos, x,y,z);
    PRINT_VEC3("aPos:\n", aPos);
    BIND_IN_LOCATION(0, aPos);
    CREATE_BINDING(0, ubo);

    RUN_JIT();
    GET_GL_POS();
    PRINT_VEC4("gl_Position:\n", glPos);
    CREATE_SIMT_VEC4(v, x, y, z, SPLAT(1.0));
    SimtVec4 out = simt_mat4_mul_vec4(ubo.mvp, v);
    PRINT_VEC4("gold: \n", out);
    ASSERT_EQ_VEC4(glPos, out)
})

TEST(cfg_if, "out/cfg_if.spv", VERTEX_SHADER, {
    CREATE_SIMT_VEC3(aPos, SPLAT(1.5), SPLAT(0.0), SPLAT(0.0));
    BIND_IN_LOCATION(0, aPos);
    RUN_JIT();
    GET_GL_POS();
    CREATE_SIMT_VEC4(expected, SPLAT(2.5), SPLAT(0.0), SPLAT(0.0), SPLAT(1.0));
    ASSERT_EQ_VEC4(glPos, expected);
})

TEST(cfg_if_else, "out/cfg_if_else.spv", VERTEX_SHADER, {
    CREATE_SIMT_VEC3(aPos, SPLAT(1.0), SPLAT(2.0), SPLAT(0.0));
    BIND_IN_LOCATION(0, aPos);
    RUN_JIT();
    GET_GL_POS();
    CREATE_SIMT_VEC4(expected, SPLAT(3.0), SPLAT(2.0), SPLAT(0.0), SPLAT(1.0));
    ASSERT_EQ_VEC4(glPos, expected);
})

TEST(cfg_loop, "out/cfg_loop.spv", VERTEX_SHADER, {
    SimtFloat x_vals;
    SimtFloat y_vals;
    SimtFloat z_vals;

    for (uint32_t lane = 0; lane < SIMT_WIDTH; lane++) {
        x_vals[lane] = (float)(1 + lane);
        y_vals[lane] = (float)(2 + lane);
        z_vals[lane] = (float)(3 + lane);
    }

    CREATE_SIMT_VEC3(aPos, x_vals, y_vals, z_vals);
    BIND_IN_LOCATION(0, aPos);
    RUN_JIT();
    GET_GL_POS();

    SimtVec4 expected = {0};
    for (uint32_t lane = 0; lane < SIMT_WIDTH; lane++) {
        expected.elem[0][lane] = 3.0f * x_vals[lane];
        expected.elem[1][lane] = y_vals[lane];
        expected.elem[2][lane] = z_vals[lane];
        expected.elem[3][lane] = 1.0f;
    }
    PRINT_VEC4("glPos: \n", glPos);
    PRINT_VEC4("expected: \n", expected);

    ASSERT_EQ_VEC4(glPos, expected);
})

TEST(cfg_loop_if, "out/cfg_loop_if.spv", VERTEX_SHADER, {
    SimtFloat x_vals;
    SimtFloat y_vals;
    SimtFloat z_vals;

    for (uint32_t lane = 0; lane < SIMT_WIDTH; lane++) {
        x_vals[lane] = (float)(1 + lane);
        y_vals[lane] = (lane % 2 == 0) ? 1.0f : -1.0f;
        z_vals[lane] = (float)(10 + lane);
    }

    CREATE_SIMT_VEC3(aPos, x_vals, y_vals, z_vals);
    BIND_IN_LOCATION(0, aPos);

    RUN_JIT();
    GET_GL_POS();

    SimtVec4 expected = {0};
    for (uint32_t lane = 0; lane < SIMT_WIDTH; lane++) {
        float chosen = (y_vals[lane] > 0.0f) ? x_vals[lane] : z_vals[lane];
        expected.elem[0][lane] = 3.0f * chosen;
        expected.elem[1][lane] = y_vals[lane];
        expected.elem[2][lane] = z_vals[lane];
        expected.elem[3][lane] = 1.0f;
    }
    PRINT_VEC4("glPos: \n", glPos);
    PRINT_VEC4("expected: \n", expected);

    ASSERT_EQ_VEC4(glPos, expected);
})

TEST(compute_vec_add, "out/vec_add.spv", COMPUTE_SHADER, {
    SimtFloat input_a = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f};
    SimtFloat input_b = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f, 110.0f, 120.0f, 130.0f, 140.0f, 150.0f, 160.0f};
    SimtFloat output_c = {0};

    jit_ctx->binding_buffers[0] = &input_a;
    jit_ctx->binding_buffers[1] = &input_b;
    jit_ctx->binding_buffers[2] = &output_c;

    for (int i = 0; i < SIMT_WIDTH; i++) {
        cs_in.gl_GlobalInvocationID.elem[0][i] = (float)i;
        cs_in.gl_LocalInvocationID.elem[0][i] = (float)i;
        cs_in.gl_LocalInvocationIndex[i] = (float)i;
        cs_in.gl_WorkGroupID.elem[0][i] = 0.0f;
        cs_in.gl_NumWorkGroups.elem[0][i] = 1.0f;
        cs_in.gl_WorkGroupSize.elem[0][i] = 16.0f;
    }

    RUN_JIT();

    SimtFloat expected_c = {11.0f, 22.0f, 33.0f, 44.0f, 55.0f, 66.0f, 77.0f, 88.0f, 99.0f, 110.0f, 121.0f, 132.0f, 143.0f, 154.0f, 165.0f, 176.0f};
    for (int i = 0; i < SIMT_WIDTH; i++) 
    {
        if (output_c[i] != expected_c[i]) 
        {
            printf("assert failed at lane %d: %f vs expected %f\n", i, output_c[i], expected_c[i]);
            return 1;
        }
    }
})

TEST(compute_multi_warp, "out/multi_warp.spv", COMPUTE_SHADER, {
    SimtFloat output[4] = {0};
    jit_ctx->active_mask = 0xFFFFu;

    for (uint32_t workgroup = 0; workgroup < 2; workgroup++) 
    {
        for (uint32_t warp = 0; warp < 2; warp++) {
            jit_ctx->binding_buffers[0] = &output[workgroup * 2 + warp];
            for (uint32_t lane = 0; lane < SIMT_WIDTH; lane++)
            {
                cs_in.gl_LocalInvocationIndex[lane] = (float)(warp * SIMT_WIDTH + lane);
                cs_in.gl_GlobalInvocationID.elem[0][lane] = (float)(workgroup * 32 + warp * SIMT_WIDTH + lane);
                cs_in.gl_WorkGroupID.elem[0][lane] = (float)workgroup;
                cs_in.gl_NumWorkGroups.elem[0][lane] = 2.0f;
                cs_in.gl_WorkGroupSize.elem[0][lane] = 32.0f;
                cs_in.gl_SubgroupID[lane] = (float)warp;
            }
            RUN_JIT();
        }
    }

    for (uint32_t i = 0; i < 4; i++)
    {
        if (output[i][0] != 7.0f) 
        {
            printf("multi-warp mismatch at %u: %f != 7\n", i, output[i][0]);
            return 1;
        }
    }
})

TEST(compute_memory_barrier, "out/memory_barrier.spv", COMPUTE_SHADER, {
    SimtUint output[16] = {0};
    uint8_t shared_memory[MAX_SHARED_MEM_SIZE] = {0};
    uint8_t spill_buffer[2048] = {0};
    jit_ctx->binding_buffers[0] = output;
    jit_ctx->shared_memory = shared_memory;
    jit_ctx->spill_buffer = spill_buffer;
    jit_ctx->active_mask = 0xFFFFu;

    for (uint32_t lane = 0; lane < SIMT_WIDTH; lane++) 
    {
        cs_in.gl_LocalInvocationIndex[lane] = (float)lane;
        cs_in.gl_GlobalInvocationID.elem[0][lane] = (float)lane;
        cs_in.gl_WorkGroupSize.elem[0][lane] = 16.0f;
    }

    for (uint32_t phase = 0; phase < ctx.shader_info.barrier_count + 1; phase++) 
    {
        jit_ctx->current_phase = phase;
        RUN_JIT();
    }

})

TEST(compute_barrier_reduction, "out/barrier_reduction.spv", COMPUTE_SHADER, {
    SimtFloat input_data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f};
    SimtFloat output_data = {0};
    uint8_t shared_mem[MAX_SHARED_MEM_SIZE] = {0};
    uint8_t spill_buffer[2048] = {0};

    jit_ctx->binding_buffers[0] = &input_data;
    jit_ctx->binding_buffers[1] = &output_data;
    jit_ctx->shared_memory = shared_mem;
    jit_ctx->spill_buffer = spill_buffer;

    for (int i = 0; i < SIMT_WIDTH; i++) {
        cs_in.gl_GlobalInvocationID.elem[0][i] = (float)i;
        cs_in.gl_LocalInvocationID.elem[0][i] = (float)i;
        cs_in.gl_LocalInvocationIndex[i] = (float)i;
        cs_in.gl_WorkGroupID.elem[0][i] = 0.0f;
        cs_in.gl_NumWorkGroups.elem[0][i] = 1.0f;
        cs_in.gl_WorkGroupSize.elem[0][i] = 16.0f;
    }

    uint32_t num_phases = ctx.shader_info.barrier_count + 1;
    printf("Shader has %u barriers -> %u phases\n", ctx.shader_info.barrier_count, num_phases);
    fflush(stdout);

    for (uint32_t phase = 0; phase < num_phases; phase++)
    {
        printf("Executing phase %u...\n", phase);
        fflush(stdout);
        jit_ctx->current_phase = phase;
        RUN_JIT();
    }

    float expected_sum = 136.0f;
    printf("[Barrier Reduction Test] Output Sum = %.1f (Expected %.1f)... ", output_data[0], expected_sum);
    if (output_data[0] != expected_sum)
    {
        printf("assert failed: output_data[0] = %f vs expected %f\n", output_data[0], expected_sum);
        return 1;
    }
})

TEST(compute_atomics, "out/atomic_test.spv", COMPUTE_SHADER, {
    SimtInt in_data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    struct {
        int32_t counter;
        int32_t max_val;
    } counter_buf = {0, 0};
    SimtInt out_prev = {0};

    jit_ctx->binding_buffers[0] = &in_data;
    jit_ctx->binding_buffers[1] = &counter_buf;
    jit_ctx->binding_buffers[2] = &out_prev;

    for (int i = 0; i < SIMT_WIDTH; i++) {
        cs_in.gl_GlobalInvocationID.elem[0][i] = (float)i;
        cs_in.gl_LocalInvocationID.elem[0][i] = (float)i;
        cs_in.gl_LocalInvocationIndex[i] = (float)i;
        cs_in.gl_WorkGroupID.elem[0][i] = 0.0f;
        cs_in.gl_NumWorkGroups.elem[0][i] = 1.0f;
        cs_in.gl_WorkGroupSize.elem[0][i] = 16.0f;
    }

    RUN_JIT();

    int expected_sum = 16 * 17 / 2; // 136
    printf("[Atomics Test] Total sum = %d (Expected %d), Max = %d (Expected 16)... ", counter_buf.counter, expected_sum, counter_buf.max_val);
    if (counter_buf.counter != expected_sum) {
        printf("FAILED (counter %d != %d)\n", counter_buf.counter, expected_sum);
        return 1;
    }
    if (counter_buf.max_val != 16) {
        printf("FAILED (max_val %d != 16)\n", counter_buf.max_val);
        return 1;
    }
})

TEST(compute_subgroups, "out/subgroup_test.spv", COMPUTE_SHADER, {
    SimtFloat in_data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f};
    struct {
        float sum_reduced;
        float ballot_bits;
        float elect_val;
        float pad;
        float shuffle_val[16];
    } out_buf = {0};

    jit_ctx->binding_buffers[0] = &in_data;
    jit_ctx->binding_buffers[1] = &out_buf;

    for (int i = 0; i < SIMT_WIDTH; i++) {
        cs_in.gl_GlobalInvocationID.elem[0][i] = (float)i;
        cs_in.gl_LocalInvocationID.elem[0][i] = (float)i;
        cs_in.gl_LocalInvocationIndex[i] = (float)i;
        cs_in.gl_WorkGroupID.elem[0][i] = 0.0f;
        cs_in.gl_NumWorkGroups.elem[0][i] = 1.0f;
        cs_in.gl_WorkGroupSize.elem[0][i] = 16.0f;
    }

    RUN_JIT();

    float expected_sum = 136.0f;
    printf("[Subgroup Test] sum = %.1f (Expected %.1f), elect = %.1f (Expected 42.0), ballot = 0x%X... ",
           out_buf.sum_reduced, expected_sum, out_buf.elect_val, (uint32_t)out_buf.ballot_bits);

    if (out_buf.sum_reduced != expected_sum) {
        printf("FAILED (sum != 136.0)\n");
        return 1;
    }
    if (out_buf.elect_val != 42.0f) {
        printf("FAILED (elect_val != 42.0)\n");
        return 1;
    }
    uint32_t expected_ballot = 0xFFE0;
    if ((uint32_t)out_buf.ballot_bits != expected_ballot) {
        printf("FAILED (ballot 0x%X != 0x%X)\n", (uint32_t)out_buf.ballot_bits, expected_ballot);
        return 1;
    }
    for (int i = 0; i < 16; i++) {
        float expected_shuf = 16.0f - (float)i;
        if (out_buf.shuffle_val[i] != expected_shuf) {
            printf("FAILED (shuffle[%d] = %f != %f)\n", i, out_buf.shuffle_val[i], expected_shuf);
            return 1;
        }
    }
})

TEST(compute_image_store, "out/image_store_test.spv", COMPUTE_SHADER, {
    uint8_t image_pixels[4 * 4 * 4] = {0};
    TextureSamplerDescriptor desc = {
        .data = image_pixels,
        .width = 4,
        .height = 4,
        .channels = 4,
        .filter = FILTER_NEAREST,
        .wrap = WRAP_CLAMP
    };
    jit_ctx->binding_buffers[0] = &desc;

    for (int i = 0; i < SIMT_WIDTH; i++) {
        cs_in.gl_GlobalInvocationID.elem[0][i] = (float)i;
        cs_in.gl_LocalInvocationID.elem[0][i] = (float)i;
        cs_in.gl_LocalInvocationIndex[i] = (float)i;
        cs_in.gl_WorkGroupID.elem[0][i] = 0.0f;
        cs_in.gl_NumWorkGroups.elem[0][i] = 1.0f;
        cs_in.gl_WorkGroupSize.elem[0][i] = 16.0f;
    }

    RUN_JIT();

    for (int i = 0; i < 16; i++) {
        int x = i % 4;
        int y = i / 4;
        int idx = (y * 4 + x) * 4;
        uint8_t expected_r = (uint8_t)((float)x / 4.0f * 255.0f + 0.5f);
        uint8_t expected_g = (uint8_t)((float)y / 4.0f * 255.0f + 0.5f);
        uint8_t expected_b = 255;
        uint8_t expected_a = 255;

        if (image_pixels[idx + 0] != expected_r ||
            image_pixels[idx + 1] != expected_g ||
            image_pixels[idx + 2] != expected_b ||
            image_pixels[idx + 3] != expected_a) {
            printf("FAILED pixel (%d, %d): got (%d, %d, %d, %d) expected (%d, %d, %d, %d)\n",
                   x, y,
                   image_pixels[idx + 0], image_pixels[idx + 1], image_pixels[idx + 2], image_pixels[idx + 3],
                   expected_r, expected_g, expected_b, expected_a);
            return 1;
        }
    }
})

TEST(compute_bitwise, "out/bitwise_test.spv", COMPUTE_SHADER, {
    SimtInt in_a = {0x0F, 0xF0, 0xAA, 0x55, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048};
    SimtInt in_b = {0xFF, 0xFF, 0x55, 0xAA, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048};
    struct {
        SimtInt out_and;
        SimtInt out_or;
        SimtInt out_xor;
        SimtInt out_shl;
    } out_buf = {0};

    jit_ctx->binding_buffers[0] = &in_a;
    jit_ctx->binding_buffers[1] = &in_b;
    jit_ctx->binding_buffers[2] = &out_buf;

    for (int i = 0; i < SIMT_WIDTH; i++) {
        cs_in.gl_GlobalInvocationID.elem[0][i] = (float)i;
        cs_in.gl_LocalInvocationID.elem[0][i] = (float)i;
        cs_in.gl_LocalInvocationIndex[i] = (float)i;
        cs_in.gl_WorkGroupID.elem[0][i] = 0.0f;
        cs_in.gl_NumWorkGroups.elem[0][i] = 1.0f;
        cs_in.gl_WorkGroupSize.elem[0][i] = 16.0f;
    }

    RUN_JIT();

    for (int i = 0; i < SIMT_WIDTH; i++) {
        int expected_and = in_a[i] & in_b[i];
        int expected_or  = in_a[i] | in_b[i];
        int expected_xor = in_a[i] ^ in_b[i];
        int expected_shl = in_a[i] << 2;

        if (out_buf.out_and[i] != expected_and ||
            out_buf.out_or[i]  != expected_or  ||
            out_buf.out_xor[i] != expected_xor ||
            out_buf.out_shl[i] != expected_shl) {
            printf("FAILED bitwise at lane %d\n", i);
            return 1;
        }
    }
})

static int test_image_write_formats(void)
{
    for (uint32_t channels = 1; channels <= 4; channels++) {
        uint8_t pixels[2 * 2 * 4] = {0};
        TextureSamplerDescriptor desc = {
            .data = pixels,
            .width = 2,
            .height = 2,
            .channels = channels,
            .filter = FILTER_NEAREST,
            .wrap = WRAP_CLAMP
        };
        int32_t x[SIMT_WIDTH] = {0};
        int32_t y[SIMT_WIDTH] = {0};
        float red[SIMT_WIDTH], green[SIMT_WIDTH], blue[SIMT_WIDTH], alpha[SIMT_WIDTH];
        for (uint32_t lane = 0; lane < SIMT_WIDTH; lane++) {
            x[lane] = lane % 2;
            y[lane] = (lane / 2) % 2;
            red[lane] = 0.25f;
            green[lane] = 0.5f;
            blue[lane] = 0.75f;
            alpha[lane] = 1.0f;
        }
        int32_t mask[SIMT_WIDTH];
        for (uint32_t lane = 0; lane < SIMT_WIDTH; lane++) mask[lane] = 1;
        image_write_2d_simt(&desc, x, y, red, green, blue, alpha, mask);

        for (uint32_t pixel = 0; pixel < 4; pixel++) {
            uint32_t offset = pixel * channels;
            if (pixels[offset] != 64 || (channels > 1 && pixels[offset + 1] != 128) ||
                (channels > 2 && pixels[offset + 2] != 191) ||
                (channels > 3 && pixels[offset + 3] != 255)) {
                printf("image channel test failed for %u channels at pixel %u\n", channels, pixel);
                return 1;
            }
        }
    }
    return 0;
}

__attribute__((constructor)) static void register_image_write_formats(void)
{
    register_test("image_write_formats", test_image_write_formats);
}

static int test_malformed_spirv(void)
{
    JitContext ctx = {0};
    uint32_t bad_magic[5] = {0, 0, 0, 1, 0};
    uint32_t truncated[1] = {0x07230203u};
    init_jit(&ctx, COMPUTE_SHADER);
    if (jit_compile_spirv(&ctx, bad_magic, 5) != NULL ||
        jit_compile_spirv(&ctx, truncated, 1) != NULL) {
        printf("malformed SPIR-V was accepted\n");
        free_jit(&ctx);
        return 1;
    }
    free_jit(&ctx);
    return 0;
}

__attribute__((constructor)) static void register_malformed_spirv(void)
{
    register_test("malformed_spirv", test_malformed_spirv);
}

int run_compilation_script(void) 
{
    printf("--- Running compilation script ---\n");
    
    int raw_status = system("python3 test.py");
    
    if (WIFEXITED(raw_status)) 
    {
        int exit_code = WEXITSTATUS(raw_status);
        if (exit_code != 0) 
        {
            printf("Error: test.py failed with exit code %d. Aborting tests.\n", exit_code);
            return 1;
        }
        return 0;
    } 
    
    printf("Error: Python script was interrupted or failed to run entirely.\n");
    return 1;
}


void run_test_suite(void) 
{
    printf("--- Starting Test Runner ---\n");
    
    TestNode* current = test_list_head;
    int test_count = 0;
    int passed = 0;
    
    while (current != NULL) 
    {
        printf("[%d] Running %s... ", test_count + 1, current->name);
        fflush(stdout);
        int res = current->func();
        if (res == 0) {
            printf("PASSED\n");
            passed++;
        } else {
            printf("FAILED\n");
        }
        
        TestNode* temp = current;
        current = current->next;
        free(temp);
        
        test_count++;
    }
    
    printf("-----------------------------------\n");
    printf("---- %d tests passed out of %d ----\n", passed, test_count);
    printf("-----------------------------------\n");

    
}

int main(void)
{
    if (run_compilation_script() == 1) 
    {
        return 1; 
    }
    srand((unsigned int)time(NULL));

    run_test_suite();
    return 0;
}