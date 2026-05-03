#include  "jit.h"
typedef enum Type {
    I32,
    F32,
    Vec3,
    Vec4,
    Mat3,
    Mat4
};

#define MAX_UBO_ELEMENTS 10
#define MAX_ELEM_SIZE (SIMT_WIDTH * 4 * 4)
#define MAX_UBO_SIZE (MAX_UBO_ELEMENTS * 4 * 4)
// typedef struct UBO_Type
// {
//     uint32_t elements;
//     Type types[MAX_UBO_ELEMENTS];

// };

typedef struct {
    //memory map 1 - is present 0 - is not
    uint32_t binding_buffers_map[MAX_BINDINGS];
    uint32_t location_in_buffers_map[MAX_ATTRIBUTES];
    uint32_t location_out_buffers_map[MAX_ATTRIBUTES];

    //offsets of each location / binding
    uint32_t binding_buffers_offset[MAX_BINDINGS];
    uint32_t location_in_buffers_offset[MAX_ATTRIBUTES];
    uint32_t location_out_buffers_offset[MAX_ATTRIBUTES];

    //size of each location / binding
    uint32_t binding_buffers_size[MAX_BINDINGS];
    uint32_t location_in_buffers_size[MAX_ATTRIBUTES];
    uint32_t location_out_buffers_size[MAX_ATTRIBUTES];

    uint32_t binding_buffers[MAX_BINDINGS * MAX_UBO_SIZE]; 
    uint32_t location_in_buffers[MAX_ATTRIBUTES * MAX_ELEM_SIZE];
    uint32_t location_out_buffers[MAX_ATTRIBUTES * MAX_ELEM_SIZE];
} ExecutionContextMap;

// Parse ExecutionContextMap to ExecutionContext (with relative addresses)
// Splats binding buffer values across SIMT_WIDTH lanes
ExecutionContext* parse_context_map_to_context(const ExecutionContextMap* map)
{
    ExecutionContext* ctx = (ExecutionContext*)malloc(sizeof(ExecutionContext));
    if (!ctx) return NULL;
    
    for (uint32_t i = 0; i < MAX_BINDINGS; i++)
    {
        if (map->binding_buffers_map[i])
        {
           ctx->binding_buffers[i] = (void*)(uintptr_t)(map->binding_buffers_offset[i] + map->binding_buffers);
        } 
        else 
        {
            ctx->binding_buffers[i] = NULL;
        }
    }
    
    for (uint32_t i = 0; i < MAX_ATTRIBUTES; i++)
    {
        if (map->location_in_buffers_map[i])
        {
            // Location buffers already contain per-lane data
            ctx->location_in_buffers[i] = (void*)(uintptr_t)(map->location_in_buffers_offset[i] + map->location_in_buffers);
        } else {
            ctx->location_in_buffers[i] = NULL;
        }
        
        if (map->location_out_buffers_map[i])
        {
            // Location buffers already contain per-lane data
            ctx->location_out_buffers[i] = (void*)(uintptr_t)(map->location_out_buffers_offset[i] + map->location_out_buffers);
        } else {
            ctx->location_out_buffers[i] = NULL;
        }
    }
    
    return ctx;
}

// Print ExecutionContext with values
void print_execution_context(const ExecutionContext* ctx)
{
    if (!ctx)
    {
        DEBUG_PRINT("ExecutionContext is NULL\n");
        return;
    }
    
    DEBUG_PRINT("=== ExecutionContext ===\n");
    DEBUG_PRINT("Binding Buffers:\n");
    for (uint32_t i = 0; i < MAX_BINDINGS; i++)
    {
        if (ctx->binding_buffers[i])
        {
            DEBUG_PRINT("  [%d] offset = 0x%lx\n", i, (uintptr_t)ctx->binding_buffers[i]);
        }
    }
    
    DEBUG_PRINT("Input Location Buffers:\n");
    for (uint32_t i = 0; i < MAX_ATTRIBUTES; i++)
    {
        if (ctx->location_in_buffers[i])
        {
            DEBUG_PRINT("  [%d] offset = 0x%lx\n", i, (uintptr_t)ctx->location_in_buffers[i]);
        }
    }
    
    DEBUG_PRINT("Output Location Buffers:\n");
    for (uint32_t i = 0; i < MAX_ATTRIBUTES; i++)
    {
        if (ctx->location_out_buffers[i])
        {
            DEBUG_PRINT("  [%d] offset = 0x%lx\n", i, (uintptr_t)ctx->location_out_buffers[i]);
        }
    }
    
    DEBUG_PRINT("========================\n");
}

// Print ExecutionContextMap with actual values
void print_execution_context_map(const ExecutionContextMap* map)
{
    if (!map)
    {
        DEBUG_PRINT("ExecutionContextMap is NULL\n");
        return;
    }
    
    DEBUG_PRINT("=== ExecutionContextMap ===\n");
    
    DEBUG_PRINT("Binding Buffers (used):\n");
    for (uint32_t i = 0; i < MAX_BINDINGS; i++)
    {
        if (map->binding_buffers_map[i])
        {
            DEBUG_PRINT("  [%d] offset=0x%x size=%u lanes=%d values (lane 0 & 15): ", 
                       i, map->binding_buffers_offset[i], map->binding_buffers_size[i],
                       map->binding_buffers_size[i] / (4 * sizeof(uint32_t)));
            uint32_t* data = (uint32_t*)&map->binding_buffers[i * MAX_UBO_SIZE];
            int vals_per_lane = map->binding_buffers_size[i] / (SIMT_WIDTH * sizeof(uint32_t));
            
            // Show lane 0
            for (int j = 0; j < vals_per_lane && j < 4; j++)
            {
                DEBUG_PRINT("0x%08x ", data[j]);
            }
            DEBUG_PRINT("| ");
            // Show lane 15
            for (int j = 0; j < vals_per_lane && j < 4; j++)
            {
                DEBUG_PRINT("0x%08x ", data[15 * vals_per_lane + j]);
            }
            DEBUG_PRINT("\n");
        }
    }
    
    DEBUG_PRINT("Input Location Buffers (used):\n");
    for (uint32_t i = 0; i < MAX_ATTRIBUTES; i++)
    {
        if (map->location_in_buffers_map[i])
        {
            DEBUG_PRINT("  [%d] offset=0x%x size=%u values: ", i, map->location_in_buffers_offset[i], map->location_in_buffers_size[i]);
            uint32_t* data = (uint32_t*)&map->location_in_buffers[i * MAX_ELEM_SIZE];
            for (int j = 0; j < 4 && j < map->location_in_buffers_size[i] / sizeof(uint32_t); j++)
            {
                DEBUG_PRINT("0x%08x ", data[j]);
            }
            DEBUG_PRINT("\n");
        }
    }
    
    DEBUG_PRINT("Output Location Buffers (used):\n");
    for (uint32_t i = 0; i < MAX_ATTRIBUTES; i++)
    {
        if (map->location_out_buffers_map[i])
        {
            DEBUG_PRINT("  [%d] offset=0x%x size=%u values: ", i, map->location_out_buffers_offset[i], map->location_out_buffers_size[i]);
            uint32_t* data = (uint32_t*)&map->location_out_buffers[i * MAX_ELEM_SIZE];
            for (int j = 0; j < 4 && j < map->location_out_buffers_size[i] / sizeof(uint32_t); j++)
            {
                DEBUG_PRINT("0x%08x ", data[j]);
            }
            DEBUG_PRINT("\n");
        }
    }
    
    DEBUG_PRINT("============================\n");
}

// Save ExecutionContextMap to binary file
int save_context_map_to_file(const ExecutionContextMap* map, const char* filename)
{
    if (!map || !filename)
    {
        DEBUG_PRINT("Error: Invalid parameters for save\n");
        return -1;
    }
    
    FILE* file = fopen(filename, "wb");
    if (!file)
    {
        DEBUG_PRINT("Error: Failed to open file %s for writing\n", filename);
        return -1;
    }
    
    // Write metadata (maps and offsets/sizes)
    if (fwrite(map->binding_buffers_map, sizeof(uint32_t), MAX_BINDINGS, file) != MAX_BINDINGS)
    {
        DEBUG_PRINT("Error: Failed to write binding_buffers_map\n");
        fclose(file);
        return -1;
    }
    
    if (fwrite(map->location_in_buffers_map, sizeof(uint32_t), MAX_ATTRIBUTES, file) != MAX_ATTRIBUTES)
    {
        DEBUG_PRINT("Error: Failed to write location_in_buffers_map\n");
        fclose(file);
        return -1;
    }
    
    if (fwrite(map->location_out_buffers_map, sizeof(uint32_t), MAX_ATTRIBUTES, file) != MAX_ATTRIBUTES)
    {
        DEBUG_PRINT("Error: Failed to write location_out_buffers_map\n");
        fclose(file);
        return -1;
    }
    
    if (fwrite(map->binding_buffers_offset, sizeof(uint32_t), MAX_BINDINGS, file) != MAX_BINDINGS)
    {
        DEBUG_PRINT("Error: Failed to write binding_buffers_offset\n");
        fclose(file);
        return -1;
    }
    
    if (fwrite(map->location_in_buffers_offset, sizeof(uint32_t), MAX_ATTRIBUTES, file) != MAX_ATTRIBUTES)
    {
        DEBUG_PRINT("Error: Failed to write location_in_buffers_offset\n");
        fclose(file);
        return -1;
    }
    
    if (fwrite(map->location_out_buffers_offset, sizeof(uint32_t), MAX_ATTRIBUTES, file) != MAX_ATTRIBUTES)
    {
        DEBUG_PRINT("Error: Failed to write location_out_buffers_offset\n");
        fclose(file);
        return -1;
    }
    
    if (fwrite(map->binding_buffers_size, sizeof(uint32_t), MAX_BINDINGS, file) != MAX_BINDINGS)
    {
        DEBUG_PRINT("Error: Failed to write binding_buffers_size\n");
        fclose(file);
        return -1;
    }
    
    if (fwrite(map->location_in_buffers_size, sizeof(uint32_t), MAX_ATTRIBUTES, file) != MAX_ATTRIBUTES)
    {
        DEBUG_PRINT("Error: Failed to write location_in_buffers_size\n");
        fclose(file);
        return -1;
    }
    
    if (fwrite(map->location_out_buffers_size, sizeof(uint32_t), MAX_ATTRIBUTES, file) != MAX_ATTRIBUTES)
    {
        DEBUG_PRINT("Error: Failed to write location_out_buffers_size\n");
        fclose(file);
        return -1;
    }
    
    // Write buffer data
    if (fwrite(map->binding_buffers, sizeof(uint32_t), MAX_BINDINGS * MAX_UBO_SIZE, file) != MAX_BINDINGS * MAX_UBO_SIZE)
    {
        DEBUG_PRINT("Error: Failed to write binding_buffers\n");
        fclose(file);
        return -1;
    }
    
    if (fwrite(map->location_in_buffers, sizeof(uint32_t), MAX_ATTRIBUTES * MAX_ELEM_SIZE, file) != MAX_ATTRIBUTES * MAX_ELEM_SIZE)
    {
        DEBUG_PRINT("Error: Failed to write location_in_buffers\n");
        fclose(file);
        return -1;
    }
    
    if (fwrite(map->location_out_buffers, sizeof(uint32_t), MAX_ATTRIBUTES * MAX_ELEM_SIZE, file) != MAX_ATTRIBUTES * MAX_ELEM_SIZE)
    {
        DEBUG_PRINT("Error: Failed to write location_out_buffers\n");
        fclose(file);
        return -1;
    }
    
    fclose(file);
    DEBUG_PRINT("Successfully saved context map to %s\n", filename);
    return 0;
}

// Load ExecutionContextMap from binary file
int load_context_map_from_file(ExecutionContextMap* map, const char* filename)
{
    if (!map || !filename)
    {
        DEBUG_PRINT("Error: Invalid parameters for load\n");
        return -1;
    }
    
    FILE* file = fopen(filename, "rb");
    if (!file)
    {
        DEBUG_PRINT("Error: Failed to open file %s for reading\n", filename);
        return -1;
    }
    
    // Read metadata (maps and offsets/sizes)
    if (fread(map->binding_buffers_map, sizeof(uint32_t), MAX_BINDINGS, file) != MAX_BINDINGS)
    {
        DEBUG_PRINT("Error: Failed to read binding_buffers_map\n");
        fclose(file);
        return -1;
    }
    
    if (fread(map->location_in_buffers_map, sizeof(uint32_t), MAX_ATTRIBUTES, file) != MAX_ATTRIBUTES)
    {
        DEBUG_PRINT("Error: Failed to read location_in_buffers_map\n");
        fclose(file);
        return -1;
    }
    
    if (fread(map->location_out_buffers_map, sizeof(uint32_t), MAX_ATTRIBUTES, file) != MAX_ATTRIBUTES)
    {
        DEBUG_PRINT("Error: Failed to read location_out_buffers_map\n");
        fclose(file);
        return -1;
    }
    
    if (fread(map->binding_buffers_offset, sizeof(uint32_t), MAX_BINDINGS, file) != MAX_BINDINGS)
    {
        DEBUG_PRINT("Error: Failed to read binding_buffers_offset\n");
        fclose(file);
        return -1;
    }
    
    if (fread(map->location_in_buffers_offset, sizeof(uint32_t), MAX_ATTRIBUTES, file) != MAX_ATTRIBUTES)
    {
        DEBUG_PRINT("Error: Failed to read location_in_buffers_offset\n");
        fclose(file);
        return -1;
    }
    
    if (fread(map->location_out_buffers_offset, sizeof(uint32_t), MAX_ATTRIBUTES, file) != MAX_ATTRIBUTES)
    {
        DEBUG_PRINT("Error: Failed to read location_out_buffers_offset\n");
        fclose(file);
        return -1;
    }
    
    if (fread(map->binding_buffers_size, sizeof(uint32_t), MAX_BINDINGS, file) != MAX_BINDINGS)
    {
        DEBUG_PRINT("Error: Failed to read binding_buffers_size\n");
        fclose(file);
        return -1;
    }
    
    if (fread(map->location_in_buffers_size, sizeof(uint32_t), MAX_ATTRIBUTES, file) != MAX_ATTRIBUTES)
    {
        DEBUG_PRINT("Error: Failed to read location_in_buffers_size\n");
        fclose(file);
        return -1;
    }
    
    if (fread(map->location_out_buffers_size, sizeof(uint32_t), MAX_ATTRIBUTES, file) != MAX_ATTRIBUTES)
    {
        DEBUG_PRINT("Error: Failed to read location_out_buffers_size\n");
        fclose(file);
        return -1;
    }
    
    // Read buffer data
    if (fread(map->binding_buffers, sizeof(uint32_t), MAX_BINDINGS * MAX_UBO_SIZE, file) != MAX_BINDINGS * MAX_UBO_SIZE)
    {
        DEBUG_PRINT("Error: Failed to read binding_buffers\n");
        fclose(file);
        return -1;
    }
    
    if (fread(map->location_in_buffers, sizeof(uint32_t), MAX_ATTRIBUTES * MAX_ELEM_SIZE, file) != MAX_ATTRIBUTES * MAX_ELEM_SIZE)
    {
        DEBUG_PRINT("Error: Failed to read location_in_buffers\n");
        fclose(file);
        return -1;
    }
    
    if (fread(map->location_out_buffers, sizeof(uint32_t), MAX_ATTRIBUTES * MAX_ELEM_SIZE, file) != MAX_ATTRIBUTES * MAX_ELEM_SIZE)
    {
        DEBUG_PRINT("Error: Failed to read location_out_buffers\n");
        fclose(file);
        return -1;
    }
    
    fclose(file);
    DEBUG_PRINT("Successfully loaded context map from %s\n", filename);
    return 0;
}

// Test example with splatting
void test_context_parsing()
{
    DEBUG_PRINT("Starting context parsing test...\n");
    
    ExecutionContextMap map = {0};
    
    // Save to file
    const char* test_file = "./spirv/spirv.bin";
     ExecutionContextMap loaded_map = {0};
        if (load_context_map_from_file(&loaded_map, test_file) == 0)
        {
            DEBUG_PRINT("\n=== LOADED MAP ===\n");
            print_execution_context_map(&loaded_map);
            
            // Parse loaded map to context
            ExecutionContext* ctx = parse_context_map_to_context(&loaded_map);

            if (ctx)
            {
                DEBUG_PRINT("\n=== PARSED CONTEXT ===\n");
                //print_execution_context(ctx);

                for(uint32_t i = 0; i < 16; i++)
                {
                    struct u {
                        float x;
                        float y;
                    };
                    struct u l = ((struct u*)ctx->binding_buffers[0])[i];
                    printf("ubo lane: %d x = %f y = %f\n", i, l.x, l.y);
                }
                for(uint32_t i = 0; i < 16; i++)
                {

                    float o = ((float*)ctx->location_out_buffers[0])[i];
                    printf("out lane: %d %f\n", i, o);
                }
                free(ctx);
                DEBUG_PRINT("Test completed successfully!\n");
            }
        }
}