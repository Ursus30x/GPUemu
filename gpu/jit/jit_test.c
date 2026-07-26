#include "test.h"
#include <sys/wait.h>
#include <time.h>

TEST(fs_art, "out/art.spv", FRAGMENT_SHADER, {
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

TEST(vec_mvp, "out/mvp.spv", VERTEX_SHADER,({
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
    
}))

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
        passed += (current->func() == 0);
        
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