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
    STATE_ID_TEXTURE_CONFIG
} StateID;

typedef struct __attribute__((packed)) {
    uint32_t data_vram_addr; // VRAM offset where pixel bytes start
    uint32_t width;
    uint32_t height;
    uint32_t channels;       // 1, 2, 3, 4
    uint32_t filter;         // 0: FILTER_NEAREST, 1: FILTER_LINEAR
    uint32_t wrap;           // 0: WRAP_REPEAT,    1: WRAP_CLAMP
} GpuTextureDescriptorVram;

typedef struct __attribute__((packed)) {
    uint32_t binding_slot;   // 1..MAX_BINDINGS-1
    uint32_t desc_vram_addr; // VRAM offset of GpuTextureDescriptorVram
} SetTexturePayload;


typedef enum {
    PRIMITIVE_TYPE_POINTS  = 0x01, //TO-DO IN FUTURE
    PRIMITIVE_TYPE_LINES   = 0x02,
    PRIMITIVE_TYPE_TRIANGLES = 0x03, // TO-DO IN FUTURE
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