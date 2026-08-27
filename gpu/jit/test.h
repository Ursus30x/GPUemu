#define JIT
#include "debug_gpu.h"
#include "jit.h"
#include "jit_smpl.h"

#include <stdio.h> 
#include <stdlib.h>
#include <math.h>


#define SPLAT(value) (SimtFloat){value, value, value, value, value, value, value, value, value, value, value, value, value, value, value, value}
#define CREATE_SIMT_F32(name, values) SimtFloat name = values;
#define CREATE_SIMT_VEC3(name, a, b, c) SimtVec3 name = { .elem = { a,  b,  c } };
#define CREATE_SIMT_VEC4(name, a, b, c, d) SimtVec4 name = { .elem = { a,  b,  c, d } };
#define CREATE_SIMT_VEC2(name, a, b) SimtVec2 name = { .elem = { a,  b } };


#define SIMT_VEC4(a, b, c, d) (SimtVec4){ a,  b,  c, d };
#define CREATE_MAT4(name, elems) SimtFloat name = (SimtFloat) elems;
#define RAND_FLOAT(name) \
    SimtFloat name; \
    for(uint32_t i = 0; i < SIMT_WIDTH; i++) \
    { \
        name[i] = 2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f; \
    }

#define SET_FRAGMENT_SHADER ctx.shader_type = FRAGMENT_SHADER;
#define SET_VERTEX_SHADER ctx.shader_type = VERTEX_SHADER;

#define RAND_SIMT()  (SimtFloat){ 2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f,  2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f,  2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f,  2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f,  2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f,  2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f,  2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f,  2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f,  2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f,  2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f,  2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f,  2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f,  2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f,  2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f,  2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f,  2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f}

#define BIND_IN_LOCATION(location, value) jit_ctx->location_in_buffers[location] = &value;
#define BIND_OUT_LOCATION(location, value) jit_ctx->location_out_buffers[location] = &value;
#define CREATE_BINDING(attribute, value) jit_ctx->binding_buffers[attribute] = &value;
#define SET_GL_FRAGCORD(c) fs_in.gl_FragCoord = c;
#define GET_GL_POS() SimtVec4 glPos = vs_out.gl_Position;
#define RUN_JIT() func(jit_ctx, &vs_out, &fs_in, &cs_in);

#define PRINT_VEC3(caption, name)  \
    printf("%s", caption); \
    for(uint32_t i = 0; i < SIMT_WIDTH; i++) \
    { \
        printf("lane%d: [%f, %f, %f]\n", i, name.elem[0][i], name.elem[1][i], name.elem[2][i]); \
    }

#define PRINT_VEC4(caption, name)  \
    printf("%s", caption); \
    for(uint32_t i = 0; i < SIMT_WIDTH; i++) \
    { \
        printf("lane%d: [%f, %f, %f, %f]\n", i, name.elem[0][i], name.elem[1][i], name.elem[2][i], name.elem[3][i]); \
    }
#define PRINT_VEC2(caption, name)  \
    printf("%s", caption); \
    for(uint32_t i = 0; i < SIMT_WIDTH; i++) \
    { \
        printf("lane%d: [%f, %f]\n", i, name.elem[0][i], name.elem[1][i]); \
    }


#define ASSERT_EQ_VEC4(val, expected)  \
    for(uint32_t i = 0; i < SIMT_WIDTH; i++) \
    { \
        if(!(val.elem[0][i]==expected.elem[0][i] && val.elem[1][i]==expected.elem[1][i] && val.elem[2][i]==expected.elem[2][i] && val.elem[3][i]==expected.elem[3][i])){ \
            printf("asset failed\n"); \
            return 1; \
        }\
    }
#define ASSERT_NEAR_VEC4(val, expected, eps) \
    for(uint32_t i = 0; i < SIMT_WIDTH; i++) \
    { \
        if(fabsf(val.elem[0][i] - expected.elem[0][i]) > eps || \
           fabsf(val.elem[1][i] - expected.elem[1][i]) > eps || \
           fabsf(val.elem[2][i] - expected.elem[2][i]) > eps || \
           fabsf(val.elem[3][i] - expected.elem[3][i]) > eps) { \
            printf("assert near failed at lane %u: [%f,%f,%f,%f] vs [%f,%f,%f,%f]\n", \
                   i, val.elem[0][i], val.elem[1][i], val.elem[2][i], val.elem[3][i], \
                   expected.elem[0][i], expected.elem[1][i], expected.elem[2][i], expected.elem[3][i]); \
            return 1; \
        } \
    }
#define ASSERT_EQ_MAT4(val, expected)                                  \
    for (uint32_t i = 0; i < SIMT_WIDTH; i++)                          \
    {                                                                   \
        for (uint32_t c = 0; c < 4; c++)                               \
        {                                                               \
            for (uint32_t r = 0; r < 4; r++)                           \
            {                                                           \
                if (val.cols[c][r][i] != expected.cols[c][r][i])       \
                {                                                       \
                    printf("assert failed at col %u row %u lane %u\n", \
                           c, r, i);                                    \
                    return 1;                                           \
                }                                                       \
            }                                                           \
        }                                                               \
    }
const float epsilon = 1e-6f;


typedef int (*TestFunc)(void);

typedef struct TestNode {
    const char* name;
    TestFunc func;
    struct TestNode* next;
} TestNode;
TestNode* test_list_head = NULL;

void register_test(const char* name, TestFunc func)
{
    TestNode* new_node = (TestNode*)malloc(sizeof(TestNode));
    new_node->name = name;
    new_node->func = func;
    new_node->next = test_list_head;
    test_list_head = new_node;
}

#define TEST(test_name, path, shader_type, ...) \
    int test_name(void) \
    { \
        jitted_func_t func; \
        JitContext ctx = {0}; \
        FILE* f = fopen(path, "rb"); \
        if (!f) \
        { \
            fprintf(stderr, "Failed to open SPIR-V file: %s\n", path); \
            return 1; \
        } \
        fseek(f, 0, SEEK_END); \
        size_t file_size = ftell(f); \
        fseek(f, 0, SEEK_SET); \
        uint32_t* spirv_code = malloc(file_size); \
        if (fread(spirv_code, 1, file_size, f) != (size_t)file_size) \
        { \
            fprintf(stderr, "Failed to read SPIR-V file: %s\n",  path); \
            free(spirv_code); \
            fclose(f); \
            return 1; \
        } \
        fclose(f); \
        init_jit(&ctx, shader_type); \
        func = jit_compile_spirv(&ctx, spirv_code, file_size / 4); \
        free(spirv_code); \
        ExecutionContext jit_ctx_storage = {0}; \
        ExecutionContext *jit_ctx = &jit_ctx_storage; \
        BuiltinVertexOutput vs_out = {0}; \
        BuiltinFragmentInput fs_in = {0}; \
        BuiltinComputeInput cs_in = {0}; \
        __VA_ARGS__ ; \
        free_jit(&ctx); \
        printf("Test passed! Output matches expected results.\n"); \
        return 0; \
        \
    } \
    \
    __attribute__((constructor)) void register_##test_name(void) { \
        register_test(#test_name, test_name); \
    }

