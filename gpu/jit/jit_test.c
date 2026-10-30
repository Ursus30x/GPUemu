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

    for (int i = 0; i < 4; i++) 
    {
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

typedef struct {
    float bounding_sphere[4];
    float cone[4];
    uint32_t vertex_offset;
    uint32_t vertex_count;
    uint32_t primitive_offset;
    uint32_t primitive_count;
} TestMeshlet;

typedef struct {
    uint32_t meshlet_ids[16];
} TestTaskPayload;

TEST(task_shader_meshlet_cull, "out/meshlet_cull.spv", TASK_SHADER, {
    TestMeshlet meshlets[16];
    memset(meshlets, 0, sizeof(meshlets));
    for (int i = 0; i < 16; i++) 
    {
        if ((i % 2) == 0) 
        {
            meshlets[i].bounding_sphere[3] = 1.0f;
            meshlets[i].cone[3] = 0.0f;
        } else if ((i % 4) == 1) 
        {
            meshlets[i].bounding_sphere[3] = -1.0f;
            meshlets[i].cone[3] = 0.0f;
        } else {
            meshlets[i].bounding_sphere[3] = 1.0f;
            meshlets[i].cone[3] = 0.8f;
        }
    }

    TestTaskPayload payload;
    memset(&payload, 0xFF, sizeof(payload));

    jit_ctx->binding_buffers[0] = meshlets;
    jit_ctx->task_payload = &payload;
    memset(jit_ctx->task_group_count, 0, sizeof(jit_ctx->task_group_count));

    for (int i = 0; i < SIMT_WIDTH; i++) 
    {
        cs_in.gl_GlobalInvocationID.elem[0][i] = (float)i;
        cs_in.gl_LocalInvocationID.elem[0][i] = (float)i;
        cs_in.gl_LocalInvocationIndex[i] = (float)i;
        cs_in.gl_WorkGroupID.elem[0][i] = 0.0f;
        cs_in.gl_NumWorkGroups.elem[0][i] = 1.0f;
        cs_in.gl_WorkGroupSize.elem[0][i] = 16.0f;
    }

    RUN_JIT();

    if (ctx.shader_info.execution_model != SpvExecutionModelTaskEXT ||
        ctx.shader_info.local_size_x != 16) 
    {
        printf("FAILED task shader metadata: model=%u, local_size_x=%u\n",
               ctx.shader_info.execution_model, ctx.shader_info.local_size_x);
        return 1;
    }

    if (jit_ctx->task_group_count[0] != 8 ||
        jit_ctx->task_group_count[1] != 1 ||
        jit_ctx->task_group_count[2] != 1)
    {
        printf("FAILED EmitMeshTasksEXT counts: (%u, %u, %u) != (8, 1, 1)\n",
               jit_ctx->task_group_count[0],
               jit_ctx->task_group_count[1],
               jit_ctx->task_group_count[2]);
        return 1;
    }

    for (uint32_t i = 0; i < 8; i++) 
    {
        uint32_t expected_id = i * 2;
        if (payload.meshlet_ids[i] != expected_id) 
        {
            printf("FAILED task payload.meshlet_ids[%u] = %u (expected %u)\n",
                   i, payload.meshlet_ids[i], expected_id);
            return 1;
        }
    }
    printf("[Task Shader Test] Emitted %u mesh tasks, compacted IDs: [%u, %u, %u, %u, %u, %u, %u, %u]... ",
           jit_ctx->task_group_count[0],
           payload.meshlet_ids[0], payload.meshlet_ids[1], payload.meshlet_ids[2], payload.meshlet_ids[3],
           payload.meshlet_ids[4], payload.meshlet_ids[5], payload.meshlet_ids[6], payload.meshlet_ids[7]);
})

TEST(mesh_shader_meshlet_gen, "out/meshlet_gen.spv", MESH_SHADER, {
    TestMeshlet meshlets[4];
    memset(meshlets, 0, sizeof(meshlets));
    meshlets[2].vertex_offset = 4;
    meshlets[2].vertex_count = 4;
    meshlets[2].primitive_offset = 1;
    meshlets[2].primitive_count = 2;

    float positions[8][4];
    for (int v = 0; v < 8; v++) 
    {
        positions[v][0] = (float)v * 10.0f;
        positions[v][1] = (float)v * 20.0f;
        positions[v][2] = 0.5f;
        positions[v][3] = 1.0f;
    }

    uint32_t packed_triangles[4] = {
        0,
        0u | (1u << 8) | (2u << 16) | (0u << 24),
        2u | (3u << 8) | (0u << 16) | (1u << 24),
        0
    };

    TestTaskPayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.meshlet_ids[0] = 2;

    BuiltinMeshOutput mesh_out;
    memset(&mesh_out, 0, sizeof(mesh_out));

    jit_ctx->binding_buffers[0] = meshlets;
    jit_ctx->binding_buffers[1] = positions;
    jit_ctx->binding_buffers[2] = packed_triangles;
    jit_ctx->task_payload = &payload;
    jit_ctx->mesh_output = &mesh_out;

    for (int i = 0; i < SIMT_WIDTH; i++) 
    {
        cs_in.gl_GlobalInvocationID.elem[0][i] = (float)i;
        cs_in.gl_LocalInvocationID.elem[0][i] = (float)i;
        cs_in.gl_LocalInvocationIndex[i] = (float)i;
        cs_in.gl_WorkGroupID.elem[0][i] = 0.0f;
        cs_in.gl_NumWorkGroups.elem[0][i] = 1.0f;
        cs_in.gl_WorkGroupSize.elem[0][i] = 16.0f;
    }

    RUN_JIT();

    if (ctx.shader_info.execution_model != SpvExecutionModelMeshEXT ||
        ctx.shader_info.local_size_x != 16 ||
        ctx.shader_info.max_output_vertices != 64 ||
        ctx.shader_info.max_output_primitives != 126 ||
        ctx.shader_info.output_topology != SpvExecutionModeOutputTrianglesEXT) 
    {
        printf("FAILED mesh shader metadata: model=%u, local=%u, max_v=%u, max_p=%u, topo=%u\n",
               ctx.shader_info.execution_model,
               ctx.shader_info.local_size_x,
               ctx.shader_info.max_output_vertices,
               ctx.shader_info.max_output_primitives,
               ctx.shader_info.output_topology);
        return 1;
    }

    if (mesh_out.vertex_count != 4 || mesh_out.primitive_count != 2) 
    {
        printf("FAILED SetMeshOutputsEXT: vertex_count=%u, primitive_count=%u (expected 4, 2)\n",
               mesh_out.vertex_count, mesh_out.primitive_count);
        return 1;
    }

    for (uint32_t v = 0; v < 4; v++) 
    {
        float exp_x = (float)(4 + v) * 10.0f;
        float exp_y = (float)(4 + v) * 20.0f;
        if (mesh_out.vertices[v].position[0] != exp_x ||
            mesh_out.vertices[v].position[1] != exp_y ||
            mesh_out.vertices[v].position[2] != 0.5f ||
            mesh_out.vertices[v].position[3] != 1.0f) 
        {
            printf("FAILED gl_MeshVerticesEXT[%u].gl_Position = (%f, %f, %f, %f)\n",
                   v,
                   mesh_out.vertices[v].position[0],
                   mesh_out.vertices[v].position[1],
                   mesh_out.vertices[v].position[2],
                   mesh_out.vertices[v].position[3]);
            return 1;
        }
        if (mesh_out.per_vertex_out[0][v][0] != 2.0f ||
            mesh_out.per_vertex_out[0][v][1] != (float)v ||
            mesh_out.per_vertex_out[0][v][2] != 1.0f ||
            mesh_out.per_vertex_out[0][v][3] != 1.0f) {
            printf("FAILED out_color[%u] = (%f, %f, %f, %f)\n",
                   v,
                   mesh_out.per_vertex_out[0][v][0],
                   mesh_out.per_vertex_out[0][v][1],
                   mesh_out.per_vertex_out[0][v][2],
                   mesh_out.per_vertex_out[0][v][3]);
            return 1;
        }
    }

    if (mesh_out.triangle_indices[0][0] != 0 ||
        mesh_out.triangle_indices[0][1] != 1 ||
        mesh_out.triangle_indices[0][2] != 2 ||
        mesh_out.primitives[0].primitive_id != 100 ||
        mesh_out.primitives[0].cull_primitive != 0 ||
        mesh_out.per_primitive_out[1][0][0] != 0.0f ||
        mesh_out.per_primitive_out[1][0][1] != 1.0f ||
        mesh_out.per_primitive_out[1][0][2] != 2.0f) {
        printf("FAILED tri[0] = (%u, %u, %u) id=%d cull=%u\n",
               mesh_out.triangle_indices[0][0],
               mesh_out.triangle_indices[0][1],
               mesh_out.triangle_indices[0][2],
               mesh_out.primitives[0].primitive_id,
               mesh_out.primitives[0].cull_primitive);
        return 1;
    }

    if (mesh_out.triangle_indices[1][0] != 2 ||
        mesh_out.triangle_indices[1][1] != 3 ||
        mesh_out.triangle_indices[1][2] != 0 ||
        mesh_out.primitives[1].primitive_id != 101 ||
        mesh_out.primitives[1].cull_primitive != 1 ||
        mesh_out.per_primitive_out[1][1][0] != 2.0f ||
        mesh_out.per_primitive_out[1][1][1] != 3.0f ||
        mesh_out.per_primitive_out[1][1][2] != 0.0f) {
        printf("FAILED tri[1] = (%u, %u, %u) id=%d cull=%u\n",
               mesh_out.triangle_indices[1][0],
               mesh_out.triangle_indices[1][1],
               mesh_out.triangle_indices[1][2],
               mesh_out.primitives[1].primitive_id,
               mesh_out.primitives[1].cull_primitive);
        return 1;
    }

    printf("[Mesh Shader Test] verts=%u, prims=%u, tri0=(%u,%u,%u) cull=%u, tri1=(%u,%u,%u) cull=%u... ",
           mesh_out.vertex_count, mesh_out.primitive_count,
           mesh_out.triangle_indices[0][0], mesh_out.triangle_indices[0][1], mesh_out.triangle_indices[0][2],
           mesh_out.primitives[0].cull_primitive,
           mesh_out.triangle_indices[1][0], mesh_out.triangle_indices[1][1], mesh_out.triangle_indices[1][2],
           mesh_out.primitives[1].cull_primitive);
})

static uint32_t* read_spirv_binary(const char* path, size_t* out_words)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    size_t sz = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint32_t* buf = malloc(sz);
    if (!buf || fread(buf, 1, sz, f) != sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *out_words = sz / 4;
    return buf;
}

static int test_task_to_mesh_pipeline(void)
{
    size_t task_words = 0, mesh_words = 0;
    uint32_t* task_bin = read_spirv_binary("out/meshlet_cull.spv", &task_words);
    uint32_t* mesh_bin = read_spirv_binary("out/meshlet_gen.spv", &mesh_words);
    if (!task_bin || !mesh_bin) {
        free(task_bin);
        free(mesh_bin);
        return 1;
    }

    JitContext task_ctx = {0}, mesh_ctx = {0};
    init_jit(&task_ctx, TASK_SHADER);
    init_jit(&mesh_ctx, MESH_SHADER);

    jitted_func_t task_fn = jit_compile_spirv(&task_ctx, task_bin, task_words);
    jitted_func_t mesh_fn = jit_compile_spirv(&mesh_ctx, mesh_bin, mesh_words);
    free(task_bin);
    free(mesh_bin);

    if (!task_fn || !mesh_fn) {
        free_jit(&task_ctx);
        free_jit(&mesh_ctx);
        return 1;
    }

    TestMeshlet meshlets[16];
    memset(meshlets, 0, sizeof(meshlets));
    for (uint32_t i = 0; i < 16; i++) {
        bool vis = (i == 1 || i == 5 || i == 9);
        meshlets[i].bounding_sphere[3] = vis ? 1.0f : -1.0f;
        meshlets[i].cone[3] = 0.0f;
        meshlets[i].vertex_offset = i * 3;
        meshlets[i].vertex_count = 3;
        meshlets[i].primitive_offset = i;
        meshlets[i].primitive_count = 1;
    }

    float positions[48][4];
    for (int v = 0; v < 48; v++) {
        positions[v][0] = (float)v;
        positions[v][1] = (float)(v * 2);
        positions[v][2] = 0.25f;
        positions[v][3] = 1.0f;
    }

    uint32_t packed_triangles[16];
    for (int p = 0; p < 16; p++) {
        packed_triangles[p] = 0u | (1u << 8) | (2u << 16) | (0u << 24);
    }

    TestTaskPayload payload;
    memset(&payload, 0xFF, sizeof(payload));

    ExecutionContext ectx;
    memset(&ectx, 0, sizeof(ectx));
    ectx.active_mask = 0xFFFFu;
    ectx.binding_buffers[0] = meshlets;
    ectx.binding_buffers[1] = positions;
    ectx.binding_buffers[2] = packed_triangles;
    ectx.task_payload = &payload;

    BuiltinVertexOutput vs_out = {0};
    BuiltinFragmentInput fs_in = {0};
    BuiltinComputeInput cs_in = {0};
    for (int i = 0; i < SIMT_WIDTH; i++) {
        cs_in.gl_GlobalInvocationID.elem[0][i] = (float)i;
        cs_in.gl_LocalInvocationID.elem[0][i] = (float)i;
        cs_in.gl_LocalInvocationIndex[i] = (float)i;
        cs_in.gl_WorkGroupID.elem[0][i] = 0.0f;
        cs_in.gl_NumWorkGroups.elem[0][i] = 1.0f;
        cs_in.gl_WorkGroupSize.elem[0][i] = 16.0f;
    }

    task_fn(&ectx, &vs_out, &fs_in, &cs_in);

    if (ectx.task_group_count[0] != 3 ||
        payload.meshlet_ids[0] != 1 ||
        payload.meshlet_ids[1] != 5 ||
        payload.meshlet_ids[2] != 9) {
        printf("FAILED task->mesh task stage: count=%u, ids=(%u,%u,%u)\n",
               ectx.task_group_count[0],
               payload.meshlet_ids[0], payload.meshlet_ids[1], payload.meshlet_ids[2]);
        free_jit(&task_ctx);
        free_jit(&mesh_ctx);
        return 1;
    }

    uint32_t expected_ids[3] = {1, 5, 9};
    for (uint32_t wg = 0; wg < ectx.task_group_count[0]; wg++) {
        BuiltinMeshOutput mesh_out;
        memset(&mesh_out, 0, sizeof(mesh_out));
        ectx.mesh_output = &mesh_out;
        for (int i = 0; i < SIMT_WIDTH; i++) {
            cs_in.gl_WorkGroupID.elem[0][i] = (float)wg;
        }
        mesh_fn(&ectx, &vs_out, &fs_in, &cs_in);

        uint32_t m_id = expected_ids[wg];
        if (mesh_out.vertex_count != 3 || mesh_out.primitive_count != 1 ||
            mesh_out.vertices[0].position[0] != (float)(m_id * 3) ||
            mesh_out.per_vertex_out[0][0][0] != (float)m_id) {
            printf("FAILED task->mesh wg %u (meshlet %u)\n", wg, m_id);
            free_jit(&task_ctx);
            free_jit(&mesh_ctx);
            return 1;
        }
    }

    printf("[Task->Mesh Pipeline Test] 3 visible meshlets (1, 5, 9) dispatched and expanded... ");
    free_jit(&task_ctx);
    free_jit(&mesh_ctx);
    return 0;
}

__attribute__((constructor)) static void register_task_to_mesh_pipeline(void)
{
    register_test("task_to_mesh_pipeline", test_task_to_mesh_pipeline);
}

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


void test_3d_volume_and_anisotropic(void)
{
    printf("--- Running 3D Volume, LOD Mipmapping & Anisotropic Tests ---\n");

    // 1. Test 3D Volume Texture Sampling (2x2x2)
    uint8_t tex_3d[2 * 2 * 2 * 4];
    for (int z = 0; z < 2; z++) {
        for (int y = 0; y < 2; y++) {
            for (int x = 0; x < 2; x++) {
                int idx = ((z * 2 + y) * 2 + x) * 4;
                tex_3d[idx + 0] = (uint8_t)(x * 255);
                tex_3d[idx + 1] = (uint8_t)(y * 255);
                tex_3d[idx + 2] = (uint8_t)(z * 255);
                tex_3d[idx + 3] = 255;
            }
        }
    }

    TextureSamplerDescriptor desc_3d = {
        .data = tex_3d,
        .width = 2,
        .height = 2,
        .depth = 2,
        .channels = 4,
        .dimension = TEXTURE_DIM_3D,
        .filter = FILTER_LINEAR,
        .wrap_u = WRAP_CLAMP,
        .wrap_v = WRAP_CLAMP,
        .wrap_w = WRAP_CLAMP,
        .num_mip_levels = 1,
        .max_anisotropy = 1.0f
    };

    float u[SIMT_WIDTH] = {0.5f}, v[SIMT_WIDTH] = {0.5f}, w[SIMT_WIDTH] = {0.5f};
    float out_r[SIMT_WIDTH], out_g[SIMT_WIDTH], out_b[SIMT_WIDTH], out_a[SIMT_WIDTH];

    sample_texture_generic_simt(&desc_3d, u, v, w, NULL, NULL, NULL, NULL, NULL, NULL, NULL, out_r, out_g, out_b, out_a);

    printf("[3D Volume Test] Center (0.5, 0.5, 0.5) -> R: %.3f, G: %.3f, B: %.3f (Expected ~0.500)\n", out_r[0], out_g[0], out_b[0]);

    // 2. Test Mipmap LOD selection (Level 0: 4x4 red, Level 1: 2x2 green, Level 2: 1x1 blue)
    uint8_t mip0[4 * 4 * 4], mip1[2 * 2 * 4], mip2[1 * 1 * 4];
    memset(mip0, 0, sizeof(mip0)); for (int i = 0; i < 4*4; i++) { mip0[i*4] = 255; mip0[i*4+3] = 255; }
    memset(mip1, 0, sizeof(mip1)); for (int i = 0; i < 2*2; i++) { mip1[i*4+1] = 255; mip1[i*4+3] = 255; }
    memset(mip2, 0, sizeof(mip2)); for (int i = 0; i < 1*1; i++) { mip2[i*4+2] = 255; mip2[i*4+3] = 255; }

    TextureSamplerDescriptor desc_lod = {
        .data = mip0,
        .width = 4,
        .height = 4,
        .depth = 1,
        .channels = 4,
        .dimension = TEXTURE_DIM_2D,
        .filter = FILTER_LINEAR_MIPMAP_NEAREST,
        .wrap_u = WRAP_REPEAT,
        .wrap_v = WRAP_REPEAT,
        .num_mip_levels = 3,
        .max_anisotropy = 1.0f,
        .min_lod = 0.0f,
        .max_lod = 2.0f
    };
    desc_lod.mip_addr[0] = mip0;
    desc_lod.mip_addr[1] = mip1;
    desc_lod.mip_addr[2] = mip2;

    float exp_lod[SIMT_WIDTH] = {1.0f}; // Explicit LOD 1 => Green
    sample_texture_generic_simt(&desc_lod, u, v, w, NULL, NULL, NULL, NULL, NULL, NULL, exp_lod, out_r, out_g, out_b, out_a);

    printf("[LOD Mipmap Test] Explicit LOD 1.0 -> R: %.3f, G: %.3f, B: %.3f (Expected Green G=1.0)\n", out_r[0], out_g[0], out_b[0]);

    // 3. Test Anisotropic Filtering
    TextureSamplerDescriptor desc_aniso = desc_lod;
    desc_aniso.max_anisotropy = 16.0f;
    desc_aniso.filter = FILTER_LINEAR_MIPMAP_LINEAR;

    float du_dx[SIMT_WIDTH] = {0.25f}; // Oblique stretch
    sample_texture_generic_simt(&desc_aniso, u, v, w, du_dx, NULL, NULL, NULL, NULL, NULL, NULL, out_r, out_g, out_b, out_a);
    printf("[Anisotropic Test] MaxAniso 16.0 -> Sample executed successfully R: %.3f, G: %.3f, B: %.3f\n", out_r[0], out_g[0], out_b[0]);
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

    test_3d_volume_and_anisotropic();
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