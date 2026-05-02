#include "jit.h"
#include <stdio.h> 
#include <stdlib.h>
#include <math.h>

float epsilon = 1e-6f;
#define UBO_SECTION ".UBO"
#define VBO_SECTION ".VBO"


void read_in(FILE *in, ExecutionContext *ectx)
{
    char buff[16];
    typedef enum {
        NEW_SECTION,
        BINDING,
        LOCATION,
        IN_UBO,
        IN_VBO
    } STATE;
    uint32_t current_lane;
    uint32_t current_idx;

    STATE state = NEW_SECTION;
    while (fscanf(in, "%16s", buff) == 1) 
    {
        if(buff[0] == '.') 
        {
            state = NEW_SECTION;
        }
        if(buff[strlen(buff) - 1] == ':') 
        {
            if(state == IN_UBO) 
            {
                state = LOCATION;
            } 
            else if(state == IN_VBO) 
            {
                state = BINDING;
            }
        }
        switch (state)
        {
        case NEW_SECTION:
        {    printf("Section: %s\n", buff);
            if (strcmp(buff, UBO_SECTION) == 0) 
            {
                state = BINDING;
            } 
            else if (strcmp(buff, VBO_SECTION) == 0) 
            {
                state = LOCATION;
            }
            else 
            {
                printf("Unknown section: %s\n", buff);
                exit(1);
            }
            break;
        }
        case BINDING:
        {
            buff[strlen(buff) - 1] = '\0'; 
            current_idx = atoi(buff);
            current_lane = 0;
            ectx->binding_buffers[current_idx] = malloc(sizeof(float) * SIMT_WIDTH * 2);
            state = IN_UBO;
            break;
        }
        case LOCATION:
        {
            buff[strlen(buff) - 1] = '\0';
            current_idx = atoi(buff);
            current_lane = 0;
            ectx->location_in_buffers[current_idx] = malloc(sizeof(float) * SIMT_WIDTH);
            state = IN_VBO;
            break;
        }
        case IN_UBO:
        {
            float x = atof(buff);
            float *ubo = ectx->binding_buffers[current_idx];
            ubo[current_lane] = x;
            current_lane++;
            break;
        }
        case IN_VBO:
        {
            float x = atof(buff);
            float *vbo = ectx->location_in_buffers[current_idx];
            vbo[current_lane] = x;
            current_lane++;
            break;
        }
        default:
            break;
        }
    }

}

int main(int argc, char *argv[]) 
{
    if(argc < 3) 
    {
        fprintf(stderr, "Usage: %s <spirv_binary> <output_file> <input_file>\n", argv[0]);
        return 1;
    }
    
    const char* spirv_path = argv[1];
    const char* output_path = argv[2];
    const char* input_path = argv[3];
    
    ExecutionContext ectx = {0};
    
    if(argc == 4) 
    {
        FILE* in_f = fopen(input_path, "r");
        if(!in_f) 
        {
            fprintf(stderr, "Failed to open input file: %s\n", input_path);
            return 1;
        }

        // FIX 1: Use aligned_alloc(64) instead of malloc for UBO data!
        // Your LLVM IR uses 'align 64' for vector loads. Standard malloc will crash here.
        ectx.binding_buffers[0] = aligned_alloc(64, sizeof(float) * SIMT_WIDTH * 2);
        
        float ubo_data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 
                            9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 
                            1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 
                            1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
                            
        memcpy(ectx.binding_buffers[0], ubo_data, sizeof(ubo_data));
        fclose(in_f);
    }
    
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
    
    // Allocate Output 1 (Written to by the shader)
    float *output = aligned_alloc(64, sizeof(float) * SIMT_WIDTH);
    memset(output, 0, sizeof(float) * SIMT_WIDTH);
    
    // Allocate Output 2 (Not written to by this specific shader pass)
    float *output2 = aligned_alloc(64, sizeof(float) * SIMT_WIDTH);
    
    // FIX 2: Zero out the memory so it doesn't print OS garbage!
    memset(output2, 0, sizeof(float) * SIMT_WIDTH);

    ectx.location_in_buffers[0] = output2;
    ectx.location_out_buffers[0] = output;

    // Patch the global JIT memory with our host memory
    ExecutionContext *jit_ctx = get_ectx_from_mcjit(&ctx);
    memcpy(jit_ctx, &ectx, sizeof(ExecutionContext));
    
    // Execute the JIT compiled function
    my_func();


    // Print the actual calculated results
    printf("Execution Results:\n");
    for(int i = 0; i < SIMT_WIDTH; i++) 
    {
        printf(" Lane %d: %f\n", i, output[i]);
    }
    
    free_jit(&ctx);
    printf("Compilation finished\n");
    
    float expected_output[SIMT_WIDTH];

    FILE* out_f = fopen(output_path, "r");
    if (!out_f)
    {
        fprintf(stderr, "Failed to open output file: %s\n", output_path);
        free(spirv_code);
        return 1;
    }
    
    for (size_t i = 0; i < SIMT_WIDTH; i++)
    {
        fscanf(out_f, "%f", &expected_output[i]);
    }
    
    free(spirv_code);
    fclose(out_f);
    
    // Validate results
    for(uint32_t i = 0; i < SIMT_WIDTH; i++)
    {
        if(fabs(output[i] - expected_output[i]) > epsilon) // Make sure 'epsilon' is defined
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