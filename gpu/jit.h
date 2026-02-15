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

typedef struct {
    uint32_t bound;
    uint8_t* type_kind_map;
    
    LLVMValueRef* id_val_map; 
    
    LLVMContextRef context;
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    LLVMValueRef func;
    LLVMValueRef out_ptr_arg;

    LLVMValueRef emask;

    LLVMTypeRef float_type;
    LLVMTypeRef int_type;
    LLVMTypeRef i1_type;
    LLVMTypeRef vec_float_type;
    LLVMTypeRef vec_i1_type;
    
} JitContext;


LLVMValueRef get_val(JitContext* ctx, uint32_t id);

void set_val(JitContext* ctx, uint32_t id, LLVMValueRef val);
void jit_emit_instr(JitContext* ctx, uint16_t opcode, uint32_t res_id, uint32_t type_id, uint32_t* operands, int operand_count);
void jit_compile_spirv(uint32_t* binary, size_t word_count);
void build_masked_store(JitContext* ctx, LLVMValueRef val_to_store, LLVMValueRef ptr, LLVMValueRef mask);


#endif