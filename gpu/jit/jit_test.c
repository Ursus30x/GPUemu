#include "jit.h"
#include <stdio.h> 
#include <stdlib.h>
#include <math.h>

float epsilon = 1e-6f;

int main(int argc, char *argv[]) 
{
    if(argc != 3) 
    {
        fprintf(stderr, "Usage: %s <spirv_binary> <output_file>\n", argv[0]);
        return 1;
    }
    const char* spirv_path = argv[1];
    const char* output_path = argv[2];
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
    float *output;
    printf("SPIR-V file '%s' loaded successfully (%zu bytes)\n", spirv_path, file_size);
    printf("Compiling SPIR-V to native code...\n");
    output = jit_compile_spirv(spirv_code, file_size / 4);
    printf("Compilation finished'\n");
    
    float expected_output[SIMT_WIDTH];

    FILE* out_f = fopen(output_path, "r");
    if (!out_f)
    {
        fprintf(stderr, "Failed to open output file: %s\n", output_path);
        return 1;
    }
    
    for (size_t i = 0; i < SIMT_WIDTH; i++)
    {
        fscanf(out_f, "%f", &expected_output[i]);
    }
    free(spirv_code);
    fclose(out_f);
    for(uint32_t i = 0; i < SIMT_WIDTH; i++)
    {
        if(fabs(output[i] - expected_output[i]) > epsilon) 
        {
            printf("Test failed at lane %d: Expected %f, Got %f\n", i, expected_output[i], output[i]);
            for(uint32_t i = 0; i < SIMT_WIDTH; i++)
            {
                printf(" Lane %d: Expected %f, Got %f\n", i, expected_output[i], output[i]);
            }
            return 1;
        }
    }
    printf("Test passed! Output matches expected results.\n");
    return 0;
}