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
        passed += (current->func() == 0);
        
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