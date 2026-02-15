#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>

#include "spirv_jit_meta.h"
#include "jit.h"
#include "debug_gpu.h"

LLVMValueRef get_val(JitContext* ctx, uint32_t id) 
{
    return ctx->id_val_map[id];
}

void set_val(JitContext* ctx, uint32_t id, LLVMValueRef val) 
{
    ctx->id_val_map[id] = val;
}

static inline void handle_op_constant(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands) {
    uint8_t kind = ctx->type_kind_map[type_id];
    LLVMValueRef const_val;
    
    if (kind == SpvOpTypeFloat) {
        float val = *(float*)&operands[0];
        const_val = LLVMConstReal(ctx->float_type, val);
    } else {
        int32_t val = *(int32_t*)&operands[0];
        const_val = LLVMConstInt(ctx->int_type, (unsigned long long)val, 1);
    }

    // Creating the SIMT vector
    LLVMValueRef* vals = malloc(sizeof(LLVMValueRef) * SIMT_WIDTH);
    for (int i = 0; i < SIMT_WIDTH; i++) {
        vals[i] = const_val;
    }

    LLVMValueRef vec_val = LLVMConstVector(vals, SIMT_WIDTH);
    free(vals);
    
    set_val(ctx, res_id, vec_val);
}

static inline void handle_op_fadd(JitContext* ctx, uint32_t res_id, uint32_t* operands) {
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildFAdd(ctx->builder, lhs, rhs, "v_fadd");
    set_val(ctx, res_id, res);
}

static inline void handle_op_fmul(JitContext* ctx, uint32_t res_id, uint32_t* operands) {
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildFMul(ctx->builder, lhs, rhs, "v_fmul");
    set_val(ctx, res_id, res);
}

static inline void handle_op_fdiv(JitContext* ctx, uint32_t res_id, uint32_t* operands)
 {
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildFDiv(ctx->builder, lhs, rhs, "v_fdiv");
    set_val(ctx, res_id, res);
}

static inline void handle_op_return_value(JitContext* ctx, uint32_t* operands)
 {
    LLVMValueRef vec_val = get_val(ctx, operands[0]);
    
    LLVMTypeRef vec_type = LLVMVectorType(ctx->float_type, SIMT_WIDTH);
    LLVMTypeRef vec_ptr_type = LLVMPointerType(vec_type, 0);
    LLVMValueRef bitcast_ptr = LLVMBuildBitCast(ctx->builder, ctx->out_ptr_arg, vec_ptr_type, "vec_ptr_cast");
    
    LLVMBuildStore(ctx->builder, vec_val, bitcast_ptr);
    LLVMBuildRetVoid(ctx->builder);
}

void jit_emit_instr(JitContext* ctx, uint16_t opcode, uint32_t res_id, uint32_t type_id, uint32_t* operands, int operand_count) 
{
    
    switch (opcode) {
        case SpvOpConstant:
            handle_op_constant(ctx, res_id, type_id, operands);
            break;

        case SpvOpFAdd:
            handle_op_fadd(ctx, res_id, operands);
            break;

        case SpvOpFMul:
            handle_op_fmul(ctx, res_id, operands);
            break;

        case SpvOpFDiv:
            handle_op_fdiv(ctx, res_id, operands);
            break;

        case SpvOpReturnValue:
            handle_op_return_value(ctx, operands);
            break;

        default:
            DEBUG_PRINT("Unhandled opcode %d in JIT emitter\n", opcode);
            break;
        }
}

void jit_compile_spirv(uint32_t* binary, size_t word_count) 
{
    uint32_t* p = binary + 5;
    uint32_t* end = binary + word_count;
    
    JitContext ctx;
    ctx.bound = binary[3];
    ctx.type_kind_map = (uint8_t*)calloc(ctx.bound, sizeof(uint8_t));

    ctx.id_val_map = (LLVMValueRef*)calloc(ctx.bound, sizeof(LLVMValueRef));

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();

    ctx.context = LLVMContextCreate();
    ctx.module = LLVMModuleCreateWithNameInContext("spirv_vector_module", ctx.context);
    ctx.builder = LLVMCreateBuilderInContext(ctx.context);

    // Create function: void main_simt(float* out_ptr)
    LLVMTypeRef param_types[] = { LLVMPointerType(LLVMFloatTypeInContext(ctx.context), 0) };
    LLVMTypeRef func_type = LLVMFunctionType(LLVMVoidTypeInContext(ctx.context), param_types, 1, 0);
    ctx.func = LLVMAddFunction(ctx.module, "main_simt", func_type);
    ctx.out_ptr_arg = LLVMGetParam(ctx.func, 0);

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx.context, ctx.func, "entry");
    LLVMPositionBuilderAtEnd(ctx.builder, entry);

    DEBUG_PRINT("--- Starting JIT Compilation (LLVM Vector Backend) ---\n");

    ctx.float_type = LLVMFloatTypeInContext(ctx.context);
    ctx.int_type = LLVMInt32TypeInContext(ctx.context);

    while (p < end) {
        uint32_t instruction = p[0];
        uint16_t opcode = instruction & 0xFFFF;
        uint16_t inst_word_count = instruction >> 16;

        if (opcode == SpvOpTypeFloat || opcode == SpvOpTypeInt) {
            ctx.type_kind_map[p[1]] = (uint8_t)opcode;
        }

        if (opcode == SpvOpSource || opcode == SpvOpName || opcode == SpvOpMemberName || 
            opcode == SpvOpLine || opcode == SpvOpNoLine) {
            p += inst_word_count;
            continue;
        }

        SpvOpMeta meta = SPV_META[opcode];
        uint32_t type_id = 0, res_id = 0;
        int current_idx = 1;

        if (meta.has_result_type) type_id = p[current_idx++];
        if (meta.has_result_id) res_id = p[current_idx++];

        uint32_t operands[16];
        int op_count = 0;
        while (current_idx < inst_word_count && op_count < 16) {
            operands[op_count++] = p[current_idx++];
        }

        jit_emit_instr(&ctx, opcode, res_id, type_id, operands, op_count);
        p += inst_word_count;
    }

    char *error = NULL;
    LLVMVerifyModule(ctx.module, LLVMAbortProcessAction, &error);
    LLVMDisposeMessage(error);

    LLVMExecutionEngineRef engine;
    if (LLVMCreateExecutionEngineForModule(&engine, ctx.module, &error) != 0) {
        DEBUG_PRINT("Failed to create execution engine: %s\n", error);
        return;
    }

    typedef void (*jitted_simt_func_t)(float*);
    jitted_simt_func_t my_simt_func = (jitted_simt_func_t)LLVMGetFunctionAddress(engine, "main_simt");


    //TO-DO only for debugging, remove later
    // Align the buffer to 64 bytes for AVX-512 compatibility
    float *results = aligned_alloc(64, sizeof(float) * SIMT_WIDTH);
    memset(results, 0, sizeof(float) * SIMT_WIDTH);
    
    my_simt_func(results);

    DEBUG_PRINT("Execution Results:\n");
    for(int i=0; i<SIMT_WIDTH; i++) DEBUG_PRINT(" Lane %d: %f\n", i, results[i]);

    free(results);
    free(ctx.type_kind_map);
    free(ctx.id_val_map);
    LLVMDisposeBuilder(ctx.builder);
    LLVMDisposeExecutionEngine(engine);
    LLVMContextDispose(ctx.context);
}
