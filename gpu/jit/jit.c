#include "jit.h"
#include "jit_alu.h"
#include "jit_decorators.h"
#include "jit_flow.h"
#include "jit_mem.h"
#include "jit_smpl.h"
#include <llvm-c/Transforms/PassBuilder.h>
#include "debug_gpu.h"

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

G_GNUC_UNUSED static const char*  spv_opcode_to_string(SpvOp op)
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
    /* Compute pointer to location_out_buffers[0] from the passed-in ExecutionContext parameter */
    LLVMValueRef indices[] = {
        LLVMConstInt(ctx->int_type, 0, 0), /* struct ptr idx */
        LLVMConstInt(ctx->int_type, 2, 0), /* location_out_buffers field */
        LLVMConstInt(ctx->int_type, 0, 0)  /* first element (location 0) */
    };

    LLVMValueRef slot_ptr = LLVMBuildInBoundsGEP2(ctx->builder, ctx->exec_ctx_type, ctx->env_arg_param, indices, 3, "out_slot_ptr");
    /* slot_ptr has type i8** (pointer to element in the array); load the actual i8* */
    LLVMValueRef actual_out_ptr = LLVMBuildLoad2(ctx->builder, ctx->ptr_type, slot_ptr, "out_actual_ptr");
    LLVMValueRef bitcast_ptr = LLVMBuildBitCast(ctx->builder, actual_out_ptr, vec_ptr_type, "vec_ptr_cast");
    
    build_masked_store(ctx, vec_val, bitcast_ptr, ctx->emask);
    
    LLVMBuildRetVoid(ctx->builder);
}
/*
 Example usage:
    {
        LLVMValueRef float_val = LLVMConstReal(ctx->float_type, 42.0);
        LLVMValueRef args2[] = { float_val };
        jit_call_printf(ctx, "Example float: %f\n", args2, 1);
    }
*/
void jit_call_printf(JitContext* ctx, const char* fmt, LLVMValueRef* args, unsigned num_args)
{
    // Get or add printf function to module
    LLVMValueRef printf_func = LLVMGetNamedFunction(ctx->module, "printf");
    if (!printf_func) 
    {
        LLVMTypeRef printf_decl_arg_types[] = { LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0) };
        LLVMTypeRef printf_decl_type = LLVMFunctionType(
            LLVMInt32TypeInContext(ctx->context), printf_decl_arg_types, 1, 1
        );
        printf_func = LLVMAddFunction(ctx->module, "printf", printf_decl_type);
    }

    // Build format string
    LLVMValueRef fmt_str = LLVMBuildGlobalStringPtr(ctx->builder, fmt, "fmt");
    
    // Prepare call arguments: format string + user args, converting floats to doubles
    LLVMValueRef* call_args = malloc((num_args + 1) * sizeof(LLVMValueRef));
    LLVMTypeRef* arg_types = malloc((num_args + 1) * sizeof(LLVMTypeRef));
    
    call_args[0] = fmt_str;
    arg_types[0] = LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0);
    
    for (unsigned i = 0; i < num_args; ++i)
    {
        LLVMTypeRef arg_type = LLVMTypeOf(args[i]);
        // Convert float to double for printf
        if (LLVMGetTypeKind(arg_type) == LLVMFloatTypeKind)
        {
            call_args[i + 1] = LLVMBuildFPExt(ctx->builder, args[i], LLVMDoubleTypeInContext(ctx->context), "f2d");
            arg_types[i + 1] = LLVMDoubleTypeInContext(ctx->context);
        }
        else
        {
            call_args[i + 1] = args[i];
            arg_types[i + 1] = arg_type;
        }
    }

    LLVMTypeRef printf_type = LLVMFunctionType(
        LLVMInt32TypeInContext(ctx->context), arg_types, num_args + 1, 1
    );
    
    LLVMBuildCall2(ctx->builder, printf_type, printf_func, call_args, num_args + 1, "");
    
    free(call_args);
    free(arg_types);
}
// Print all SIMT_WIDTH elements of a vector in compact format: [1.0 2.0 3.0 ...]
void jit_call_printf_simt(JitContext* ctx, const char* fmt, LLVMValueRef vec_val)
{
    jit_call_printf(ctx, "[", NULL, 0);
    
    for (unsigned lane = 0; lane < SIMT_WIDTH; lane++)
    {
        LLVMValueRef elem = LLVMBuildExtractValue(ctx->builder, vec_val, lane, "simt_elem");
        LLVMValueRef args[] = { elem };
        
        char fmt_with_sep[256];
        if (lane == 0)
            snprintf(fmt_with_sep, sizeof(fmt_with_sep), "%s", fmt);
        else
            snprintf(fmt_with_sep, sizeof(fmt_with_sep), " %s", fmt);
        
        jit_call_printf(ctx, fmt_with_sep, args, 1);
    }
    
    jit_call_printf(ctx, "]\n", NULL, 0);
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
        case SpvOpName:
            handle_op_name(ctx, operands);
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
        case SpvOpULessThan:
            handle_op_ulessthan(ctx, res_id, operands);
            break;
        case SpvOpIEqual:
            handle_op_iequal(ctx, res_id, operands);
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
        case SpvOpBranch:
            handle_op_branch(ctx, operands);
            break;
        case SpvOpBranchConditional:
            handle_op_branch_conditional(ctx, operands);
            break;
        case SpvOpSelectionMerge:
            handle_op_selection_merge(ctx, operands);
            break;
        case SpvOpLoopMerge:
            handle_op_loop_merge(ctx, operands);
            break;
        case SpvOpPhi:
            handle_op_phi(ctx, res_id, type_id, operands, operand_count);
            break;
        case SpvOpKill:
            LLVMBuildUnreachable(ctx->builder);
            break;
        case SpvOpUnreachable:
            LLVMBuildUnreachable(ctx->builder);
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
        case SpvOpConstantComposite:
            handle_op_composite_construct(ctx, res_id, type_id, operands);
            break;
        case SpvOpCompositeExtract:
            handle_op_composite_extract(ctx, res_id, operands, operand_count - 1); 
            break;
        case SpvOpTypeMatrix:
            handle_op_type_matrix(ctx, res_id, operands);
            break;
        case SpvOpVectorTimesScalar:
            handle_op_vector_times_scalar(ctx, res_id, operands);
            break;
        case SpvOpDot:
            handle_op_dot(ctx, res_id, operands);
            break;
        case SpvOpMatrixTimesVector:
            handle_op_matrix_times_vector(ctx, res_id, operands);
            break;
        case SpvOpVectorShuffle:
            handle_op_vector_shuffle(ctx, res_id,type_id, operands);
            break;
        case SpvOpEntryPoint:
            handle_op_entry_point(ctx, operands, operand_count);
            break;
        case SpvOpExecutionMode:
            if (operand_count >= 5 && operands[1] == 17 /* SpvExecutionModeLocalSize */) {
                ctx->shader_info.local_size_x = operands[2];
                ctx->shader_info.local_size_y = operands[3];
                ctx->shader_info.local_size_z = operands[4];
            }
            break;
        case SpvOpControlBarrier:
        case SpvOpMemoryBarrier:
        {
            LLVMBuildFence(ctx->builder, LLVMAtomicOrderingSequentiallyConsistent, 0, "");
            if (opcode == SpvOpControlBarrier) {
                uint32_t barrier_idx = ctx->barrier_count++;
                ctx->shader_info.barrier_count = ctx->barrier_count;

                LLVMValueRef indices[] = {
                    LLVMConstInt(ctx->int_type, 0, 0),
                    LLVMConstInt(ctx->int_type, 5, 0)
                };
                LLVMValueRef phase_slot = LLVMBuildInBoundsGEP2(ctx->builder, ctx->exec_ctx_type, ctx->env_arg_param, indices, 2, "phase_slot");
                LLVMValueRef phase_val = LLVMBuildLoad2(ctx->builder, ctx->int_type, phase_slot, "phase_val");

                LLVMValueRef is_current = LLVMBuildICmp(ctx->builder, LLVMIntEQ, phase_val, LLVMConstInt(ctx->int_type, barrier_idx, 0), "is_phase_match");

                LLVMBasicBlockRef next_phase_bb = LLVMAppendBasicBlockInContext(ctx->context, ctx->func, "next_phase_bb");
                LLVMBasicBlockRef exit_bb = LLVMAppendBasicBlockInContext(ctx->context, ctx->func, "phase_exit_bb");

                LLVMBuildCondBr(ctx->builder, is_current, exit_bb, next_phase_bb);

                LLVMPositionBuilderAtEnd(ctx->builder, exit_bb);
                LLVMBuildRetVoid(ctx->builder);

                LLVMPositionBuilderAtEnd(ctx->builder, next_phase_bb);
                ctx->current_block = next_phase_bb;
            }
            break;
        }
        case SpvOpTypeImage:
            handle_op_type_image(ctx, res_id, operands);
            break;
        case SpvOpTypeSampler:
            handle_op_type_sampler(ctx, res_id);
            break;
        case SpvOpTypeSampledImage:
            handle_op_type_sampled_image(ctx, res_id, operands[0]);
            break;
        case SpvOpSampledImage:
            handle_op_sampled_image(ctx, res_id, operands);
            break;
        case SpvOpImage:
            handle_op_image(ctx, res_id, operands);
            break;
        case SpvOpImageSampleImplicitLod:
            handle_op_image_sample_implicit_lod(ctx, res_id, operands);
            break;
        case SpvOpImageSampleExplicitLod:
            handle_op_image_sample_explicit_lod(ctx, res_id, operands);
            break;
        case SpvOpImageFetch:
            handle_op_image_fetch(ctx, res_id, operands);
            break;
        case SpvOpImageQuerySizeLod:
            handle_op_image_query_size_lod(ctx, res_id, operands);
            break;
        case SpvOpImageQuerySize:
            handle_op_image_query_size(ctx, res_id, operands);
            break;
        default:
            DEBUG_PRINT("Unhandled opcode %d in JIT emitter\n", opcode);
            break;
    }
}
void build_masked_store(JitContext *ctx, LLVMValueRef val_to_store, LLVMValueRef ptr, LLVMValueRef mask) 
{
    LLVMTypeRef type = LLVMTypeOf(val_to_store);
    
    if (LLVMGetTypeKind(type) == LLVMVectorTypeKind)
    {
        LLVMValueRef current_val = LLVMBuildLoad2(ctx->builder, type, ptr, "v_load_current");
        LLVMValueRef masked_val = LLVMBuildSelect(ctx->builder, mask, val_to_store, current_val, "v_select_mask");
        LLVMBuildStore(ctx->builder, masked_val, ptr);
    } 
    else if (LLVMGetTypeKind(type) == LLVMArrayTypeKind) 
    {
        uint32_t count = LLVMGetArrayLength(type);
        LLVMTypeRef elem_type = LLVMGetElementType(type);

        for (uint32_t i = 0; i < count; i++)
        {

            LLVMValueRef index = LLVMConstInt(LLVMInt32TypeInContext(ctx->context), i, 0);
            
            LLVMValueRef element_ptr = LLVMBuildInBoundsGEP2(ctx->builder, elem_type, ptr, &index, 1, "elem_ptr");
            
            LLVMValueRef element_val = LLVMBuildExtractValue(ctx->builder, val_to_store, i, "elem_val");

            build_masked_store(ctx, element_val, element_ptr, mask);
        }
    }
}
LLVMTypeRef map_spv_to_llvm_type(JitContext *ctx, uint32_t type_id) 
{
    SpvTypeInfo* info = &ctx->type_info[type_id]; 

    switch (info->opcode)
    {
        case SpvOpTypeFloat:
        case SpvOpTypeInt:
            return ctx->vec_float_type;

        case SpvOpTypeBool:
            return ctx->vec_i1_type;

        case SpvOpTypeVector: 
        {
            uint32_t count = info->member_count; 
            return LLVMArrayType(ctx->vec_float_type, count);
        }

        case SpvOpTypeMatrix: 
        {
            LLVMTypeRef column_type = map_spv_to_llvm_type(ctx, info->base_type_id);
            uint32_t col_count = info->member_count;
            return LLVMArrayType(column_type, col_count);
        }

        case SpvOpTypePointer:
            return map_spv_to_llvm_type(ctx, info->base_type_id);

        case SpvOpTypeStruct: 
            //TO-DO
            return LLVMStructTypeInContext(ctx->context, NULL, 0, 0);
        case SpvOpTypeSampler:
        case SpvOpTypeSampledImage:
        case SpvOpTypeImage:
        {
            return ctx->ptr_type;
        }

        default:
            DEBUG_PRINT("Unsupported type opcode: %d\n", info->opcode);
            return NULL;
    }
}

jitted_func_t jit_compile_spirv(JitContext* ctx, uint32_t* binary, size_t word_count) 
{   
    uint32_t* p = binary + 5;
    uint32_t* end = binary + word_count;
        

    ctx->bound = binary[3];
    ctx->type_kind_map = (uint8_t*)calloc(ctx->bound, sizeof(uint8_t));
    create_glsl_std_450_map(ctx);

    ctx->id_val_map = (LLVMValueRef*)calloc(ctx->bound, sizeof(LLVMValueRef));
    ctx->decorations = calloc(ctx->bound, sizeof(SpvDecoInfo));
    ctx->member_decorations = calloc(ctx->bound, sizeof(MemberDecoNode*));
    ctx->type_info = calloc(ctx->bound, sizeof(SpvTypeInfo));
    ctx->names = calloc(ctx->bound, sizeof(char*));
    for (uint32_t i = 0; i < ctx->bound; i++)
    {
        ctx->decorations[i].descriptor_set = -1;
        ctx->decorations[i].binding = -1;
        ctx->decorations[i].location = -1;
        ctx->decorations[i].builtin = -1;
        ctx->decorations[i].array_stride = -1;
    }
 


    ctx->func = NULL;
    ctx->current_block = NULL;
    ctx->control_stack_depth = 0;
    memset(ctx->control_stack, 0, sizeof(ctx->control_stack));
    ctx->shared_mem_offset = 0;
    ctx->spill_mem_offset = 0;
    ctx->barrier_count = 0;
    ctx->shader_info.barrier_count = 0;

    DEBUG_PRINT("--- Starting JIT Compilation (LLVM ORC Backend) ---\n");
    LLVMValueRef values[SIMT_WIDTH];
    for (size_t i = 0; i < SIMT_WIDTH; i++)
    {
        values[i] = LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0); 
    }
    ctx->emask = LLVMConstVector(values, SIMT_WIDTH);
    
    while (p < end) 
    {
        uint32_t instruction = p[0];
        uint16_t opcode = instruction & 0xFFFF;
        uint16_t inst_word_count = instruction >> 16;

        if (opcode == SpvOpTypeFloat || opcode == SpvOpTypeInt) 
        {
            ctx->type_kind_map[p[1]] = (uint8_t)opcode;
        }

        if (opcode == SpvOpSource || opcode == SpvOpMemberName || 
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

        jit_emit_instr(ctx, opcode, res_id, type_id, operands, op_count);
        p += inst_word_count;
    }
    

    char *error = NULL;
    //LLVMDumpModule(ctx->module);
    LLVMVerifyModule(ctx->module, LLVMAbortProcessAction, &error);
    LLVMDisposeMessage(error);
    //LLVMDumpModule(ctx->module);


    char* triple = LLVMGetDefaultTargetTriple(); 
    char* cpu = LLVMGetHostCPUName();            
    char* features = LLVMGetHostCPUFeatures();    

    LLVMTargetRef target;
    LLVMGetTargetFromTriple(triple, &target, &error);

    LLVMTargetMachineRef tm = LLVMCreateTargetMachine(
        target, 
        triple, 
        cpu,       
        features,
        LLVMCodeGenLevelAggressive, 
        LLVMRelocDefault, 
        LLVMCodeModelDefault
    );
    LLVMPassBuilderOptionsRef options = LLVMCreatePassBuilderOptions();
    LLVMPassBuilderOptionsSetVerifyEach(options, 1);
    LLVMPassBuilderOptionsSetDebugLogging(options, 0);

    const char* pipeline = "default<O2>";

    LLVMErrorRef err = LLVMRunPasses(ctx->module, pipeline, tm, options);

    if (err) 
    {
        char* msg = LLVMGetErrorMessage(err);
        fprintf(stderr, "Pass error: %s\n", msg);
        LLVMDisposeErrorMessage(msg);
    }

    LLVMDisposePassBuilderOptions(options);

    LLVMTargetMachineEmitToFile(tm, ctx->module, "shader_output.s", 
                             LLVMAssemblyFile, &error);


    jitted_func_t my_func = (jitted_func_t)LLVMGetFunctionAddress(ctx->engine, "main_simt");

    for(uint32_t i=0; i<ctx->bound; i++) 
    {
        MemberDecoNode* n = ctx->member_decorations[i];
        while(n) { MemberDecoNode* next = n->next; free(n); n = next; }
        if(ctx->type_info[i].opcode == SpvOpTypeStruct) free(ctx->type_info[i].member_types);
    }
    free(ctx->type_kind_map);
    free(ctx->id_val_map);
   
    return my_func;
}

void init_jit(JitContext* ctx,shader_t shader_type)
{
    ctx->shader_type = shader_type;

    char *error = NULL;
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();

    ctx->context = LLVMContextCreate();
    ctx->module = LLVMModuleCreateWithNameInContext("spirv_vector_module", ctx->context);
    ctx->builder = LLVMCreateBuilderInContext(ctx->context);

    ctx->env_arg_param = NULL;
    ctx->vs_data_param = NULL;
    ctx->fs_data_param = NULL;
    ctx->cs_data_param = NULL;

    ctx->float_type = LLVMFloatTypeInContext(ctx->context);
    ctx->int_type = LLVMInt32TypeInContext(ctx->context);
    ctx->vec_float_type = LLVMVectorType(LLVMFloatTypeInContext(ctx->context), SIMT_WIDTH);
    ctx->vec_i1_type = LLVMVectorType(LLVMInt1TypeInContext(ctx->context), SIMT_WIDTH);
    ctx->int8_type = LLVMInt8TypeInContext(ctx->context);
    ctx->ptr_type = LLVMPointerType(ctx->int8_type, 0);

    LLVMTypeRef float_type = LLVMFloatTypeInContext(ctx->context);
    // <16 x float>
    LLVMTypeRef simt_float =
        LLVMVectorType(float_type, 16);

    LLVMTypeRef ptr_type =
        LLVMPointerType(
            LLVMInt8TypeInContext(ctx->context),
            0);

    // SimtVec4 = [4 x <16 x float>]
    LLVMTypeRef simt_vec4_type =
        LLVMArrayType(simt_float, 4);

    // SimtVec3 = [3 x <16 x float>]
    LLVMTypeRef simt_vec3_type =
        LLVMArrayType(simt_float, 3);

    // BuiltinVertexOutput
    LLVMTypeRef builtin_fields[] = {
        simt_vec4_type,                 // gl_Position
        simt_float,                     // gl_PointSize
        LLVMArrayType(simt_float, 1),   // gl_ClipDistance[1]
        LLVMArrayType(simt_float, 1)    // gl_CullDistance[1]
    };

    LLVMTypeRef builtin_vertex_output_type =
        LLVMStructTypeInContext(
            ctx->context,
            builtin_fields,
            4,
            0);

    // ExecutionContext
    LLVMTypeRef exec_ctx_fields[] = {
        LLVMArrayType(ptr_type, MAX_BINDINGS),     // binding_buffers (0)
        LLVMArrayType(ptr_type, MAX_ATTRIBUTES),   // location_in_buffers (1)
        LLVMArrayType(ptr_type, MAX_ATTRIBUTES),   // location_out_buffers (2)
        ptr_type,                                  // shared_memory (3)
        ptr_type,                                  // spill_buffer (4)
        ctx->int_type,                             // current_phase (5)
        builtin_vertex_output_type                 // vertexOut (6)
    };

    ctx->exec_ctx_type = LLVMStructTypeInContext(ctx->context, exec_ctx_fields, 7, 0);

    LLVMTypeRef vs_data_fields[] = {
        simt_vec4_type,// gl_Position (Index 0)
        simt_float,    // gl_PointSize (Index 1)
        simt_float,    // gl_ClipDistance (Index 2)
        simt_float     // gl_CullDistance (Index 3)
    };
    ctx->vs_data_type = LLVMStructTypeInContext(ctx->context, vs_data_fields, 4, 0);
    LLVMTypeRef fs_data_fields[] = {
        simt_vec4_type,// gl_FragCoord (Index 0)
        simt_float,    // gl_FrontFacing (Index 1)
        simt_vec4_type,// gl_PointCoord (Index 2)
        simt_float     // gl_SampleID (Index 3)
    };
    ctx->fs_data_type = LLVMStructTypeInContext(ctx->context, fs_data_fields, 4, 0);

    LLVMTypeRef cs_data_fields[] = {
        simt_vec3_type, // gl_GlobalInvocationID (Index 0)
        simt_vec3_type, // gl_LocalInvocationID (Index 1)
        simt_float,     // gl_LocalInvocationIndex (Index 2)
        simt_vec3_type, // gl_WorkGroupID (Index 3)
        simt_vec3_type, // gl_NumWorkGroups (Index 4)
        simt_vec3_type  // gl_WorkGroupSize (Index 5)
    };
    ctx->cs_data_type = LLVMStructTypeInContext(ctx->context, cs_data_fields, 6, 0);

    if (LLVMCreateExecutionEngineForModule(&ctx->engine, ctx->module, &error) != 0) 
    {
        DEBUG_PRINT("Failed to create execution engine: %s\n", error);
    }

}
ExecutionContext* get_ectx_from_mcjit(JitContext *ctx)
{
   uint64_t addr = LLVMGetGlobalValueAddress(ctx->engine, "ectx");

    if (!addr)
        return NULL;

    return (ExecutionContext*)(uintptr_t)addr;
}

BuiltinVertexOutput* get_vs_data_from_mcjit(JitContext *ctx)
{
   uint64_t addr = LLVMGetGlobalValueAddress(ctx->engine, "vs_data");

    if (!addr)
        return NULL;

    return (BuiltinVertexOutput*)(uintptr_t)addr;
}
BuiltinFragmentInput* get_fs_data_from_mcjit(JitContext *ctx)
{
   uint64_t addr = LLVMGetGlobalValueAddress(ctx->engine, "fs_data");

    if (!addr)
        return NULL;

    return (BuiltinFragmentInput*)(uintptr_t)addr;
}
void free_jit(JitContext* ctx)
{
    if (!ctx) return;

    if (ctx->builder) 
    {
        LLVMDisposeBuilder(ctx->builder);
    }

    if (ctx->jit) 
    {
        LLVMOrcDisposeLLJIT(ctx->jit);
    }

    if (ctx->engine) 
    {
        LLVMDisposeExecutionEngine(ctx->engine);
    }


    if (ctx->ts_ctx) 
    {
        LLVMOrcDisposeThreadSafeContext(ctx->ts_ctx);
    } 
    else if (ctx->context) 
    {
        LLVMContextDispose(ctx->context);
    }


   // if (ctx->type_kind_map) free(ctx->type_kind_map);
   // if (ctx->id_val_map)    free(ctx->id_val_map);
   // if (ctx->decorations)   free(ctx->decorations);
    //if (ctx->type_info)     free(ctx->type_info);


    // if (ctx->member_decorations)
    // {
    //     for (uint32_t i = 0; i < ctx->bound; ++i) 
    //     {
    //         MemberDecoNode* node = ctx->member_decorations[i];
    //         while (node) 
    //         {
    //             MemberDecoNode* next = node->next; 
    //             free(node);
    //             node = next;
    //         }
    //     }
    //     free(ctx->member_decorations);
    // }

    if (ctx->names) 
    {
        for (uint32_t i = 0; i < ctx->bound; ++i)
        {
            if (ctx->names[i]) 
            {
                free(ctx->names[i]);
            }
        }
        free(ctx->names);
    }

    // ==========================================================
    memset(ctx, 0, sizeof(JitContext));
}
