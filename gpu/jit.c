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

static inline void handle_op_constant(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands) 
{
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
    for (int i = 0; i < SIMT_WIDTH; i++) 
    {
        vals[i] = const_val;
    }

    LLVMValueRef vec_val = LLVMConstVector(vals, SIMT_WIDTH);
    free(vals);
    
    set_val(ctx, res_id, vec_val);
}

static inline void handle_op_fadd(JitContext* ctx, uint32_t res_id, uint32_t* operands) 
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildFAdd(ctx->builder, lhs, rhs, "v_fadd");
    set_val(ctx, res_id, res);
}

static inline void handle_op_fmul(JitContext* ctx, uint32_t res_id, uint32_t* operands) 
{
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
static inline void handle_op_fsub(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildFSub(ctx->builder, lhs, rhs, "v_fsub");
    set_val(ctx, res_id, res);
}
static inline void handle_op_fneg(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef op = get_val(ctx, operands[0]);
    LLVMValueRef res = LLVMBuildFNeg(ctx->builder, op, "v_fneg");
    set_val(ctx, res_id, res);
}
static inline void handle_op_isub(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildSub(ctx->builder, lhs, rhs, "v_isub");
    set_val(ctx, res_id, res);
}
static inline void handle_op_imul(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildMul(ctx->builder, lhs, rhs, "v_imul");
    set_val(ctx, res_id, res);
}
static inline void handle_op_sdiv(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildSDiv(ctx->builder, lhs, rhs, "v_sdiv");
    set_val(ctx, res_id, res);
}
static inline void handle_op_udiv(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildUDiv(ctx->builder, lhs, rhs, "v_udiv");
    set_val(ctx, res_id, res);
}
static inline void handle_op_iadd(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildAdd(ctx->builder, lhs, rhs, "v_iadd");
    set_val(ctx, res_id, res);
}

static inline void handle_op_sitof(JitContext* ctx, uint32_t res_id, uint32_t* operands) 
{
    LLVMValueRef op = get_val(ctx, operands[0]); 
    LLVMValueRef res = LLVMBuildSIToFP(ctx->builder,op, ctx->vec_float_type, "v_sitof");
    set_val(ctx, res_id, res);
}
static inline void handle_op_select(JitContext* ctx, uint32_t res_id, uint32_t* operands) 
{
    LLVMValueRef cond = get_val(ctx, operands[0]); // Vector of i1
    LLVMValueRef true_val = get_val(ctx, operands[1]);
    LLVMValueRef false_val = get_val(ctx, operands[2]);
    LLVMValueRef res = LLVMBuildSelect(ctx->builder, cond, true_val, false_val, "v_select");
    set_val(ctx, res_id, res);
}
static inline void handle_op_return_value(JitContext* ctx, uint32_t* operands)
{
    LLVMValueRef vec_val = get_val(ctx, operands[0]);
    
    LLVMTypeRef vec_ptr_type = LLVMPointerType(ctx->vec_float_type, 0);
    LLVMValueRef bitcast_ptr = LLVMBuildBitCast(ctx->builder, ctx->out_ptr_arg, vec_ptr_type, "vec_ptr_cast");
    
    build_masked_store(ctx, vec_val, bitcast_ptr, ctx->emask);
    
    LLVMBuildRetVoid(ctx->builder);
}


static inline void handle_op_decorate(JitContext* ctx, uint32_t* operands, int op_count) 
{
    uint32_t target_id = operands[0];
    uint32_t decoration = operands[1];
    uint32_t value = (op_count > 2) ? operands[2] : 0;

    ctx->decorations[target_id].is_decorated = 1;
    DEBUG_PRINT("Decorating ID %d with decoration %d (value: %d)\n", target_id, decoration, value);
    switch(decoration) {
        case SpvDecorationBinding:
            ctx->decorations[target_id].binding = value;
            break;
        case SpvDecorationDescriptorSet:
            ctx->decorations[target_id].descriptor_set = value;
            break;
        case SpvDecorationLocation:
            ctx->decorations[target_id].location = value;
            break;
        case SpvDecorationBuiltIn:
            ctx->decorations[target_id].builtin = value;
            break;
        default:
            DEBUG_PRINT("Ignored decoration %d on ID %d\n", decoration, target_id);
            break;
    }
}
static inline void handle_op_type_pointer(JitContext* ctx, uint32_t res_id, uint32_t* operands) 
{
    //uint32_t storage_class = operands[0];
    uint32_t type_id =  operands[1];
    ctx->type_info[res_id].opcode = SpvOpTypePointer;
    ctx->type_info[res_id].base_type_id = type_id; 
}

static inline void handle_op_type_array(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    uint32_t element_type_id = operands[0];
    //uint32_t length_id  = operands[1];
    ctx->type_info[res_id].opcode = SpvOpTypeArray;
    ctx->type_info[res_id].base_type_id = element_type_id;
}

static inline void handle_op_type_struct(JitContext* ctx, uint32_t res_id, uint32_t* member_types, int count) 
{
    ctx->type_info[res_id].opcode = SpvOpTypeStruct;
    ctx->type_info[res_id].member_count = count;
    ctx->type_info[res_id].member_types = malloc(sizeof(uint32_t) * count);
    memcpy(ctx->type_info[res_id].member_types, member_types, sizeof(uint32_t) * count);
}

static inline void handle_op_type_void(JitContext* ctx, uint32_t res_id)
{
    ctx->type_info[res_id].opcode = SpvOpTypeVoid;
    ctx->type_info[res_id].base_type_id = 0;
    ctx->type_info[res_id].member_types = NULL;
    ctx->type_info[res_id].member_count = 0;
}
static inline void handle_op_type_bool(JitContext* ctx, uint32_t res_id)
{
    ctx->type_info[res_id].opcode = SpvOpTypeBool;
    ctx->type_info[res_id].base_type_id = 0;
    ctx->type_info[res_id].member_types = NULL;
    ctx->type_info[res_id].member_count = 0;
}

static inline void handle_op_type_int(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    ctx->type_info[res_id].opcode = SpvOpTypeInt;

    uint32_t width = operands[0];
    uint32_t signedness = operands[1];

    ctx->type_info[res_id].base_type_id = width;
    ctx->type_info[res_id].member_types = NULL;
    ctx->type_info[res_id].member_count = signedness;
}
static inline void handle_op_type_float(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    ctx->type_info[res_id].opcode = SpvOpTypeFloat;

    uint32_t width = operands[0];

    ctx->type_info[res_id].base_type_id = width;
    ctx->type_info[res_id].member_types = NULL;
    ctx->type_info[res_id].member_count = 0;
}
static inline void handle_op_type_vector(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    ctx->type_info[res_id].opcode = SpvOpTypeVector;

    uint32_t component_type = operands[0];
    uint32_t count = operands[1];

    ctx->type_info[res_id].base_type_id = component_type;
    ctx->type_info[res_id].member_types = NULL;
    ctx->type_info[res_id].member_count = count;
}

static inline void handle_op_member_decorate(JitContext* ctx, uint32_t* operands) {
    uint32_t struct_id = operands[0];
    uint32_t member = operands[1];
    uint32_t decoration = operands[2];
    uint32_t value = operands[3];
    MemberDecoNode* node = ctx->member_decorations[struct_id];
    while(node) 
    {
        if(node->member_index == member) break;
        node = node->next;
    }
    if(!node) 
    {
        node = malloc(sizeof(MemberDecoNode));
        node->matrix_stride = -1; // Default
        node->member_index = member;
        node->offset = 0; // Default
        node->next = ctx->member_decorations[struct_id];
        ctx->member_decorations[struct_id] = node;
    }
    
    if(decoration == SpvDecorationOffset) 
    {
        node->offset = value;
    }
}


void jit_emit_instr(JitContext* ctx, uint16_t opcode, uint32_t res_id, uint32_t type_id, uint32_t* operands, int operand_count) 
{
    
    switch (opcode) 
    {
        case SpvOpTypeVoid:
            handle_op_type_void(ctx, res_id);
            break;

        case SpvOpTypeBool:
            handle_op_type_bool(ctx, res_id);
            break;

        case SpvOpTypeInt:
            handle_op_type_int(ctx, res_id, operands);
            break;
        case SpvOpTypeFloat:
            handle_op_type_float(ctx, res_id, operands);
            break;
        case SpvOpTypeVector:
            handle_op_type_vector(ctx, res_id, operands);
            break;
        case SpvOpTypeStruct:
            handle_op_type_struct(ctx, res_id, operands, operand_count);
            break;
        case SpvOpTypePointer:
            handle_op_type_pointer(ctx, res_id, operands);
            break;
        case SpvOpTypeArray:
            handle_op_type_array(ctx, res_id, operands);
            break;
        case SpvOpConstant:
            handle_op_constant(ctx, res_id, type_id, operands);
            break;
        case SpvOpDecorate:
            handle_op_decorate(ctx, operands, operand_count);
            break;
        case SpvOpMemberDecorate:
            handle_op_member_decorate(ctx, operands);
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
        case SpvOpFSub:
            handle_op_fsub(ctx, res_id, operands);
            break;
        case SpvOpFNegate:
            handle_op_fneg(ctx, res_id, operands);
            break;
        case SpvOpISub:
             handle_op_isub(ctx, res_id, operands);
            break;
        case SpvOpIAdd:
            handle_op_iadd(ctx, res_id, operands);
            break;
        case SpvOpIMul:
            handle_op_imul(ctx, res_id, operands);
            break;
        case SpvOpSDiv:
            handle_op_sdiv(ctx, res_id, operands);
            break;
        case SpvOpUDiv:
            handle_op_udiv(ctx, res_id, operands);
            break;
        case SpvOpConvertSToF:
            handle_op_sitof(ctx, res_id, operands);
            break;
        case SpvOpReturnValue:
            handle_op_return_value(ctx, operands);
            break;
        default:
            DEBUG_PRINT("Unhandled opcode %d in JIT emitter\n", opcode);
            break;
    }
}
void build_masked_store(JitContext* ctx, LLVMValueRef val_to_store, LLVMValueRef ptr, LLVMValueRef mask) 
{
    LLVMValueRef current_val = LLVMBuildLoad2(ctx->builder, ctx->vec_float_type, ptr, "v_load_current");

    LLVMValueRef masked_val = LLVMBuildSelect(ctx->builder, mask, val_to_store, current_val, "v_select_mask");

    LLVMBuildStore(ctx->builder, masked_val, ptr);
}

void jit_compile_spirv(uint32_t* binary, size_t word_count) 
{
    uint32_t* p = binary + 5;
    uint32_t* end = binary + word_count;
    
    JitContext ctx;
    ctx.bound = binary[3];
    ctx.type_kind_map = (uint8_t*)calloc(ctx.bound, sizeof(uint8_t));

    ctx.id_val_map = (LLVMValueRef*)calloc(ctx.bound, sizeof(LLVMValueRef));
    ctx.decorations = calloc(ctx.bound, sizeof(SpvDecoInfo));
    ctx.member_decorations = calloc(ctx.bound, sizeof(MemberDecoNode*)); // NEW
    ctx.type_info = calloc(ctx.bound, sizeof(SpvTypeInfo)); // NEW

    for (uint32_t i = 0; i < ctx.bound; i++)
    {
        ctx.decorations[i].descriptor_set = -1;
        ctx.decorations[i].binding = -1;
        ctx.decorations[i].location = -1;
        ctx.decorations[i].builtin = -1;
        ctx.decorations[i].array_stride = -1;
    }
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();

    ctx.context = LLVMContextCreate();
    ctx.module = LLVMModuleCreateWithNameInContext("spirv_vector_module", ctx.context);
    ctx.builder = LLVMCreateBuilderInContext(ctx.context);

    ctx.float_type = LLVMFloatTypeInContext(ctx.context);
    ctx.int_type = LLVMInt32TypeInContext(ctx.context);
    ctx.vec_float_type = LLVMVectorType(LLVMFloatTypeInContext(ctx.context), SIMT_WIDTH);
    ctx.vec_i1_type = LLVMVectorType(LLVMInt1TypeInContext(ctx.context), SIMT_WIDTH);
    ctx.int8_type = LLVMInt8TypeInContext(ctx.context);
    ctx.ptr_type = LLVMPointerType(ctx.int8_type, 0);

    LLVMTypeRef ptr_array_type = LLVMArrayType(ctx.ptr_type, MAX_BINDINGS);
    LLVMTypeRef stride_array_type = LLVMArrayType(ctx.int_type, MAX_ATTRIBUTES);
    LLVMTypeRef env_struct_elements[] = {
        ptr_array_type,     
        ptr_array_type,     
        stride_array_type,  
        ctx.int_type        
    };
    LLVMTypeRef env_struct_type = LLVMStructTypeInContext(ctx.context, env_struct_elements, 4, 0);

    // Function Signature: void main_simt(ExecutionContext* env, float* out_ptr)
    LLVMTypeRef param_types[] = { 
        LLVMPointerType(env_struct_type, 0), 
        LLVMPointerType(ctx.float_type, 0) 
    };
    LLVMTypeRef func_type = LLVMFunctionType(LLVMVoidTypeInContext(ctx.context), param_types, 2, 0);
    ctx.func = LLVMAddFunction(ctx.module, "main_simt", func_type);
    ctx.out_ptr_arg = LLVMGetParam(ctx.func, 1);

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx.context, ctx.func, "entry");
    LLVMPositionBuilderAtEnd(ctx.builder, entry);

    DEBUG_PRINT("--- Starting JIT Compilation (LLVM Vector Backend) ---\n");


    LLVMValueRef values[SIMT_WIDTH];
    for (size_t i = 0; i < SIMT_WIDTH; i++)
    {
         values[i] = LLVMConstInt(LLVMInt1TypeInContext(ctx.context), 1, 0); 
    }
    
    values[0] = LLVMConstInt(LLVMInt1TypeInContext(ctx.context), 0, 0); 
    values[1] = LLVMConstInt(LLVMInt1TypeInContext(ctx.context), 0, 0); 
    values[2] = LLVMConstInt(LLVMInt1TypeInContext(ctx.context), 0, 0); 
    values[3] = LLVMConstInt(LLVMInt1TypeInContext(ctx.context), 0, 0); 
    ctx.emask = LLVMConstVector(values, SIMT_WIDTH);
 

    while (p < end) 
    {
        uint32_t instruction = p[0];
        uint16_t opcode = instruction & 0xFFFF;
        uint16_t inst_word_count = instruction >> 16;

        if (opcode == SpvOpTypeFloat || opcode == SpvOpTypeInt) 
        {
            ctx.type_kind_map[p[1]] = (uint8_t)opcode;
        }

        if (opcode == SpvOpSource || opcode == SpvOpName || opcode == SpvOpMemberName || 
            opcode == SpvOpLine || opcode == SpvOpNoLine) 
        {
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
        while (current_idx < inst_word_count && op_count < 16) 
        {
            operands[op_count++] = p[current_idx++];
        }

        jit_emit_instr(&ctx, opcode, res_id, type_id, operands, op_count);
        p += inst_word_count;
    }

    char *error = NULL;
    LLVMVerifyModule(ctx.module, LLVMAbortProcessAction, &error);
    LLVMDisposeMessage(error);

    LLVMExecutionEngineRef engine;
    if (LLVMCreateExecutionEngineForModule(&engine, ctx.module, &error) != 0) 
    {
        DEBUG_PRINT("Failed to create execution engine: %s\n", error);
        return;
    }

    typedef void (*jitted_func_t)(ExecutionContext*, float*);
    jitted_func_t my_func = (jitted_func_t)LLVMGetFunctionAddress(engine, "main_simt");



    //TO-DO only for debugging, remove later
    // Align the buffer to 64 bytes for AVX-512 compatibility
    float *results = aligned_alloc(64, sizeof(float) * SIMT_WIDTH);
    memset(results, 0, sizeof(float) * SIMT_WIDTH);
    ExecutionContext exec_ctx = {0};
    my_func(&exec_ctx, results);

    DEBUG_PRINT("Execution Results:\n");
    for(int i=0; i<SIMT_WIDTH; i++) DEBUG_PRINT(" Lane %d: %f\n", i, results[i]);
    debug_dump_all_metadata(&ctx);
    for(uint32_t i=0; i<ctx.bound; i++) 
    {
        MemberDecoNode* n = ctx.member_decorations[i];
        while(n) { MemberDecoNode* next = n->next; free(n); n = next; }
        if(ctx.type_info[i].opcode == SpvOpTypeStruct) free(ctx.type_info[i].member_types);
    }
    free(results);
    free(ctx.type_kind_map);
    free(ctx.id_val_map);
    LLVMDisposeBuilder(ctx.builder);
    LLVMDisposeExecutionEngine(engine);
    LLVMContextDispose(ctx.context);
}