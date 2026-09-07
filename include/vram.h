#ifndef GPU_VRAM_H
#define GPU_VRAM_H

#include <stdint.h>

#define MAX_UNIFORMS_PER_SHADER  16
#define MAX_ATTRIBUTES_PER_SHADER 8

typedef enum {
    D_TYPE_FLOAT = 1,
    D_TYPE_VEC2,
    D_TYPE_VEC3,
    D_TYPE_VEC4,
    D_TYPE_MAT4,
    D_TYPE_UINT32
} DataType;

typedef struct {
    uint32_t addr;
    uint32_t size;
    DataType element_type;
} GenericBufferConfig;


typedef struct {
    DataType data_type;
    uint32_t offset;
} AttributeMap;

typedef struct __attribute__((packed)) {
    DataType data_type;
    uint32_t offset_in_buffer;
} ShaderResourceMap;



typedef struct __attribute__((packed)) {
    uint32_t shader_type;
    uint32_t num_instructions;

    uint32_t num_uniforms;
    ShaderResourceMap uniform_map[MAX_UNIFORMS_PER_SHADER];

    uint32_t num_attributes;
    AttributeMap attribute_map[MAX_ATTRIBUTES_PER_SHADER];

} Shader_Header;

typedef struct
{
    Shader_Header header_section;
    uint32_t code_section;
} Shader;



typedef enum {
    CMD_NOOP               = 0x00,
    CMD_DRAW_PRIMITIVE     = 0x01,
    CMD_SET_STATE          = 0x02,
    CMD_CLEAR_FRAMEBUFFER  = 0x03,
    CMD_DMA_TRANSFER       = 0x04,
} CommandOpcode;


typedef enum {
    STATE_ID_VBO_CONFIG = 1,
    STATE_ID_EDGE_CONFIG,
    STATE_ID_UNIFORM_CONFIG,
    STATE_ID_VERTEX_SHADER_PTR,
    STATE_ID_FRAGMENT_SHADER_PTR,
    STATE_ID_TEXTURE_CONFIG,
    STATE_ID_BLEND_CONFIG,
    STATE_ID_DEPTH_CONFIG
} StateID;
#define MAX_MIP_LEVELS 14
typedef struct __attribute__((packed)) {
    uint32_t data_vram_addr;          // VRAM offset of Mip level 0 pixel data
    uint32_t mip_vram_addr[MAX_MIP_LEVELS]; // VRAM relative offsets for Mip levels 0..13
    uint32_t width;                   // Base width (Level 0)
    uint32_t height;                  // Base height (Level 0)
    uint32_t depth;                   // Base depth (Level 0, set to 1 for 2D)
    uint32_t channels;                // 1, 2, 3, 4
    uint32_t dimension;               // 0: 2D, 1: 3D (GpuTextureDimension)
    uint32_t filter;
    uint32_t wrap;                     
    uint32_t wrap_u;                  // WrapMode for U/S
    uint32_t wrap_v;                  // WrapMode for V/T
    uint32_t wrap_w;                  // WrapMode for W/R (3D volume depth axis)
    uint32_t num_mip_levels;          // Number of mipmap levels present (1 = base level only)
    float    max_anisotropy;          // Max anisotropic ratio (1.0f = disabled, up to 16.0f)
    float    min_lod;                 // Minimum clamp for LOD (e.g. 0.0f)
    float    max_lod;                 // Maximum clamp for LOD (e.g. 13.0f)
    float    lod_bias;                // User LOD bias (added to computed LOD)
} GpuTextureDescriptorVram;
typedef struct __attribute__((packed)) {
    uint32_t binding_slot;   // 1..MAX_BINDINGS-1
    uint32_t desc_vram_addr; // VRAM offset of GpuTextureDescriptorVram
} SetTexturePayload;


typedef struct __attribute__((packed)) {
    uint8_t  enable;
    uint8_t  src_factor;
    uint8_t  dst_factor;
    uint8_t  reserved;
} SetBlendPayload;

typedef struct __attribute__((packed)) {
    uint8_t  depth_test_enable;
    uint8_t  depth_write_enable;
    uint16_t reserved;
} SetDepthPayload;


typedef enum {
    PRIMITIVE_TYPE_POINTS         = 0x01,
    PRIMITIVE_TYPE_LINES          = 0x02,
    PRIMITIVE_TYPE_LINE_STRIP     = 0x03,
    PRIMITIVE_TYPE_TRIANGLES      = 0x04,
    PRIMITIVE_TYPE_TRIANGLE_STRIP = 0x05,
    PRIMITIVE_TYPE_TRIANGLE_FAN   = 0x06,
    PRIMITIVE_TYPE_QUADS          = 0x07,
} PrimitiveType;


typedef struct __attribute__((packed)) {
    PrimitiveType type;
} DrawPrimitivePayload;

typedef struct __attribute__((packed)) {
    StateID state_id;

    union {
        GenericBufferConfig buffer_config;
        struct __attribute__((packed)) {
            uint32_t vs_addr;
            uint32_t fs_addr;
        } shader_ptrs;
        SetTexturePayload texture_config;
        SetDepthPayload depth_config;
        SetBlendPayload blend_config;
    } value;
} SetStatePayload;

typedef struct __attribute__((packed)) {
    uint8_t options; // TO-DO in future
} ClearFramebufferPayload;

typedef struct __attribute__((packed)) {
    uint32_t host_addr;
    uint32_t vram_offset;
    uint32_t size;
    uint32_t cmd;
} DmaTransferPayload;

typedef struct __attribute__((packed)) {
    CommandOpcode opcode;

    union {
        DrawPrimitivePayload draw;
        SetStatePayload state;
        ClearFramebufferPayload clear;
        DmaTransferPayload dma;
        uint32_t raw_data[8];
    } payload;
} Command;



//ring buffer helpers (debug)

#define CMD_BEGIN() \
    uint32_t current_offset = 0; \
    size_t cmd_size = sizeof(Command);

#define CMD_DBG_END(offset) \
    gpu->ring_buffer_head = offset + current_offset;\
    gpu->ring_buffer_tail = offset;

#define CMD_CLEAR_FB(ring_buffer_base) \
{ \
    Command cmd1 = { \
        .opcode = CMD_CLEAR_FRAMEBUFFER, \
        .payload.clear = { \
            .options = 0b11}}; \
    memcpy(ring_buffer_base + current_offset, &cmd1, cmd_size); \
    current_offset += cmd_size; \
}

#define CMD_SET_VBO(ring_buffer_base, conf) \
{ \
   Command cmd1 = { \
        .opcode = CMD_SET_STATE,\
        .payload.state = { \
            .state_id = STATE_ID_VBO_CONFIG, \
            .value.buffer_config = conf}}; \
    memcpy(ring_buffer_base + current_offset, &cmd1, cmd_size); \
    current_offset += cmd_size; \
}

#define CMD_SET_EDGE(ring_buffer_base, conf) \
{ \
   Command cmd1 = { \
        .opcode = CMD_SET_STATE,\
        .payload.state = { \
            .state_id = STATE_ID_EDGE_CONFIG, \
            .value.buffer_config = conf}}; \
    memcpy(ring_buffer_base + current_offset, &cmd1, cmd_size); \
    current_offset += cmd_size; \
}
#define CMD_SET_UBO(ring_buffer_base, conf) \
{ \
   Command cmd1 = { \
        .opcode = CMD_SET_STATE,\
        .payload.state = { \
            .state_id = STATE_ID_UNIFORM_CONFIG, \
            .value.buffer_config = conf}}; \
    memcpy(ring_buffer_base + current_offset, &cmd1, cmd_size); \
    current_offset += cmd_size; \
}

#define CMD_DRAW_WIREFRAME(ring_buffer_base) \
{ \
   Command cmd1 = { \
        .opcode = CMD_DRAW_PRIMITIVE, \
        .payload.draw = { \
            .type = PRIMITIVE_TYPE_LINES}}; \
    memcpy(ring_buffer_base + current_offset, &cmd1, cmd_size); \
    current_offset += cmd_size; \
}

#define CMD_DRAW_TRIANGLES(ring_buffer_base) \
{ \
   Command cmd1 = { \
        .opcode = CMD_DRAW_PRIMITIVE, \
        .payload.draw = { \
            .type = PRIMITIVE_TYPE_TRIANGLES}}; \
    memcpy(ring_buffer_base + current_offset, &cmd1, cmd_size); \
    current_offset += cmd_size; \
}

#define CMD_SET_SHADERS(ring_buffer_base, vs, fs) \
{ \
    Command cmd1 = { \
        .opcode = CMD_SET_STATE,  \
        .payload.state = { \
            .state_id = STATE_ID_SHADER_PTRS, \
            .value.shader_ptrs = { \
                .fs_addr = fs, \
                .vs_addr = vs \
            } \
    }}; \
    memcpy(ring_buffer_base + current_offset, &cmd1, cmd_size); \
    current_offset += cmd_size; \
}

#define CMD_SET_TEXTURE(ring_buffer_base, slot, desc_addr) \
{ \
    Command cmd1 = { \
        .opcode = CMD_SET_STATE, \
        .payload.state = { \
            .state_id = STATE_ID_TEXTURE_CONFIG, \
            .value.texture_config = { \
                .binding_slot = slot, \
                .desc_vram_addr = desc_addr \
            }}}; \
    memcpy(ring_buffer_base + current_offset, &cmd1, cmd_size); \
    current_offset += cmd_size; \
}
#endif