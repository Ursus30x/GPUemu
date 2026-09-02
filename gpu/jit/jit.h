#ifndef JIT_H
#define JIT_H
#define G_GNUC_UNUSED  __attribute__ ((__unused__))
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Target.h>

#include <llvm-c/Orc.h>    
#include <llvm-c/LLJIT.h>   
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>

#include "spirv_jit_meta.h"

#define SIMT_WIDTH 16
#define MAX_BINDINGS 8
#define MAX_ATTRIBUTES 8
#define MAX_GLOBALS 32
#define MAX_CONTROL_STACK 16

#define VS_OUT_NAME "gl_PerVertex"

typedef enum {
    FRAGMENT_SHADER,
    VERTEX_SHADER,
    COMPUTE_SHADER
} shader_t;

typedef enum {
    JIT_CFG_NONE = 0,
    JIT_CFG_SELECTION,
    JIT_CFG_LOOP
} JitCfgKind;

typedef struct {
    JitCfgKind kind;
    uint32_t merge_id;
    uint32_t continue_id;
    uint32_t header_id;

    uint32_t true_id;
    uint32_t false_id;
    bool executed_true;
    LLVMValueRef cond;
    LLVMValueRef parent_mask;
} JitControlConstruct;

typedef float SimtFloat __attribute__((vector_size(64)));

typedef struct
{
    SimtFloat elem[4];
} SimtVec4;

typedef struct
{
    SimtFloat elem[3];
} SimtVec3;

typedef struct
{
    SimtFloat elem[4];
} SimtVec2;

typedef struct {
    SimtFloat cols[4][4];
} SimtMat4;

typedef struct {
    SimtVec3 col[3];
} SimtMat3;

typedef struct
{
    SimtVec4 gl_Position;
    SimtFloat gl_PointSize;
    SimtFloat gl_ClipDistance;
    SimtFloat gl_CullDistance;
} BuiltinVertexOutput;

typedef struct
{
    SimtVec4 gl_FragCoord;
    SimtFloat gl_FrontFacing;
    SimtVec4 gl_PointCoord;
    SimtFloat gl_SampleID;
} BuiltinFragmentInput;

typedef struct
{
    SimtVec3 gl_GlobalInvocationID;
    SimtVec3 gl_LocalInvocationID;
    SimtFloat gl_LocalInvocationIndex;
    SimtVec3 gl_WorkGroupID;
    SimtVec3 gl_NumWorkGroups;
    SimtVec3 gl_WorkGroupSize;
} BuiltinComputeInput;

typedef struct {
    void* binding_buffers[MAX_BINDINGS]; 
    void* location_in_buffers[MAX_ATTRIBUTES];
    void* location_out_buffers[MAX_ATTRIBUTES];
    void* shared_memory;
    void* spill_buffer;
    uint32_t current_phase;
} ExecutionContext;

#define MAX_SHARED_MEM_SIZE (16 * 1024)

typedef struct {
    int32_t descriptor_set;
    int32_t binding;
    int32_t location;
    int32_t builtin;
    int32_t array_stride; 
    int is_decorated;
} SpvDecoInfo;

typedef struct MemberDecoNode {
    uint32_t member_index;
    int32_t offset;       
    int32_t buildin;
    int32_t matrix_stride;
    struct MemberDecoNode* next;
} MemberDecoNode;

typedef struct {
    uint32_t res_id;
    uint32_t storage_class;
    int32_t binding_or_loc;
    uint32_t base_type;
} GlobalResolution;

typedef enum {
    DIM_1D,
    DIM_2D,
    DIM_3D,
    DIM_CUBE,
    DIM_RECT,
    DIM_BUFFER,
    DIM_SUBPASS_DATA
} ImageDim;

typedef enum {
    SPV_IMAGE_FORMAT_UNKNOWN,
    SPV_IMAGE_FORMAT_R32F,
    SPV_IMAGE_FORMAT_R32I,
    SPV_IMAGE_FORMAT_R32UI,
} SpvImageFormat;

typedef struct {
    uint32_t sampled_type;       /* Result of Sampled Type */
    ImageDim dim;
    uint32_t depth;              /* 0, 1, or 2 */
    uint32_t arrayed;            /* 0 or 1 */
    uint32_t ms;                 /* 0 or 1 */
    uint32_t sampled;            /* 0, 1, or 2 */
    SpvImageFormat format;

    /* Optional Access Qualifier */
    uint32_t has_access_qualifier;
    uint32_t access_qualifier;
} ImageType;
typedef struct {
    SpvOp opcode;           
    uint32_t base_type_id;  
    uint32_t* member_types;
    uint32_t member_count;
    ImageType image_type;
    struct {
        uint32_t image_type_id;
    } sampled_image;
} SpvTypeInfo;
typedef struct JitContext JitContext;
typedef void (*AluHandler)(JitContext* ctx, uint32_t res_id, uint32_t* operands);

typedef struct {
    int id;
} ShaderInterface;

typedef struct ShaderInfo {
    char entry_point_name[64];
    uint32_t execution_model;
    ShaderInterface interface[MAX_ATTRIBUTES+MAX_BINDINGS];
    uint32_t interface_count;
    uint32_t local_size_x;
    uint32_t local_size_y;
    uint32_t local_size_z;
    uint32_t barrier_count;
} ShaderInfo;



struct JitContext{
    shader_t shader_type;
    uint32_t bound;
    uint8_t* type_kind_map;
    
    LLVMValueRef* id_val_map; 

    SpvDecoInfo* decorations;
    MemberDecoNode** member_decorations;
    SpvTypeInfo* type_info;

    GlobalResolution globals[MAX_GLOBALS];
    uint32_t global_count;

    LLVMContextRef context;
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    LLVMOrcLLJITRef jit;
    
    LLVMValueRef func;
    LLVMValueRef out_ptr_arg;
    LLVMValueRef env_arg;   
    LLVMValueRef vs_data;
    LLVMTypeRef  vs_data_type;   
    LLVMTypeRef  fs_data_type;   
    LLVMTypeRef  cs_data_type;

    LLVMExecutionEngineRef engine;
    LLVMBasicBlockRef current_block;
    
    LLVMValueRef emask;

    LLVMValueRef lane_ids;    
    LLVMTypeRef float_type;
    LLVMTypeRef int_type;
    LLVMTypeRef i1_type;
    LLVMTypeRef vec_float_type;
    LLVMTypeRef vec_i1_type;
    LLVMTypeRef int8_type;
    LLVMTypeRef ptr_type;
    LLVMTypeRef exec_ctx_type;

    /* LLVMValueRefs for entry-function parameters (thread-local state) */
    LLVMValueRef env_arg_param;   /* ExecutionContext* parameter */
    LLVMValueRef vs_data_param;   /* BuiltinVertexOutput* parameter */
    LLVMValueRef fs_data_param; 
    LLVMValueRef cs_data_param;   /* BuiltinComputeInput* parameter */

    LLVMOrcThreadSafeContextRef ts_ctx;

    AluHandler glsl_handlers[82];

    ShaderInfo shader_info;

    char **names;

    JitControlConstruct control_stack[MAX_CONTROL_STACK];
    uint32_t control_stack_depth;

    uint32_t shared_mem_offset;
    uint32_t spill_mem_offset;
    uint32_t barrier_count;
    LLVMValueRef switch_inst;
    LLVMBasicBlockRef phase_bbs[16];
    
};

/* Jitted function now takes pointers to per-invocation state to be thread-safe */
typedef void (*jitted_func_t)(ExecutionContext*, BuiltinVertexOutput*, BuiltinFragmentInput*, BuiltinComputeInput*);

void jit_call_printf(JitContext* ctx, const char* fmt, LLVMValueRef* args, unsigned num_args);
void jit_call_printf_simt(JitContext* ctx, const char* fmt, LLVMValueRef vec_val);
void set_val(JitContext* ctx, uint32_t id, LLVMValueRef val);
void jit_emit_instr(JitContext* ctx, uint16_t opcode, uint32_t res_id, uint32_t type_id, uint32_t* operands, int operand_count);
void build_masked_store(JitContext* ctx, LLVMValueRef val_to_store, LLVMValueRef ptr, LLVMValueRef mask);
void init_jit(JitContext* ctx, shader_t shader_type);
void free_jit(JitContext* ctx);

jitted_func_t jit_compile_spirv(JitContext* ctx, uint32_t* binary, size_t word_count);

LLVMValueRef get_val(JitContext* ctx, uint32_t id);
LLVMTypeRef map_spv_to_llvm_type(JitContext *ctx, uint32_t type_id);
ExecutionContext* get_ectx_from_mcjit(JitContext *ctx);
BuiltinVertexOutput* get_vs_data_from_mcjit(JitContext *ctx);
BuiltinFragmentInput* get_fs_data_from_mcjit(JitContext *ctx);
#endif