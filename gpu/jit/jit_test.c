#include "jit.h"
#include <stdio.h> 
#include <stdlib.h>
#include <math.h>
#include "test.h"

const float epsilon = 1e-6f;


int main(int argc, char *argv[]) 
{
    if(argc != 3) 
    {
        fprintf(stderr, "Usage: %s <spirv_binary> <memory.bin>\n", argv[0]);
        return 1;
    }
    
    const char* spirv_path = argv[1];
    const char* memory_path = argv[2];

    

    FILE* f = fopen(spirv_path, "rb");
    if (!f)
    {
        fprintf(stderr, "Failed to open SPIR-V file: %s\n", spirv_path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint32_t* spirv_code = malloc(file_size);
    if (fread(spirv_code, 1, file_size, f) != (size_t)file_size)
    {
        fprintf(stderr, "Failed to read SPIR-V file: %s\n", spirv_path);
        free(spirv_code);
        fclose(f);
        return 1;
    }
    fclose(f);

    printf("SPIR-V file '%s' loaded successfully (%zu bytes)\n", spirv_path, file_size);
    printf("Compiling SPIR-V to native code...\n");
    
    JitContext ctx = {0};
    init_jit(&ctx);
    jitted_func_t my_func = jit_compile_spirv(&ctx, spirv_code, file_size / 4);
    

    ExecutionContextMap loaded_map = {0};
    if (load_context_map_from_file(&loaded_map, memory_path) != 0)
    {
        printf("can not load test memory: %s",memory_path);
        exit(1);
    }

    ExecutionContext* ectx = parse_context_map_to_context(&loaded_map);

    ExecutionContext *jit_ctx = get_ectx_from_mcjit(&ctx);
    memcpy(jit_ctx, ectx, sizeof(ExecutionContext));

    float expected_output[SIMT_WIDTH];
    float *out_0 = jit_ctx->location_out_buffers[0];
    for(uint32_t i = 0; i < 16; i++)
    {
        expected_output[i] = out_0[i];
        out_0[i] = 0.0f;
    }
    my_func();

    float* output = jit_ctx->location_out_buffers[0];

    printf("Execution Results:\n");
    for(int i = 0; i < SIMT_WIDTH; i++) 
    {
        printf(" Lane %d: %f\n", i, (output)[i]);
    }
    
    free_jit(&ctx);
    printf("Compilation finished\n");
    
 

   
    free(spirv_code);

    for(uint32_t i = 0; i < SIMT_WIDTH; i++)
    {
        if(fabs(output[i] - expected_output[i]) > epsilon) 
        {
            printf("Test failed at lane %d: Expected %f, Got %f\n", i, expected_output[i], output[i]);
            for(uint32_t j = 0; j < SIMT_WIDTH; j++)
            {
                printf(" Lane %d: Expected %f, Got %f\n", j, expected_output[j], output[j]);
            }
            return 1;
        }
    }
    
    printf("Test passed! Output matches expected results.\n");
    return 0;
}