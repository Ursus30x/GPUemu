#include <stddef.h>
#include "jit_mem.h"
#include "jit_flow.h"
#include "jit_decorators.h"
#include "debug_gpu.h"

/* ========================================================================= */
/* Section 1: ExecutionContext Accessors & Value Extraction Helpers          */
/* ========================================================================= */

enum ExecutionContextField {
    ECTX_FIELD_BINDINGS     = 0,
    ECTX_FIELD_LOC_IN       = 1,
    ECTX_FIELD_LOC_OUT      = 2,
    ECTX_FIELD_SHARED_MEM   = 3,
    ECTX_FIELD_SPILL        = 4,
    ECTX_FIELD_CUR_PHASE    = 5,
    ECTX_FIELD_ACTIVE_MASK  = 6,
    ECTX_FIELD_TASK_PAYLOAD = 7,
    ECTX_FIELD_MESH_OUTPUT  = 8,
    ECTX_FIELD_TASK_GROUPS  = 9,
};

static inline LLVMValueRef jit_get_context_field_ptr(JitContext *ctx, uint32_t field_idx, const char *name)
{
    LLVMValueRef indices[] = {
        LLVMConstInt(ctx->int_type, 0, 0),
        LLVMConstInt(ctx->int_type, field_idx, 0)
    };
    return LLVMBuildInBoundsGEP2(ctx->builder, ctx->exec_ctx_type, ctx->env_arg_param, indices, 2, name);
}

static inline LLVMValueRef jit_get_context_indexed_ptr(JitContext *ctx, uint32_t field_idx, uint32_t sub_idx, const char *name)
{
    LLVMValueRef indices[] = {
        LLVMConstInt(ctx->int_type, 0, 0),
        LLVMConstInt(ctx->int_type, field_idx, 0),
        LLVMConstInt(ctx->int_type, sub_idx, 0)
    };
    return LLVMBuildInBoundsGEP2(ctx->builder, ctx->exec_ctx_type, ctx->env_arg_param, indices, 3, name);
}

static LLVMValueRef jit_extract_scalar_u32(JitContext* ctx, LLVMValueRef val)
{
    if (!val) return LLVMConstInt(ctx->int_type, 0, 0);
    LLVMTypeRef t = LLVMTypeOf(val);
    if (LLVMGetTypeKind(t) == LLVMVectorTypeKind) 
    {
        val = LLVMBuildExtractElement(ctx->builder, val, LLVMConstInt(ctx->int_type, 0, 0), "ext_l0");
        t = LLVMTypeOf(val);
    }
    if (LLVMGetTypeKind(t) == LLVMFloatTypeKind || LLVMGetTypeKind(t) == LLVMDoubleTypeKind) 
    {
        return LLVMBuildFPToUI(ctx->builder, val, ctx->int_type, "f2u_s");
    }
    return val;
}

static uint32_t jit_extract_constant_index(JitContext *ctx, uint32_t id, LLVMValueRef val)
{
    if (id < ctx->bound && ctx->constant_uint_map[id] != 0)
    {
        return ctx->constant_uint_map[id];
    }
    if (val && LLVMIsConstant(val))
    {
        LLVMValueRef elem = val;
        if (LLVMGetTypeKind(LLVMTypeOf(elem)) == LLVMVectorTypeKind)
        {
            elem = LLVMGetAggregateElement(val, 0);
        }
        if (elem && LLVMIsConstant(elem))
        {
            LLVMTypeRef t = LLVMTypeOf(elem);
            if (LLVMGetTypeKind(t) == LLVMFloatTypeKind || LLVMGetTypeKind(t) == LLVMDoubleTypeKind)
            {
                LLVMBool loses;
                double d = LLVMConstRealGetDouble(elem, &loses);
                return (uint32_t)d;
            }
            else if (LLVMGetTypeKind(t) == LLVMIntegerTypeKind)
            {
                return (uint32_t)LLVMConstIntGetZExtValue(elem);
            }
        }
    }
    if (id < ctx->bound)
    {
        return ctx->constant_uint_map[id];
    }
    return 0;
}

static LLVMValueRef jit_to_i64_index(JitContext* ctx, LLVMValueRef idx_val)
{
    LLVMTypeRef i64_type = LLVMInt64TypeInContext(ctx->context);
    if (!idx_val) return LLVMConstInt(i64_type, 0, 0);
    LLVMTypeRef type = LLVMTypeOf(idx_val);
    if (LLVMGetTypeKind(type) == LLVMVectorTypeKind)
    {
        idx_val = LLVMBuildExtractElement(ctx->builder, idx_val, LLVMConstInt(ctx->int_type, 0, 0), "idx_s");
        type = LLVMTypeOf(idx_val);
    }
    if (LLVMGetTypeKind(type) == LLVMFloatTypeKind || LLVMGetTypeKind(type) == LLVMDoubleTypeKind)
    {
        return LLVMBuildFPToSI(ctx->builder, idx_val, i64_type, "idx_f2i");
    }
    return LLVMBuildZExt(ctx->builder, idx_val, i64_type, "idx64");
}

static LLVMValueRef jit_to_i64_index_vec(JitContext* ctx, LLVMValueRef idx_val)
{
    LLVMTypeRef i64_type = LLVMInt64TypeInContext(ctx->context);
    LLVMTypeRef vec_i64_type = LLVMVectorType(i64_type, SIMT_WIDTH);
    if (!idx_val) return LLVMConstNull(vec_i64_type);

    LLVMTypeRef type = LLVMTypeOf(idx_val);
    if (LLVMGetTypeKind(type) == LLVMVectorTypeKind)
    {
        LLVMTypeRef elem_type = LLVMGetElementType(type);
        if (LLVMGetTypeKind(elem_type) == LLVMFloatTypeKind || LLVMGetTypeKind(elem_type) == LLVMDoubleTypeKind)
        {
            return LLVMBuildFPToSI(ctx->builder, idx_val, vec_i64_type, "idx_vec_f2i");
        }
        return LLVMBuildSExt(ctx->builder, idx_val, vec_i64_type, "idx_vec_sext");
    }

    LLVMValueRef scalar_i64 = jit_to_i64_index(ctx, idx_val);
    LLVMValueRef res = LLVMGetUndef(vec_i64_type);
    for (int lane = 0; lane < SIMT_WIDTH; lane++)
    {
        res = LLVMBuildInsertElement(ctx->builder, res, scalar_i64, LLVMConstInt(ctx->int_type, lane, 0), "idx_vec_splat");
    }
    return res;
}

static LLVMValueRef splat_i64_to_vec(JitContext* ctx, uint64_t val)
{
    LLVMTypeRef i64_type = LLVMInt64TypeInContext(ctx->context);
    LLVMValueRef c = LLVMConstInt(i64_type, val, 0);
    LLVMValueRef vals[SIMT_WIDTH];
    for (int i = 0; i < SIMT_WIDTH; i++) vals[i] = c;
    return LLVMConstVector(vals, SIMT_WIDTH);
}

/* ========================================================================= */
/* Section 2: SIMT Pointer & Vector Math Utilities                           */
/* ========================================================================= */

static LLVMValueRef splat_ptr_to_vec(JitContext* ctx, LLVMValueRef ptr)
{
    if (LLVMGetTypeKind(LLVMTypeOf(ptr)) == LLVMVectorTypeKind)
        return ptr;
    LLVMValueRef res = LLVMGetUndef(ctx->vec_ptr_type);
    for (int lane = 0; lane < SIMT_WIDTH; lane++)
    {
        res = LLVMBuildInsertElement(ctx->builder, res, ptr, LLVMConstInt(ctx->int_type, lane, 0), "splat_ptr");
    }
    return res;
}

static LLVMValueRef offset_vec_ptr(JitContext* ctx, LLVMValueRef vec_ptr, uint32_t byte_offset)
{
    if (byte_offset == 0) return vec_ptr;
    LLVMTypeRef i8_type = LLVMInt8TypeInContext(ctx->context);
    LLVMTypeRef i64_type = LLVMInt64TypeInContext(ctx->context);
    LLVMValueRef off_val = LLVMConstInt(i64_type, byte_offset, 0);
    LLVMValueRef res = LLVMGetUndef(ctx->vec_ptr_type);
    for (int lane = 0; lane < SIMT_WIDTH; lane++)
    {
        LLVMValueRef lane_idx = LLVMConstInt(ctx->int_type, lane, 0);
        LLVMValueRef lane_ptr = LLVMBuildExtractElement(ctx->builder, vec_ptr, lane_idx, "lane_ptr");
        LLVMValueRef new_ptr = LLVMBuildGEP2(ctx->builder, i8_type, lane_ptr, &off_val, 1, "lane_ptr_off");
        res = LLVMBuildInsertElement(ctx->builder, res, new_ptr, lane_idx, "vec_ptr_ins");
    }
    return res;
}

/* ========================================================================= */
/* Section 3: SIMT Gather & Scatter Primitives                               */
/* ========================================================================= */

static LLVMValueRef emit_simt_scalar_gather(JitContext* ctx, SpvTypeInfo* info, LLVMValueRef vec_ptr)
{
    LLVMTypeRef target_vec_type = (info && info->opcode == SpvOpTypeBool) ? ctx->vec_i1_type : ctx->vec_float_type;
    LLVMValueRef res_vec = LLVMConstNull(target_vec_type);
    LLVMValueRef lane0_ptr = LLVMBuildExtractElement(ctx->builder, vec_ptr, LLVMConstInt(ctx->int_type, 0, 0), "gather_l0_ptr");

    for (int lane = 0; lane < SIMT_WIDTH; lane++)
    {
        LLVMValueRef lane_idx = LLVMConstInt(ctx->int_type, lane, 0);
        LLVMValueRef lane_active = LLVMBuildExtractElement(ctx->builder, ctx->emask, lane_idx, "gather_active");
        LLVMValueRef lane_ptr = LLVMBuildExtractElement(ctx->builder, vec_ptr, lane_idx, "gather_lane_ptr");
        LLVMValueRef safe_ptr = LLVMBuildSelect(ctx->builder, lane_active, lane_ptr, lane0_ptr, "safe_gather_ptr");

        LLVMValueRef val = NULL;
        if (!info || info->opcode == SpvOpTypeFloat)
        {
            val = LLVMBuildLoad2(ctx->builder, ctx->float_type, safe_ptr, "ld_f32");
            LLVMSetAlignment(val, 4);
        }
        else if (info->opcode == SpvOpTypeInt)
        {
            LLVMValueRef raw_i32 = LLVMBuildLoad2(ctx->builder, ctx->int_type, safe_ptr, "ld_i32");
            LLVMSetAlignment(raw_i32, 4);
            val = (info->member_count == 1)
                ? LLVMBuildSIToFP(ctx->builder, raw_i32, ctx->float_type, "ld_i2f")
                : LLVMBuildUIToFP(ctx->builder, raw_i32, ctx->float_type, "ld_u2f");
        }
        else
        {
            LLVMValueRef raw_i32 = LLVMBuildLoad2(ctx->builder, ctx->int_type, safe_ptr, "ld_b32");
            LLVMSetAlignment(raw_i32, 4);
            val = LLVMBuildICmp(ctx->builder, LLVMIntNE, raw_i32, LLVMConstInt(ctx->int_type, 0, 0), "ld_b2i1");
        }

        res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, val, lane_idx, "gather_ins");
    }
    return LLVMBuildSelect(ctx->builder, ctx->emask, res_vec, LLVMConstNull(target_vec_type), "gather_masked");
}

static void emit_simt_scalar_scatter(JitContext* ctx, SpvTypeInfo* info, LLVMValueRef val_to_store, LLVMValueRef vec_ptr, LLVMValueRef mask)
{
    for (int lane = 0; lane < SIMT_WIDTH; lane++)
    {
        LLVMValueRef lane_idx = LLVMConstInt(ctx->int_type, lane, 0);
        LLVMValueRef lane_active = LLVMBuildExtractElement(ctx->builder, mask, lane_idx, "scatter_active");
        LLVMBasicBlockRef active_bb = LLVMAppendBasicBlockInContext(ctx->context, ctx->func, "scatter_lane_bb");
        LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(ctx->context, ctx->func, "scatter_merge_bb");

        LLVMBuildCondBr(ctx->builder, lane_active, active_bb, merge_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, active_bb);
        LLVMValueRef lane_ptr = LLVMBuildExtractElement(ctx->builder, vec_ptr, lane_idx, "scatter_lane_ptr");
        LLVMValueRef lane_val = LLVMBuildExtractElement(ctx->builder, val_to_store, lane_idx, "scatter_lane_val");
        LLVMTypeRef val_type = LLVMTypeOf(lane_val);

        if (info && info->opcode == SpvOpTypeInt)
        {
            if (LLVMGetTypeKind(val_type) == LLVMFloatTypeKind)
            {
                lane_val = (info->member_count == 1)
                    ? LLVMBuildFPToSI(ctx->builder, lane_val, ctx->int_type, "st_f2i")
                    : LLVMBuildFPToUI(ctx->builder, lane_val, ctx->int_type, "st_f2u");
            }
        }
        else if (info && info->opcode == SpvOpTypeBool)
        {
            if (LLVMGetTypeKind(val_type) == LLVMIntegerTypeKind && LLVMGetIntTypeWidth(val_type) == 1)
            {
                lane_val = LLVMBuildZExt(ctx->builder, lane_val, ctx->int_type, "st_b2u32");
            }
            else if (LLVMGetTypeKind(val_type) == LLVMFloatTypeKind)
            {
                lane_val = LLVMBuildFPToUI(ctx->builder, lane_val, ctx->int_type, "st_fb2u32");
            }
        }

        LLVMValueRef st = LLVMBuildStore(ctx->builder, lane_val, lane_ptr);
        LLVMSetAlignment(st, 4);
        LLVMBuildBr(ctx->builder, merge_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
        ctx->current_block = merge_bb;
    }
}

/* ========================================================================= */
/* Section 4: Load & Store Pipelines (Scalar & SIMT Vector)                  */
/* ========================================================================= */

static LLVMValueRef build_recursive_load(JitContext *ctx, LLVMTypeRef type, LLVMValueRef ptr) 
{
    LLVMTypeKind kind = LLVMGetTypeKind(type);

    if (kind == LLVMVectorTypeKind) 
    {
        LLVMValueRef load = LLVMBuildLoad2(ctx->builder, type, ptr, "load_simt");
        LLVMSetAlignment(load, 4);
        return load;
    } 
    else if (kind == LLVMArrayTypeKind) 
    {
        uint32_t count = LLVMGetArrayLength(type);
        LLVMTypeRef elem_type = LLVMGetElementType(type);
        LLVMValueRef aggregate = LLVMGetUndef(type);

        for (uint32_t i = 0; i < count; i++) 
        {
            LLVMValueRef index = LLVMConstInt(LLVMInt32TypeInContext(ctx->context), i, 0);
            LLVMValueRef element_ptr = LLVMBuildInBoundsGEP2(ctx->builder, elem_type, ptr, &index, 1, "load_gep");
            LLVMValueRef element_val = build_recursive_load(ctx, elem_type, element_ptr);
            aggregate = LLVMBuildInsertValue(ctx->builder, aggregate, element_val, i, "insert_elem");
        }
        return aggregate;
    }
    else if (kind == LLVMStructTypeKind)
    {
        uint32_t count = LLVMCountStructElementTypes(type);
        LLVMValueRef aggregate = LLVMGetUndef(type);
        for (uint32_t i = 0; i < count; i++) 
        {
            LLVMTypeRef elem_type = LLVMStructGetTypeAtIndex(type, i);
            LLVMValueRef element_ptr = LLVMBuildStructGEP2(ctx->builder, type, ptr, i, "load_struct_gep");
            LLVMValueRef element_val = build_recursive_load(ctx, elem_type, element_ptr);
            aggregate = LLVMBuildInsertValue(ctx->builder, aggregate, element_val, i, "insert_struct_elem");
        }
        return aggregate;
    }

    LLVMValueRef load = LLVMBuildLoad2(ctx->builder, type, ptr, "load_scalar");
    LLVMSetAlignment(load, 4);
    return load;
}

static LLVMValueRef build_vec_ptr_load(JitContext* ctx, uint32_t spv_type_id, LLVMValueRef vec_ptr)
{
    SpvTypeInfo* info = (spv_type_id > 0 && spv_type_id < ctx->bound) ? &ctx->type_info[spv_type_id] : NULL;
    if (!info || info->opcode == SpvOpTypeFloat || info->opcode == SpvOpTypeInt || info->opcode == SpvOpTypeBool)
    {
        return emit_simt_scalar_gather(ctx, info, vec_ptr);
    }
    else if (info->opcode == SpvOpTypeVector || info->opcode == SpvOpTypeMatrix)
    {
        LLVMTypeRef agg_type = map_spv_to_llvm_type(ctx, spv_type_id);
        LLVMValueRef agg = LLVMGetUndef(agg_type);
        uint32_t elem_type_id = info->base_type_id;
        uint32_t elem_size = get_spv_natural_byte_size(ctx, elem_type_id);
        for (uint32_t i = 0; i < info->member_count; i++)
        {
            LLVMValueRef elem_ptr = offset_vec_ptr(ctx, vec_ptr, i * elem_size);
            LLVMValueRef elem_val = build_vec_ptr_load(ctx, elem_type_id, elem_ptr);
            agg = LLVMBuildInsertValue(ctx->builder, agg, elem_val, i, "gather_vec_ins");
        }
        return agg;
    }
    else if (info->opcode == SpvOpTypeArray)
    {
        LLVMTypeRef agg_type = map_spv_to_llvm_type(ctx, spv_type_id);
        LLVMValueRef agg = LLVMGetUndef(agg_type);
        uint32_t elem_type_id = info->base_type_id;
        uint32_t stride = (ctx->decorations[spv_type_id].array_stride > 0)
            ? (uint32_t)ctx->decorations[spv_type_id].array_stride
            : get_spv_natural_byte_size(ctx, elem_type_id);
        uint32_t count = LLVMGetArrayLength(agg_type);
        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem_ptr = offset_vec_ptr(ctx, vec_ptr, i * stride);
            LLVMValueRef elem_val = build_vec_ptr_load(ctx, elem_type_id, elem_ptr);
            agg = LLVMBuildInsertValue(ctx->builder, agg, elem_val, i, "gather_arr_ins");
        }
        return agg;
    }
    else if (info->opcode == SpvOpTypeStruct)
    {
        LLVMTypeRef agg_type = map_spv_to_llvm_type(ctx, spv_type_id);
        LLVMValueRef agg = LLVMGetUndef(agg_type);
        for (uint32_t i = 0; i < info->member_count; i++)
        {
            uint32_t m_off = get_spv_struct_member_offset(ctx, spv_type_id, i);
            LLVMValueRef m_ptr = offset_vec_ptr(ctx, vec_ptr, m_off);
            LLVMValueRef m_val = build_vec_ptr_load(ctx, info->member_types[i], m_ptr);
            agg = LLVMBuildInsertValue(ctx->builder, agg, m_val, i, "gather_struct_ins");
        }
        return agg;
    }
    return LLVMConstNull(map_spv_to_llvm_type(ctx, spv_type_id));
}

void handle_op_load(JitContext* ctx, uint32_t res_id, uint32_t res_type_id, uint32_t* operands) 
{
    uint32_t ptr_id = operands[0];
    LLVMValueRef ptr = get_val(ctx, ptr_id);
    
    SpvTypeInfo* type_info = &ctx->type_info[res_type_id];
    if (type_info->opcode == SpvOpTypeSampledImage || 
        type_info->opcode == SpvOpTypeImage || 
        type_info->opcode == SpvOpTypeSampler)
    {
        set_val(ctx, res_id, ptr);
        return;
    }

    LLVMTypeRef result_llvm_type = map_spv_to_llvm_type(ctx, res_type_id);

    if (LLVMIsConstant(ptr) && LLVMIsNull(ptr)) 
    {
        set_val(ctx, res_id, LLVMConstNull(result_llvm_type));
        return;
    }

    uint32_t ptr_type_id = (ptr_id < ctx->bound) ? ctx->var_type_map[ptr_id] : 0;
    uint32_t storage_class = (ptr_type_id > 0 && ptr_type_id < ctx->bound) ? ctx->type_info[ptr_type_id].storage_class : 0;
    if (LLVMGetTypeKind(LLVMTypeOf(ptr)) == LLVMVectorTypeKind ||
        storage_class == SpvStorageClassTaskPayloadWorkgroupEXT)
    {
        LLVMValueRef vec_ptr = splat_ptr_to_vec(ctx, ptr);
        LLVMValueRef final_val = build_vec_ptr_load(ctx, res_type_id, vec_ptr);
        set_val(ctx, res_id, final_val);
        return;
    }

    LLVMValueRef final_val = build_recursive_load(ctx, result_llvm_type, ptr);
    set_val(ctx, res_id, final_val);
}

static void build_recursive_store(JitContext *ctx, LLVMValueRef val_to_store, LLVMValueRef ptr, LLVMValueRef mask)
{
    LLVMTypeRef type = LLVMTypeOf(val_to_store);
    LLVMTypeKind kind = LLVMGetTypeKind(type);

    if (kind == LLVMVectorTypeKind) 
    {
        LLVMValueRef current_mem_val = LLVMBuildLoad2(ctx->builder, type, ptr, "current_mem");
        LLVMSetAlignment(current_mem_val, 4);
        
        LLVMValueRef masked_val = LLVMBuildSelect(ctx->builder, mask, val_to_store, current_mem_val, "masked_val");
        LLVMValueRef store_inst = LLVMBuildStore(ctx->builder, masked_val, ptr);
        LLVMSetAlignment(store_inst, 4);
    } 
    else if (kind == LLVMArrayTypeKind) 
    {
        uint32_t count = LLVMGetArrayLength(type);
        LLVMTypeRef elem_type = LLVMGetElementType(type);

        for (uint32_t i = 0; i < count; i++) 
        {
            LLVMValueRef index = LLVMConstInt(LLVMInt32TypeInContext(ctx->context), i, 0);
            LLVMValueRef element_ptr = LLVMBuildInBoundsGEP2(ctx->builder, elem_type, ptr, &index, 1, "store_gep");
            LLVMValueRef element_val = LLVMBuildExtractValue(ctx->builder, val_to_store, i, "extract_elem");
            build_recursive_store(ctx, element_val, element_ptr, mask);
        }
    }
    else if (kind == LLVMStructTypeKind)
    {
        uint32_t count = LLVMCountStructElementTypes(type);
        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef element_ptr = LLVMBuildStructGEP2(ctx->builder, type, ptr, i, "store_struct_gep");
            LLVMValueRef element_val = LLVMBuildExtractValue(ctx->builder, val_to_store, i, "extract_struct_elem");
            build_recursive_store(ctx, element_val, element_ptr, mask);
        }
    }
    else
    {
        LLVMValueRef store_inst = LLVMBuildStore(ctx->builder, val_to_store, ptr);
        LLVMSetAlignment(store_inst, 4);
    }
}

static void build_vec_ptr_store(JitContext* ctx, uint32_t spv_type_id, LLVMValueRef val_to_store, LLVMValueRef vec_ptr, LLVMValueRef mask)
{
    SpvTypeInfo* info = (spv_type_id > 0 && spv_type_id < ctx->bound) ? &ctx->type_info[spv_type_id] : NULL;
    if (!info || info->opcode == SpvOpTypeFloat || info->opcode == SpvOpTypeInt || info->opcode == SpvOpTypeBool)
    {
        emit_simt_scalar_scatter(ctx, info, val_to_store, vec_ptr, mask);
    }
    else if (info->opcode == SpvOpTypeVector || info->opcode == SpvOpTypeMatrix)
    {
        uint32_t elem_type_id = info->base_type_id;
        uint32_t elem_size = get_spv_natural_byte_size(ctx, elem_type_id);
        for (uint32_t i = 0; i < info->member_count; i++)
        {
            LLVMValueRef elem_ptr = offset_vec_ptr(ctx, vec_ptr, i * elem_size);
            LLVMValueRef elem_val = LLVMBuildExtractValue(ctx->builder, val_to_store, i, "scatter_vec_ext");
            build_vec_ptr_store(ctx, elem_type_id, elem_val, elem_ptr, mask);
        }
    }
    else if (info->opcode == SpvOpTypeArray)
    {
        uint32_t elem_type_id = info->base_type_id;
        uint32_t stride = (ctx->decorations[spv_type_id].array_stride > 0)
            ? (uint32_t)ctx->decorations[spv_type_id].array_stride
            : get_spv_natural_byte_size(ctx, elem_type_id);
        uint32_t count = LLVMGetArrayLength(LLVMTypeOf(val_to_store));
        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem_ptr = offset_vec_ptr(ctx, vec_ptr, i * stride);
            LLVMValueRef elem_val = LLVMBuildExtractValue(ctx->builder, val_to_store, i, "scatter_arr_ext");
            build_vec_ptr_store(ctx, elem_type_id, elem_val, elem_ptr, mask);
        }
    }
    else if (info->opcode == SpvOpTypeStruct)
    {
        for (uint32_t i = 0; i < info->member_count; i++)
        {
            uint32_t m_off = get_spv_struct_member_offset(ctx, spv_type_id, i);
            LLVMValueRef m_ptr = offset_vec_ptr(ctx, vec_ptr, m_off);
            LLVMValueRef m_val = LLVMBuildExtractValue(ctx->builder, val_to_store, i, "scatter_struct_ext");
            build_vec_ptr_store(ctx, info->member_types[i], m_val, m_ptr, mask);
        }
    }
}

void handle_op_store(JitContext* ctx, uint32_t* operands)
{
    uint32_t ptr_id = operands[0];
    uint32_t val_id = operands[1];

    LLVMValueRef ptr = get_val(ctx, ptr_id);
    LLVMValueRef val = get_val(ctx, val_id);

    uint32_t ptr_type_id = (ptr_id < ctx->bound) ? ctx->var_type_map[ptr_id] : 0;
    uint32_t storage_class = (ptr_type_id > 0 && ptr_type_id < ctx->bound) ? ctx->type_info[ptr_type_id].storage_class : 0;
    if (LLVMGetTypeKind(LLVMTypeOf(ptr)) == LLVMVectorTypeKind ||
        storage_class == SpvStorageClassTaskPayloadWorkgroupEXT)
    {
        uint32_t pointee_type_id = (ptr_type_id > 0 && ptr_type_id < ctx->bound)
            ? ctx->type_info[ptr_type_id].base_type_id
            : 0;
        LLVMValueRef vec_ptr = splat_ptr_to_vec(ctx, ptr);
        build_vec_ptr_store(ctx, pointee_type_id, val, vec_ptr, ctx->emask);
        return;
    }

    build_recursive_store(ctx, val, ptr, ctx->emask);
}

/* ========================================================================= */
/* Section 5: Pointer Math & Access Chains                                   */
/* ========================================================================= */

uint32_t get_spv_type_size(uint32_t spv_type_id)
{
    (void)spv_type_id;
    return 4;
}

static uint32_t get_struct_member_byte_offset(JitContext* ctx, uint32_t struct_type_id, uint32_t member_idx, uint32_t storage_class)
{
    uint32_t offset = get_spv_struct_member_offset(ctx, struct_type_id, member_idx);
    if (storage_class == SpvStorageClassFunction || storage_class == SpvStorageClassPrivate)
    {
        return SIMT_WIDTH * offset;
    }
    return offset;
}

static uint32_t get_access_chain_element_stride(
    JitContext* ctx,
    uint32_t type_id,
    uint32_t base_id,
    int operand_idx,
    uint32_t storage_class,
    bool is_vec_mode)
{
    SpvTypeInfo* info = &ctx->type_info[type_id];
    if (info->opcode == SpvOpTypeArray || info->opcode == SpvOpTypeRuntimeArray)
    {
        if (ctx->shader_type == MESH_SHADER &&
            storage_class == SpvStorageClassOutput &&
            operand_idx == 1 &&
            ctx->decorations[base_id].location >= 0)
        {
            uint32_t elem_sz = get_spv_natural_byte_size(ctx, info->base_type_id);
            return (elem_sz > 16) ? elem_sz : 16;
        }
        if (ctx->decorations[type_id].array_stride > 0)
        {
            return (uint32_t)ctx->decorations[type_id].array_stride;
        }
        uint32_t natural_sz = get_spv_natural_byte_size(ctx, info->base_type_id);
        if (!is_vec_mode && (storage_class == SpvStorageClassFunction || storage_class == SpvStorageClassPrivate))
        {
            natural_sz *= SIMT_WIDTH;
        }
        return (natural_sz > 0) ? natural_sz : 4;
    }
    else if (info->opcode == SpvOpTypeVector || info->opcode == SpvOpTypeMatrix)
    {
        uint32_t natural_sz = get_spv_natural_byte_size(ctx, info->base_type_id);
        if (!is_vec_mode &&
            (storage_class == SpvStorageClassOutput ||
             storage_class == SpvStorageClassInput ||
             storage_class == SpvStorageClassFunction ||
             storage_class == SpvStorageClassPrivate))
        {
            natural_sz *= SIMT_WIDTH;
        }
        return (natural_sz > 0) ? natural_sz : 4;
    }
    return 4;
}

void handle_op_access_chain(JitContext *ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands, int operand_count) 
{
    uint32_t base_id = operands[0];
    LLVMValueRef base_ptr = get_val(ctx, base_id);
    if (!base_ptr)
    {
        base_ptr = LLVMConstNull(ctx->ptr_type);
    }

    LLVMTypeRef i64_type = LLVMInt64TypeInContext(ctx->context);
    LLVMTypeRef i8_type = LLVMInt8TypeInContext(ctx->context);

    uint32_t base_ptr_type = (base_id < ctx->bound && ctx->var_type_map[base_id] != 0)
                                ? ctx->var_type_map[base_id]
                                : ctx->type_kind_map[base_id];
    uint32_t current_type_id = (base_ptr_type != 0 && ctx->type_info[base_ptr_type].opcode == SpvOpTypePointer) 
                                ? ctx->type_info[base_ptr_type].base_type_id 
                                : ctx->type_info[type_id].base_type_id;

    uint32_t storage_class = ctx->type_info[type_id].storage_class;
    bool use_vec_ptr = (LLVMGetTypeKind(LLVMTypeOf(base_ptr)) == LLVMVectorTypeKind) ||
                       (storage_class == SpvStorageClassTaskPayloadWorkgroupEXT) ||
                       ((ctx->shader_type == TASK_SHADER || ctx->shader_type == MESH_SHADER) &&
                        (storage_class == SpvStorageClassOutput ||
                         storage_class == SpvStorageClassStorageBuffer ||
                         storage_class == SpvStorageClassUniform ||
                         storage_class == SpvStorageClassWorkgroup));

    if (use_vec_ptr)
    {
        LLVMTypeRef vec_i64_type = LLVMVectorType(i64_type, SIMT_WIDTH);
        LLVMValueRef total_offset_vec = LLVMConstNull(vec_i64_type);

        for (int i = 1; i < operand_count; i++)
        {
            uint32_t index_id = operands[i];
            LLVMValueRef idx_val = get_val(ctx, index_id);
            if (!idx_val) idx_val = LLVMConstInt(ctx->int_type, 0, 0);

            SpvTypeInfo* info = &ctx->type_info[current_type_id];
            if (info->opcode == SpvOpTypeStruct)
            {
                uint32_t member_idx = jit_extract_constant_index(ctx, index_id, idx_val);
                uint32_t offset_bytes = get_struct_member_byte_offset(ctx, current_type_id, member_idx, storage_class);
                total_offset_vec = LLVMBuildAdd(ctx->builder, total_offset_vec, splat_i64_to_vec(ctx, offset_bytes), "v_struct_off");
                if (info->member_types && member_idx < info->member_count)
                {
                    current_type_id = info->member_types[member_idx];
                }
            }
            else
            {
                LLVMValueRef idx_vec = jit_to_i64_index_vec(ctx, idx_val);
                uint32_t stride = get_access_chain_element_stride(ctx, current_type_id, base_id, i, storage_class, true);
                LLVMValueRef step_vec = LLVMBuildMul(ctx->builder, idx_vec, splat_i64_to_vec(ctx, stride), "v_step");
                total_offset_vec = LLVMBuildAdd(ctx->builder, total_offset_vec, step_vec, "v_elem_off");
                current_type_id = info->base_type_id;
            }
        }

        LLVMValueRef base_vec_ptr = splat_ptr_to_vec(ctx, base_ptr);
        LLVMValueRef final_vec_ptr = LLVMGetUndef(ctx->vec_ptr_type);
        for (int lane = 0; lane < SIMT_WIDTH; lane++)
        {
            LLVMValueRef lane_idx = LLVMConstInt(ctx->int_type, lane, 0);
            LLVMValueRef lp = LLVMBuildExtractElement(ctx->builder, base_vec_ptr, lane_idx, "ac_base_lp");
            LLVMValueRef lo = LLVMBuildExtractElement(ctx->builder, total_offset_vec, lane_idx, "ac_off_lo");
            LLVMValueRef gep = LLVMBuildGEP2(ctx->builder, i8_type, lp, &lo, 1, "ac_lane_gep");
            final_vec_ptr = LLVMBuildInsertElement(ctx->builder, final_vec_ptr, gep, lane_idx, "ac_vec_ptr");
        }

        set_val(ctx, res_id, final_vec_ptr);
    }
    else
    {
        LLVMValueRef total_offset = LLVMConstInt(i64_type, 0, 0);

        for (int i = 1; i < operand_count; i++)
        {
            uint32_t index_id = operands[i];
            LLVMValueRef idx_val = get_val(ctx, index_id);
            if (!idx_val) idx_val = LLVMConstInt(ctx->int_type, 0, 0);

            SpvTypeInfo* info = &ctx->type_info[current_type_id];
            if (info->opcode == SpvOpTypeStruct)
            {
                uint32_t member_idx = jit_extract_constant_index(ctx, index_id, idx_val);
                uint32_t offset_bytes = get_struct_member_byte_offset(ctx, current_type_id, member_idx, storage_class);
                total_offset = LLVMBuildAdd(ctx->builder, total_offset, LLVMConstInt(i64_type, offset_bytes, 0), "s_struct_off");
                if (info->member_types && member_idx < info->member_count)
                {
                    current_type_id = info->member_types[member_idx];
                }
            }
            else
            {
                LLVMValueRef idx_scalar = jit_to_i64_index(ctx, idx_val);
                uint32_t stride = get_access_chain_element_stride(ctx, current_type_id, base_id, i, storage_class, false);
                LLVMValueRef step = LLVMBuildMul(ctx->builder, idx_scalar, LLVMConstInt(i64_type, stride, 0), "s_step");
                total_offset = LLVMBuildAdd(ctx->builder, total_offset, step, "s_elem_off");
                current_type_id = info->base_type_id;
            }
        }

        LLVMValueRef gep_indices[] = { total_offset };
        LLVMValueRef final_ptr = LLVMBuildGEP2(ctx->builder, i8_type, base_ptr, gep_indices, 1, "access_chain_ptr");
        set_val(ctx, res_id, final_ptr);
    }

    ctx->type_kind_map[res_id] = ctx->type_kind_map[base_id];
    ctx->var_type_map[res_id] = type_id;
}

/* ========================================================================= */
/* Section 6: Variable Allocation & Mesh/Task Built-ins                      */
/* ========================================================================= */

void handle_op_variable(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t opCount, uint32_t* operands) 
{
    uint32_t storage_class = operands[0];
    SpvDecoInfo* deco = &ctx->decorations[res_id];
    
    ctx->type_kind_map[res_id] = type_id;
    ctx->var_type_map[res_id] = type_id;

    // --- CASE A: GLOBAL SCOPE (Module Level) ---
    if (ctx->func == NULL) 
    {
        if (storage_class == SpvStorageClassOutput &&
            deco->location >= 0 && deco->location < MAX_ATTRIBUTES)
        {
            ctx->output_location_mask |= 1u << deco->location;
        }

        if (ctx->global_count < MAX_GLOBALS) 
        {
            uint32_t idx = ctx->global_count++;
            ctx->globals[idx].res_id = res_id;
            ctx->globals[idx].storage_class = storage_class;
            ctx->globals[idx].base_type = type_id;
            
            if (storage_class == SpvStorageClassUniform || 
                storage_class == SpvStorageClassStorageBuffer || 
                storage_class == SpvStorageClassUniformConstant)
            {
                ctx->globals[idx].binding_or_loc = deco->binding;
            }
            else if (storage_class == SpvStorageClassWorkgroup)
            {
                uint32_t offset = ctx->shared_mem_offset;
                ctx->shared_mem_offset += (1024 + 63) & ~63;
                ctx->globals[idx].binding_or_loc = offset;
            }
            else
            {
                ctx->globals[idx].binding_or_loc = deco->location;
            }
        }
        else 
        {
            DEBUG_PRINT("Error: Exceeded MAX_PENDING_GLOBALS\n");
        }
        return;
    }

    // --- CASE B: FUNCTION SCOPE (Inside a Function) ---
    if (storage_class == SpvStorageClassFunction || storage_class == SpvStorageClassPrivate) 
    {
        LLVMTypeRef type = map_spv_to_llvm_type(ctx, type_id);
        LLVMValueRef alloca_inst = LLVMBuildAlloca(ctx->builder, type, "local_var");
        LLVMSetAlignment(alloca_inst, 64);
        
        if (opCount > 1) 
        {
            uint32_t init_id = operands[1];
            LLVMValueRef init_val = ctx->id_val_map[init_id];
            if (init_val != NULL) 
            {
                LLVMBuildStore(ctx->builder, init_val, alloca_inst);
            }
        }
        
        set_val(ctx, res_id, alloca_inst);
    }
    else if (storage_class == SpvStorageClassUniform || 
             storage_class == SpvStorageClassStorageBuffer ||
             storage_class == SpvStorageClassUniformConstant ||
             storage_class == SpvStorageClassInput || 
             storage_class == SpvStorageClassOutput)
    {
        uint32_t field_idx = ECTX_FIELD_BINDINGS;
        int32_t offset = 0;

        if (storage_class == SpvStorageClassUniform || 
            storage_class == SpvStorageClassStorageBuffer || 
            storage_class == SpvStorageClassUniformConstant)
        { 
            field_idx = ECTX_FIELD_BINDINGS; 
            offset = deco->binding; 
        }
        else if (storage_class == SpvStorageClassInput)
        { 
            field_idx = ECTX_FIELD_LOC_IN; 
            offset = deco->location; 
        }
        else if (storage_class == SpvStorageClassOutput)
        { 
            field_idx = ECTX_FIELD_LOC_OUT; 
            offset = deco->location; 
        }

        if (offset >= 0) 
        {
            LLVMValueRef slot_ptr = jit_get_context_indexed_ptr(ctx, field_idx, (uint32_t)offset, "res_slot");
            LLVMValueRef actual_ptr = LLVMBuildLoad2(ctx->builder, ctx->ptr_type, slot_ptr, "res_ptr");
            set_val(ctx, res_id, actual_ptr);
        }
    }
    else 
    {
        DEBUG_PRINT("Warning: Unhandled Storage Class %u in function scope\n", storage_class);
    }
}

void handle_op_set_mesh_out(JitContext* ctx, uint32_t* operands)
{
    LLVMValueRef v_cnt = jit_extract_scalar_u32(ctx, get_val(ctx, operands[0]));
    LLVMValueRef p_cnt = jit_extract_scalar_u32(ctx, get_val(ctx, operands[1]));
    LLVMValueRef lane0_active = LLVMBuildExtractElement(
        ctx->builder, jit_get_emask(ctx), LLVMConstInt(ctx->int_type, 0, 0), "lane0_active");

    LLVMValueRef mesh_slot = jit_get_context_field_ptr(ctx, ECTX_FIELD_MESH_OUTPUT, "mesh_slot");
    LLVMValueRef mesh_base = LLVMBuildLoad2(ctx->builder, ctx->ptr_type, mesh_slot, "mesh_base");

    LLVMTypeRef i8_type = LLVMInt8TypeInContext(ctx->context);
    LLVMTypeRef i64_type = LLVMInt64TypeInContext(ctx->context);
    LLVMValueRef v_off = LLVMConstInt(i64_type, offsetof(BuiltinMeshOutput, vertex_count), 0);
    LLVMValueRef p_off = LLVMConstInt(i64_type, offsetof(BuiltinMeshOutput, primitive_count), 0);

    LLVMValueRef v_ptr = LLVMBuildGEP2(ctx->builder, i8_type, mesh_base, &v_off, 1, "v_cnt_ptr");
    LLVMValueRef p_ptr = LLVMBuildGEP2(ctx->builder, i8_type, mesh_base, &p_off, 1, "p_cnt_ptr");

    LLVMValueRef old_v = LLVMBuildLoad2(ctx->builder, ctx->int_type, v_ptr, "old_v_cnt");
    LLVMValueRef old_p = LLVMBuildLoad2(ctx->builder, ctx->int_type, p_ptr, "old_p_cnt");

    LLVMBuildStore(ctx->builder, LLVMBuildSelect(ctx->builder, lane0_active, v_cnt, old_v, "sel_v_cnt"), v_ptr);
    LLVMBuildStore(ctx->builder, LLVMBuildSelect(ctx->builder, lane0_active, p_cnt, old_p, "sel_p_cnt"), p_ptr);
}

void handle_op_emit_mesh_task(JitContext* ctx, uint32_t* operands)
{
    LLVMValueRef lane0_active = LLVMBuildExtractElement(
        ctx->builder, jit_get_emask(ctx), LLVMConstInt(ctx->int_type, 0, 0), "lane0_active");

    for (int k = 0; k < 3; k++) 
    {
        LLVMValueRef g_cnt = jit_extract_scalar_u32(ctx, get_val(ctx, operands[k]));
        LLVMValueRef slot = jit_get_context_indexed_ptr(ctx, ECTX_FIELD_TASK_GROUPS, (uint32_t)k, "task_grp_slot");
        LLVMValueRef old_g = LLVMBuildLoad2(ctx->builder, ctx->int_type, slot, "old_g_cnt");
        LLVMBuildStore(ctx->builder, LLVMBuildSelect(ctx->builder, lane0_active, g_cnt, old_g, "sel_g_cnt"), slot);
    }
    LLVMBuildRetVoid(ctx->builder);
}
