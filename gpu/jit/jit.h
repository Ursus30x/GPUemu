#ifndef JIT_H
#define JIT_H

#include "gpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>

#include "spirv_jit_meta.h"

#define SIMT_WIDTH 16
#define MAX_BINDINGS 8
#define MAX_ATTRIBUTES 8

typedef struct {
    uint8_t* uniform_buffers[MAX_BINDINGS];
    uint8_t* vertex_buffers[MAX_ATTRIBUTES];
    uint32_t vertex_stride[MAX_ATTRIBUTES];
    uint32_t base_vertex_index;
} ExecutionContext;

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
    int32_t matrix_stride;
    struct MemberDecoNode* next;
} MemberDecoNode;


typedef struct {
    SpvOp opcode;           
    uint32_t base_type_id;  
    uint32_t* member_types;
    uint32_t member_count;
} SpvTypeInfo;

typedef struct {
    uint32_t bound;
    uint8_t* type_kind_map;
    
    LLVMValueRef* id_val_map; 

    SpvDecoInfo* decorations;           // ID -> Decorations
    MemberDecoNode** member_decorations;// ID -> List of Member Decorations
    SpvTypeInfo* type_info;             // ID -> Type Structure Info
    
    LLVMContextRef context;
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    LLVMValueRef func;
    LLVMValueRef out_ptr_arg;
    LLVMValueRef env_arg;     


    LLVMValueRef emask;

    LLVMValueRef lane_ids;    
    LLVMTypeRef float_type;
    LLVMTypeRef int_type;
    LLVMTypeRef i1_type;
    LLVMTypeRef vec_float_type;
    LLVMTypeRef vec_i1_type;
    LLVMTypeRef int8_type;
    LLVMTypeRef ptr_type; 

    
} JitContext;


LLVMValueRef get_val(JitContext* ctx, uint32_t id);

void set_val(JitContext* ctx, uint32_t id, LLVMValueRef val);
void jit_emit_instr(JitContext* ctx, uint16_t opcode, uint32_t res_id, uint32_t type_id, uint32_t* operands, int operand_count);
void jit_compile_spirv(uint32_t* binary, size_t word_count);
void build_masked_store(JitContext* ctx, LLVMValueRef val_to_store, LLVMValueRef ptr, LLVMValueRef mask);


#endif