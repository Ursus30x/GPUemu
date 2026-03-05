
#include "jit.h"
#include "jit_alu.h"
#include "jit_decorators.h"
#include "jit_flow.h"
#include "jit_mem.h"
#include <llvm-c/Transforms/PassBuilder.h>

static void debug_print_single_deco(uint32_t id, SpvDecoInfo* d)
{
    if (!d || !d->is_decorated)
        return;

    DEBUG_PRINT("ID %u Decorations:\n", id);

    if (d->descriptor_set >= 0)
        DEBUG_PRINT("  DescriptorSet: %d\n", d->descriptor_set);

    if (d->binding >= 0)
        DEBUG_PRINT("  Binding: %d\n", d->binding);

    if (d->location >= 0)
        DEBUG_PRINT("  Location: %d\n", d->location);

    if (d->builtin >= 0)
        DEBUG_PRINT("  BuiltIn: %d\n", d->builtin);

    if (d->array_stride >= 0)
        DEBUG_PRINT("  ArrayStride: %d\n", d->array_stride);

    DEBUG_PRINT("\n");
}

static void debug_dump_decorations(JitContext* ctx)
{
    DEBUG_PRINT("==== ID Decorations ====\n");

    for (uint32_t i = 0; i < ctx->bound; i++)
    {
        if (ctx->decorations[i].is_decorated)
            debug_print_single_deco(i, &ctx->decorations[i]);
    }
}
/* ============================================================
   Member Decorations Debug
   ============================================================ */

static void G_GNUC_UNUSED debug_print_member_list(uint32_t struct_id, MemberDecoNode* node)
{
    while (node)
    {
        DEBUG_PRINT("Struct %u Member %u:\n",
                    struct_id, node->member_index);

        if (node->offset >= 0)
            DEBUG_PRINT("  Offset: %d\n", node->offset);

        if (node->matrix_stride >= 0)
            DEBUG_PRINT("  MatrixStride: %d\n", node->matrix_stride);

        DEBUG_PRINT("\n");

        node = node->next;
    }
}

static void G_GNUC_UNUSED debug_dump_member_decorations(JitContext* ctx)
{
    DEBUG_PRINT("==== Member Decorations ====\n");

    for (uint32_t i = 0; i < ctx->bound; i++)
    {
        if (ctx->member_decorations[i])
            debug_print_member_list(i, ctx->member_decorations[i]);
    }
}


/* ============================================================
   Type Info Debug
   ============================================================ */

static const char* G_GNUC_UNUSED spv_opcode_to_string(SpvOp op)
{
    switch (op)
    {
        case SpvOpTypeStruct:   return "OpTypeStruct";
        case SpvOpTypePointer:  return "OpTypePointer";
        case SpvOpTypeArray:    return "OpTypeArray";
        case SpvOpTypeFloat:    return "OpTypeFloat";
        case SpvOpTypeInt:      return "OpTypeInt";
        default:                return "Other";
    }
}

static void G_GNUC_UNUSED debug_dump_type_info(JitContext* ctx)
{
    DEBUG_PRINT("==== Type Info ====\n");

    for (uint32_t i = 0; i < ctx->bound; i++)
    {
        SpvTypeInfo* t = &ctx->type_info[i];

        if (!t->opcode)
            continue;

        DEBUG_PRINT("Type ID %u:\n", i);
        DEBUG_PRINT("  Opcode: %s\n", spv_opcode_to_string(t->opcode));
        DEBUG_PRINT("  Base Type ID: %u\n", t->base_type_id);

        if (t->member_count > 0 && t->member_types)
        {
            DEBUG_PRINT("  Members (%u): ", t->member_count);
            for (uint32_t m = 0; m < t->member_count; m++)
                DEBUG_PRINT("%u ", t->member_types[m]);
            DEBUG_PRINT("\n");
        }

        DEBUG_PRINT("\n");
    }
}


/* ============================================================
   Full Metadata Dump
   ============================================================ */

static void G_GNUC_UNUSED debug_dump_all_metadata(JitContext* ctx)
{
    DEBUG_PRINT("\n====================================\n");
    DEBUG_PRINT("        SPIR-V METADATA DUMP        \n");
    DEBUG_PRINT("====================================\n\n");

    debug_dump_decorations(ctx);
    debug_dump_member_decorations(ctx);
    debug_dump_type_info(ctx);

    DEBUG_PRINT("====================================\n\n");
}
LLVMValueRef get_val(JitContext* ctx, uint32_t id) 
{
    return ctx->id_val_map[id];
}

void set_val(JitContext* ctx, uint32_t id, LLVMValueRef val) 
{
    ctx->id_val_map[id] = val;
}


static inline void handle_op_return_value(JitContext* ctx, uint32_t* operands)
{
    LLVMValueRef vec_val = get_val(ctx, operands[0]);
    
    LLVMTypeRef vec_ptr_type = LLVMPointerType(ctx->vec_float_type, 0);
    LLVMValueRef bitcast_ptr = LLVMBuildBitCast(ctx->builder, ctx->out_ptr_arg, vec_ptr_type, "vec_ptr_cast");
    
    build_masked_store(ctx, vec_val, bitcast_ptr, ctx->emask);
    
    LLVMBuildRetVoid(ctx->builder);
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
        case SpvOpSLessThan:
            handle_op_slessthan(ctx, res_id, operands);
            break;
        case SpvOpFOrdLessThan:
            handle_op_fordlessthan(ctx, res_id, operands);
            break;
        case SpvOpFOrdGreaterThan:
            handle_op_fordgreaterthan(ctx, res_id, operands);
            break;
        case SpvOpFMod:
            handle_op_fmod(ctx, res_id, operands);
            break;
        case SpvOpTypeFunction:
            handle_op_type_function(ctx, res_id, operands);
            break;
        case SpvOpFunction:
            handle_op_function(ctx, res_id, type_id, operands);
            break;
        case SpvOpLabel:
            handle_op_label(ctx, res_id);
            break;
        case SpvOpFunctionEnd:
            // Nothing to do here for now, but we could add cleanup logic if needed in the future
            break;
        case SpvOpReturn:
            LLVMBuildRetVoid(ctx->builder);
            break;
        case SpvOpVariable:
            handle_op_variable(ctx, res_id, type_id, operand_count, operands);
            break;
        case SpvOpLoad:
            handle_op_load(ctx, res_id, type_id, operands);
            break;
        case SpvOpStore:
            handle_op_store(ctx, operands);
            break;
        case SpvOpAccessChain:
            handle_op_access_chain(ctx, res_id, type_id, operands, operand_count);
            break;
        case SpvOpExtInst:
            handle_op_ext_instr(ctx, res_id, operands);
            break;
        case SpvOpCompositeConstruct:
            handle_op_composite_construct(ctx, res_id, type_id, operands);
            break;
        case SpvOpCompositeExtract:
            handle_op_composite_extract(ctx, res_id, operands);
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

float* jit_compile_spirv(uint32_t* binary, size_t word_count) 
{   
    uint32_t* p = binary + 5;
    uint32_t* end = binary + word_count;
    
    JitContext ctx = {0};
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();

    ctx.context = LLVMContextCreate();
    ctx.module = LLVMModuleCreateWithNameInContext("spirv_vector_module", ctx.context);
    ctx.builder = LLVMCreateBuilderInContext(ctx.context);

    ctx.bound = binary[3];
    ctx.type_kind_map = (uint8_t*)calloc(ctx.bound, sizeof(uint8_t));
    create_glsl_std_450_map(&ctx);

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
 
    ctx.float_type = LLVMFloatTypeInContext(ctx.context);
    ctx.int_type = LLVMInt32TypeInContext(ctx.context);
    ctx.vec_float_type = LLVMVectorType(LLVMFloatTypeInContext(ctx.context), SIMT_WIDTH);
    ctx.vec_i1_type = LLVMVectorType(LLVMInt1TypeInContext(ctx.context), SIMT_WIDTH);
    ctx.int8_type = LLVMInt8TypeInContext(ctx.context);
    ctx.ptr_type = LLVMPointerType(ctx.int8_type, 0);

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
        return NULL;
    }
    LLVMDumpModule(ctx.module); // Debug: Dump the generated LLVM IR
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
    free(ctx.type_kind_map);
    free(ctx.id_val_map);
    LLVMDisposeBuilder(ctx.builder);
    LLVMDisposeExecutionEngine(engine);
    LLVMContextDispose(ctx.context);
    return results;
}