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
    for (int i = 0; i < SIMT_WIDTH; i++) {
        if (output_c[i] != expected_c[i]) {
            printf("assert failed at lane %d: %f vs expected %f\n", i, output_c[i], expected_c[i]);
            return 1;
        }
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

    for (uint32_t phase = 0; phase < num_phases; phase++) {
        printf("Executing phase %u...\n", phase);
        fflush(stdout);
        jit_ctx->current_phase = phase;
        RUN_JIT();
    }

    float expected_sum = 136.0f;
    printf("[Barrier Reduction Test] Output Sum = %.1f (Expected %.1f)... ", output_data[0], expected_sum);
    if (output_data[0] != expected_sum) {
        printf("assert failed: output_data[0] = %f vs expected %f\n", output_data[0], expected_sum);
        return 1;
    }
})

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