#include "jit_alu.h"
#include "jit_atomic.h"
#include "glsl_std_450.h"
#include <llvm-c/Core.h>
#include <math.h>
#include "debug_gpu.h"

#define CREATE_CONST_VEC(name, val) \
    for(int i = 0; i < SIMT_WIDTH; i++) scalars[i] = LLVMConstReal(f32_type, val); \
    LLVMValueRef name = LLVMConstVector(scalars, SIMT_WIDTH);
   
#define CREATE_CONST_VEC_N(name, val,size) \
    for(int i = 0; i < size; i++) scalars[i] = LLVMConstReal(f32_type, val); \
    LLVMValueRef name = LLVMConstVector(scalars, size);
    
static bool is_float_type(LLVMTypeRef type) {
    if (!type) return false;
    LLVMTypeKind kind = LLVMGetTypeKind(type);
    if (kind == LLVMFloatTypeKind || kind == LLVMDoubleTypeKind)
        return true;
    if (kind == LLVMVectorTypeKind) {
        LLVMTypeRef elem = LLVMGetElementType(type);
        LLVMTypeKind elem_kind = LLVMGetTypeKind(elem);
        return (elem_kind == LLVMFloatTypeKind || elem_kind == LLVMDoubleTypeKind);
    }
    return false;
}

void handle_op_constant(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands) 
{
    uint32_t kind = ctx->type_kind_map[type_id];
    LLVMValueRef const_val;

    if (kind == SpvOpTypeFloat)
    {
        float val = *(float*)&operands[0];
        const_val = LLVMConstReal(ctx->float_type, val);

        LLVMValueRef* vals = malloc(sizeof(LLVMValueRef) * SIMT_WIDTH);
        for (int i = 0; i < SIMT_WIDTH; i++) 
        {
            vals[i] = const_val;
        }

        LLVMValueRef vec_val = LLVMConstVector(vals, SIMT_WIDTH);
        free(vals);
        set_val(ctx, res_id, vec_val);
        return;
    }

    if (kind == SpvOpTypeInt)
    {
        if (res_id < ctx->bound) 
        {
            ctx->constant_uint_map[res_id] = operands[0];
        }
        if (ctx->shader_info.local_size_id[0] == res_id) ctx->shader_info.local_size_x = operands[0];
        if (ctx->shader_info.local_size_id[1] == res_id) ctx->shader_info.local_size_y = operands[0];
        if (ctx->shader_info.local_size_id[2] == res_id) ctx->shader_info.local_size_z = operands[0];

        float val = (ctx->type_info[type_id].member_count == 1)
            ? (float)(*(int32_t*)&operands[0])
            : (float)operands[0];
        const_val = LLVMConstReal(ctx->float_type, val);

        LLVMValueRef* vals = malloc(sizeof(LLVMValueRef) * SIMT_WIDTH);
        for (int i = 0; i < SIMT_WIDTH; i++) 
        {
            vals[i] = const_val;
        }

        LLVMValueRef vec_val = LLVMConstVector(vals, SIMT_WIDTH);
        free(vals);
        set_val(ctx, res_id, vec_val);
        return;
    }

    if (kind == SpvOpTypeBool)
    {
        uint32_t val = operands[0] != 0;
        const_val = LLVMConstInt(LLVMInt1TypeInContext(ctx->context), val, 0);

        LLVMValueRef* vals = malloc(sizeof(LLVMValueRef) * SIMT_WIDTH);
        for (int i = 0; i < SIMT_WIDTH; i++) 
        {
            vals[i] = const_val;
        }

        LLVMValueRef vec_val = LLVMConstVector(vals, SIMT_WIDTH);
        free(vals);
        set_val(ctx, res_id, vec_val);
        return;
    }

    int32_t val = *(int32_t*)&operands[0];
    const_val = LLVMConstInt(ctx->int_type, (unsigned long long)val, 1);

    LLVMValueRef* vals = malloc(sizeof(LLVMValueRef) * SIMT_WIDTH);
    for (int i = 0; i < SIMT_WIDTH; i++) 
    {
        vals[i] = const_val;
    }

    LLVMValueRef vec_val = LLVMConstVector(vals, SIMT_WIDTH);
    free(vals);
    
    set_val(ctx, res_id, vec_val);
}
LLVMValueRef vec_mat_helper(JitContext* ctx, LLVMValueRef a, LLVMValueRef b, LLVMMatFunc_t func, const char* name)
{
    LLVMTypeRef vec_type = LLVMTypeOf(a);
    uint32_t num_elements = LLVMGetArrayLength(vec_type);

    LLVMValueRef res = LLVMGetUndef(vec_type);
    for (uint32_t i = 0; i < num_elements; i++) 
    {
        LLVMValueRef a_elem = LLVMBuildExtractValue(ctx->builder, a, i, "a_elem");
        LLVMValueRef b_elem = LLVMBuildExtractValue(ctx->builder, b, i, "b_elem");
        
        LLVMValueRef operation_result = func(ctx->builder, a_elem, b_elem, name);
        res = LLVMBuildInsertValue(ctx->builder, res, operation_result, i, "pack_comp");

    }
    return res;
}
LLVMValueRef mat_operation_helper(JitContext* ctx, LLVMValueRef a, LLVMValueRef b, LLVMMatFunc_t func, const char* name)
{
    LLVMValueRef res = NULL;
    
    uint8_t is_vector = LLVMGetTypeKind(LLVMTypeOf(a)) == LLVMArrayTypeKind;
    
    if (is_vector)
    {
        res = vec_mat_helper(ctx, a, b, func, name);
    }
    else
    {
       res = func(ctx->builder, b, a, name);
    }
    return res;
}
void handle_op_fadd(JitContext* ctx, uint32_t res_id, uint32_t* operands) 
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = mat_operation_helper(ctx, lhs, rhs, LLVMBuildFAdd, "add");
    set_val(ctx, res_id, res);
}

void handle_op_fmul(JitContext* ctx, uint32_t res_id, uint32_t* operands) 
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = mat_operation_helper(ctx, lhs, rhs, LLVMBuildFMul, "mul");
    set_val(ctx, res_id, res);
}

void handle_op_fdiv(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMGetTypeKind(LLVMTypeOf(lhs)) == LLVMVectorTypeKind ?
        LLVMBuildFDiv(ctx->builder, lhs, rhs, "div") :
        mat_operation_helper(ctx, lhs, rhs, LLVMBuildFDiv, "div");
    set_val(ctx, res_id, res);
}
void handle_op_fsub(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = mat_operation_helper(ctx, lhs, rhs, LLVMBuildFSub, "sub");
    set_val(ctx, res_id, res);
}
void handle_op_fneg(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef op = get_val(ctx, operands[0]);
    LLVMValueRef res = LLVMBuildFNeg(ctx->builder, op, "v_fneg");
    set_val(ctx, res_id, res);
}
void handle_op_isub(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = is_float_type(LLVMTypeOf(lhs)) ?
        LLVMBuildFSub(ctx->builder, lhs, rhs, "v_isub") :
        LLVMBuildSub(ctx->builder, lhs, rhs, "v_isub");
    set_val(ctx, res_id, res);
}
void handle_op_imul(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = is_float_type(LLVMTypeOf(lhs)) ?
        LLVMBuildFMul(ctx->builder, lhs, rhs, "v_imul") :
        LLVMBuildMul(ctx->builder, lhs, rhs, "v_imul");
    set_val(ctx, res_id, res);
}
void handle_op_sdiv(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = is_float_type(LLVMTypeOf(lhs)) ?
        LLVMBuildFDiv(ctx->builder, lhs, rhs, "v_sdiv") :
        LLVMBuildSDiv(ctx->builder, lhs, rhs, "v_sdiv");
    set_val(ctx, res_id, res);
}

void handle_op_udiv(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    if (is_float_type(LLVMTypeOf(lhs))) {
        LLVMValueRef lhs_int = jit_to_numeric_int_vector(ctx, lhs);
        LLVMValueRef rhs_int = jit_to_numeric_int_vector(ctx, rhs);
        LLVMValueRef quotient = LLVMBuildUDiv(ctx->builder, lhs_int, rhs_int, "v_udiv");
        set_val(ctx, res_id, LLVMBuildUIToFP(ctx->builder, quotient, ctx->vec_float_type, "v_udiv_f"));
        return;
    }
    set_val(ctx, res_id, LLVMBuildUDiv(ctx->builder, lhs, rhs, "v_udiv"));
}
void handle_op_iadd(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = is_float_type(LLVMTypeOf(lhs)) ?
        LLVMBuildFAdd(ctx->builder, lhs, rhs, "v_iadd") :
        LLVMBuildAdd(ctx->builder, lhs, rhs, "v_iadd");
    set_val(ctx, res_id, res);
}

void handle_op_sitof(JitContext* ctx, uint32_t res_id, uint32_t* operands) 
{
    LLVMValueRef op = get_val(ctx, operands[0]); 
    if (is_float_type(LLVMTypeOf(op))) {
        set_val(ctx, res_id, op);
        return;
    }
    LLVMValueRef res = LLVMBuildSIToFP(ctx->builder, op, ctx->vec_float_type, "v_sitof");
    set_val(ctx, res_id, res);
}
void handle_op_select(JitContext* ctx, uint32_t res_id, uint32_t* operands) 
{
    LLVMValueRef cond = get_val(ctx, operands[0]); // Vector of i1 or array of vectors
    LLVMValueRef true_val = get_val(ctx, operands[1]);
    LLVMValueRef false_val = get_val(ctx, operands[2]);
    LLVMTypeRef cond_type = LLVMTypeOf(cond);

    if (LLVMGetTypeKind(cond_type) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(cond_type);
        LLVMTypeRef res_type = LLVMTypeOf(true_val);
        LLVMValueRef res = LLVMGetUndef(res_type);
        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem_c = LLVMBuildExtractValue(ctx->builder, cond, i, "sel_c");
            LLVMValueRef elem_t = (LLVMGetTypeKind(LLVMTypeOf(true_val)) == LLVMArrayTypeKind)
                ? LLVMBuildExtractValue(ctx->builder, true_val, i, "sel_t")
                : true_val;
            LLVMValueRef elem_f = (LLVMGetTypeKind(LLVMTypeOf(false_val)) == LLVMArrayTypeKind)
                ? LLVMBuildExtractValue(ctx->builder, false_val, i, "sel_f")
                : false_val;
            LLVMValueRef elem_res = LLVMBuildSelect(ctx->builder, elem_c, elem_t, elem_f, "v_select");
            res = LLVMBuildInsertValue(ctx->builder, res, elem_res, i, "sel_res");
        }
        set_val(ctx, res_id, res);
        return;
    }

    LLVMValueRef res = LLVMBuildSelect(ctx->builder, cond, true_val, false_val, "v_select");
    set_val(ctx, res_id, res);
}

void handle_op_slessthan(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);

    LLVMValueRef cmp = is_float_type(LLVMTypeOf(lhs)) ?
        LLVMBuildFCmp(ctx->builder, LLVMRealOLT, lhs, rhs, "v_slt") :
        LLVMBuildICmp(ctx->builder, LLVMIntSLT, lhs, rhs, "v_slt");

    set_val(ctx, res_id, cmp);
}

void handle_op_ulessthan(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);

    LLVMValueRef cmp = is_float_type(LLVMTypeOf(lhs)) ?
        LLVMBuildFCmp(ctx->builder, LLVMRealOLT, lhs, rhs, "v_ult") :
        LLVMBuildICmp(ctx->builder, LLVMIntULT, lhs, rhs, "v_ult");

    set_val(ctx, res_id, cmp);
}

void handle_op_iequal(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);

    LLVMValueRef cmp = is_float_type(LLVMTypeOf(lhs)) ?
        LLVMBuildFCmp(ctx->builder, LLVMRealOEQ, lhs, rhs, "v_ieq") :
        LLVMBuildICmp(ctx->builder, LLVMIntEQ, lhs, rhs, "v_ieq");

    set_val(ctx, res_id, cmp);
}

void handle_op_fordlessthan(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);

    LLVMValueRef cmp = LLVMBuildFCmp(ctx->builder, LLVMRealOLT, lhs, rhs,"v_folt");
    set_val(ctx, res_id, cmp);
}

void handle_op_fordgreaterthan(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);

    LLVMValueRef cmp = LLVMBuildFCmp(ctx->builder, LLVMRealOGT, lhs, rhs, "v_fogt");

    set_val(ctx, res_id, cmp);
}

void handle_op_fmod(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef y = get_val(ctx, operands[1]);

    LLVMValueRef div = LLVMBuildFDiv(ctx->builder, x, y, "div");

    LLVMValueRef trunc =
        LLVMBuildFPToSI(ctx->builder,
                        div,
                        ctx->vec_float_type,
                        "trunc"); 

    LLVMValueRef mul =  LLVMBuildFMul(ctx->builder, y, trunc, "mul");
    LLVMValueRef result = LLVMBuildFSub(ctx->builder, x, mul, "fmod");

    set_val(ctx, res_id, result);
}

void handle_op_composite_construct(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands)
{
    SpvTypeInfo* info = &ctx->type_info[type_id];

    if (info->opcode == SpvOpTypeVector)
    {
        uint32_t num_components = info->member_count;
        
        LLVMTypeRef comp_elem_type = map_spv_to_llvm_type(ctx, info->base_type_id);
        LLVMTypeRef array_type = LLVMArrayType(comp_elem_type, num_components);
        LLVMValueRef composite = LLVMGetUndef(array_type);

        // Build the array by inserting each component vector
        for (uint32_t i = 0; i < num_components; i++)
        {
            LLVMValueRef component_vec = get_val(ctx, operands[i]);
            if (!component_vec)
                continue;
            
            // If operand is a scalar, broadcast it to SIMT_WIDTH
            LLVMTypeRef comp_type = LLVMTypeOf(component_vec);
            if (LLVMGetTypeKind(comp_type) != LLVMVectorTypeKind)
            {
                LLVMValueRef broadcast_vals[SIMT_WIDTH];
                for (int j = 0; j < SIMT_WIDTH; j++)
                {
                    broadcast_vals[j] = component_vec;
                }
                component_vec = LLVMConstVector(broadcast_vals, SIMT_WIDTH);
            }
            
            composite = LLVMBuildInsertValue(ctx->builder, composite, component_vec, i, "pack_comp");
        }
        set_val(ctx, res_id, composite);
        return;
    }

    if (info->opcode == SpvOpTypeMatrix)
    {
        uint32_t num_cols = info->member_count;
        uint32_t vec_type = info->base_type_id;

        SpvTypeInfo* vec_info = &ctx->type_info[vec_type];
        uint32_t num_rows = vec_info->member_count;

        LLVMTypeRef lane_vec_type = LLVMVectorType(ctx->float_type, SIMT_WIDTH);
        LLVMTypeRef column_type = LLVMArrayType(lane_vec_type, num_rows);
        LLVMTypeRef matrix_type = LLVMArrayType(column_type, num_cols);

        LLVMValueRef matrix = LLVMGetUndef(matrix_type);

        // Each operand should be a column vector [num_rows x <SIMT_WIDTH x float>]
        for (uint32_t c = 0; c < num_cols; c++)
        {
            LLVMValueRef column = get_val(ctx, operands[c]);
            
            // If operand is a scalar vector, ensure it's properly formatted
            LLVMTypeRef col_type = LLVMTypeOf(column);
            if (LLVMGetTypeKind(col_type) == LLVMVectorTypeKind)
            {
                // Convert vector of scalars to array of SIMT vectors
                LLVMValueRef column_array = LLVMGetUndef(column_type);
                for (uint32_t r = 0; r < num_rows; r++)
                {
                    LLVMValueRef scalar_elem = LLVMBuildExtractElement(ctx->builder, column, 
                                                                       LLVMConstInt(ctx->int_type, r, 0), "");
                    
                    LLVMValueRef broadcast_vals[SIMT_WIDTH];
                    for (int j = 0; j < SIMT_WIDTH; j++)
                    {
                        broadcast_vals[j] = scalar_elem;
                    }
                    LLVMValueRef simt_vec = LLVMConstVector(broadcast_vals, SIMT_WIDTH);
                    column_array = LLVMBuildInsertValue(ctx->builder, column_array, simt_vec, r, "");
                }
                column = column_array;
            }

            matrix = LLVMBuildInsertValue(ctx->builder, matrix, column, c, "insert_col");
        }

        set_val(ctx, res_id, matrix);
        return;
    }
}

void handle_op_vector_shuffle(JitContext* ctx, uint32_t res_id, uint32_t type_id,uint32_t* operands)
{
    SpvTypeInfo* info = &ctx->type_info[type_id];
    uint32_t num_components = info->member_count;

    // Word 0 is Vector 1, Word 1 is Vector 2, Words 2..N are the component indices
    uint32_t vec1_id = operands[0];
    uint32_t vec2_id = operands[1];

    LLVMValueRef vec1 = get_val(ctx, vec1_id);
    LLVMValueRef vec2 = get_val(ctx, vec2_id);

    // In your JIT, SPIR-V vectors are mapped to LLVM Arrays of SIMT vectors.
    // We need to know how many components are in Vector 1 to determine the index offset.
    LLVMTypeRef vec1_type = LLVMTypeOf(vec1);
    uint32_t vec1_components = LLVMGetArrayLength(vec1_type);

    LLVMTypeRef lane_vec_type = LLVMVectorType(ctx->float_type, SIMT_WIDTH);
    LLVMTypeRef array_type = LLVMArrayType(lane_vec_type, num_components);
    LLVMValueRef result = LLVMGetUndef(array_type);

    // Iterate through the requested indices and build the new logical vector
    for (uint32_t i = 0; i < num_components; i++)
    {
        uint32_t comp_idx = operands[2 + i];
        LLVMValueRef comp_val;

        if (comp_idx == 0xFFFFFFFF)
        {
            // FFFFFFFF means the corresponding result component has no source and is undefined
            comp_val = LLVMGetUndef(lane_vec_type);
        }
        else if (comp_idx < vec1_components)
        {
            // Extract from Vector 1
            comp_val = LLVMBuildExtractValue(ctx->builder, vec1, comp_idx, "shuf_ext_v1");
        }
        else
        {
            // Extract from Vector 2 (offset by the length of Vector 1)
            comp_val = LLVMBuildExtractValue(ctx->builder, vec2, comp_idx - vec1_components, "shuf_ext_v2");
        }

        result = LLVMBuildInsertValue(ctx->builder, result, comp_val, i, "shuf_ins");
    }

    set_val(ctx, res_id, result);
}
void handle_op_composite_extract(JitContext* ctx, uint32_t res_id, uint32_t* operands, uint32_t num_indices)
{
    // operands[0] = Composite ID
    // operands[1..] = literal indices
    
    LLVMValueRef composite = get_val(ctx, operands[0]);

    LLVMValueRef result = NULL;
    for (uint32_t i = 0; i < num_indices; i++)
    {
        uint32_t index = operands[i + 1]; 
        result = LLVMBuildExtractValue(
            ctx->builder, 
            composite, 
            index, 
            "extract_comp"
        );
        composite = result; 
    }
    DEBUG_PRINT("Extracting composite with %u indices\n", num_indices);

    set_val(ctx, res_id, result);
}
void handle_op_vector_times_scalar(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef vec = get_val(ctx, operands[0]);
    LLVMValueRef scalar = get_val(ctx, operands[1]);


    LLVMTypeRef vec_type = LLVMTypeOf(vec);
    uint32_t num_elements = LLVMGetArrayLength(vec_type);

    LLVMTypeRef lane_vec_type = LLVMVectorType(ctx->float_type, SIMT_WIDTH);
    LLVMTypeRef array_type = LLVMArrayType(lane_vec_type, num_elements);

    LLVMValueRef res = LLVMGetUndef(array_type);

    for (uint32_t i = 0; i < num_elements; i++) 
    {
        LLVMValueRef elem = LLVMBuildExtractValue(ctx->builder, vec, i, "tmp_elem");
        LLVMValueRef mul = LLVMBuildFMul(ctx->builder, elem, scalar, "mul_scalar");
        
        res = LLVMBuildInsertValue(ctx->builder, res, mul, i, "res_vec");
    }

    set_val(ctx, res_id, res);
}
LLVMValueRef calculate_dot_product(JitContext* ctx,  LLVMValueRef vecA , LLVMValueRef vecB)
{
    LLVMTypeRef vec_type = LLVMTypeOf(vecA);
    uint32_t num_elements = LLVMGetArrayLength(vec_type);

    LLVMValueRef dot_res = NULL;

    for (uint32_t i = 0; i < num_elements; i++) {
        LLVMValueRef a_elem = LLVMBuildExtractValue(ctx->builder, vecA, i, "a_elem");
        LLVMValueRef b_elem = LLVMBuildExtractValue(ctx->builder, vecB, i, "b_elem");
        
        LLVMValueRef mul = LLVMBuildFMul(ctx->builder, a_elem, b_elem, "mul_tmp");
        
        if (i == 0) 
        {
            dot_res = mul;
        } 
        else 
        {
            dot_res = LLVMBuildFAdd(ctx->builder, dot_res, mul, "dot_sum");
        }
    }
    return dot_res;
}
void handle_op_dot(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef vecA = get_val(ctx, operands[0]);
    LLVMValueRef vecB = get_val(ctx, operands[1]);
    LLVMValueRef dot_res = calculate_dot_product(ctx, vecA, vecB);
    set_val(ctx, res_id, dot_res);
}
void handle_op_matrix_times_vector(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef scalars[SIMT_WIDTH];
    LLVMTypeRef f32_type = ctx->float_type;

    LLVMValueRef matrix = get_val(ctx, operands[0]); // [NumCols x [NumRows x <16 x float>]]
    LLVMValueRef vector = get_val(ctx, operands[1]); // [NumCols x <16 x float>]

    LLVMTypeRef mat_type = LLVMTypeOf(matrix);
    uint32_t num_cols = LLVMGetArrayLength(mat_type);
    
    LLVMTypeRef col_type = LLVMGetElementType(mat_type);
    uint32_t num_rows = LLVMGetArrayLength(col_type);

    LLVMTypeRef res_vec_type = LLVMArrayType(ctx->vec_float_type, num_rows);
    LLVMValueRef res_val = LLVMGetUndef(res_vec_type);

    LLVMValueRef accumulators[4]; 
    CREATE_CONST_VEC(zero_vec, 0.0f);
    for (uint32_t r = 0; r < num_rows; r++) 
    {
        accumulators[r] = zero_vec;
    }

    for (uint32_t c = 0; c < num_cols; c++) 
    {
        LLVMValueRef column = LLVMBuildExtractValue(ctx->builder, matrix, c, "matrix_col");
        
        LLVMValueRef v_comp = LLVMBuildExtractValue(ctx->builder, vector, c, "vector_comp");

        for (uint32_t r = 0; r < num_rows; r++) 
        {
            LLVMValueRef mat_element = LLVMBuildExtractValue(ctx->builder, column, r, "mat_element");

            //Mat[c][r] * Vec[c] 
            LLVMValueRef mul = LLVMBuildFMul(ctx->builder, mat_element, v_comp, "mul_tmp");
            
            accumulators[r] = LLVMBuildFAdd(ctx->builder, accumulators[r], mul, "acc_tmp");
        }
    }

    for (uint32_t r = 0; r < num_rows; r++) 
    {
        res_val = LLVMBuildInsertValue(ctx->builder, res_val, accumulators[r], r, "final_vec");
    }

    set_val(ctx, res_id, res_val);
}


static LLVMValueRef alu_to_int_vec(JitContext* ctx, LLVMValueRef val)
{
    if (ctx->shader_type == TASK_SHADER || ctx->shader_type == MESH_SHADER)
        return jit_to_numeric_int_vector(ctx, val);
    return jit_to_int_vector(ctx, val);
}

static LLVMValueRef alu_from_int_vec(JitContext* ctx, LLVMValueRef val, bool is_unsigned)
{
    if (ctx->shader_type == TASK_SHADER || ctx->shader_type == MESH_SHADER) 
    {
        return is_unsigned
            ? LLVMBuildUIToFP(ctx->builder, val, ctx->vec_float_type, "v_i2uf")
            : LLVMBuildSIToFP(ctx->builder, val, ctx->vec_float_type, "v_i2sf");
    }
    return jit_to_float_vector(ctx, val);
}

void handle_op_bitwise_and(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = alu_to_int_vec(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = alu_to_int_vec(ctx, get_val(ctx, operands[1]));
    LLVMValueRef res = LLVMBuildAnd(ctx->builder, lhs, rhs, "v_band");
    set_val(ctx, res_id, alu_from_int_vec(ctx, res, true));
}

void handle_op_bitwise_or(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = alu_to_int_vec(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = alu_to_int_vec(ctx, get_val(ctx, operands[1]));
    LLVMValueRef res = LLVMBuildOr(ctx->builder, lhs, rhs, "v_bor");
    set_val(ctx, res_id, alu_from_int_vec(ctx, res, true));
}

void handle_op_bitwise_xor(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = alu_to_int_vec(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = alu_to_int_vec(ctx, get_val(ctx, operands[1]));
    LLVMValueRef res = LLVMBuildXor(ctx->builder, lhs, rhs, "v_bxor");
    set_val(ctx, res_id, alu_from_int_vec(ctx, res, true));
}

void handle_op_not(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef val = alu_to_int_vec(ctx, get_val(ctx, operands[0]));
    LLVMValueRef res = LLVMBuildNot(ctx->builder, val, "v_not");
    set_val(ctx, res_id, alu_from_int_vec(ctx, res, false));
}

void handle_op_shift_left_logical(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = alu_to_int_vec(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = jit_to_numeric_int_vector(ctx, get_val(ctx, operands[1]));
    LLVMValueRef res = LLVMBuildShl(ctx->builder, lhs, rhs, "v_shl");
    set_val(ctx, res_id, alu_from_int_vec(ctx, res, true));
}

void handle_op_shift_right_logical(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = alu_to_int_vec(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = jit_to_numeric_int_vector(ctx, get_val(ctx, operands[1]));
    LLVMValueRef res = LLVMBuildLShr(ctx->builder, lhs, rhs, "v_lshr");
    set_val(ctx, res_id, alu_from_int_vec(ctx, res, true));
}

void handle_op_shift_right_arithmetic(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = alu_to_int_vec(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = jit_to_numeric_int_vector(ctx, get_val(ctx, operands[1]));
    LLVMValueRef res = LLVMBuildAShr(ctx->builder, lhs, rhs, "v_ashr");
    set_val(ctx, res_id, alu_from_int_vec(ctx, res, false));
}

void handle_op_bitcast(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands)
{
    (void)type_id;
    LLVMValueRef op = get_val(ctx, operands[0]);
    set_val(ctx, res_id, op);
}

void handle_op_convert_f_to_s(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef op = get_val(ctx, operands[0]);
    LLVMValueRef res = LLVMBuildFPToSI(ctx->builder, op, LLVMVectorType(ctx->int_type, SIMT_WIDTH), "v_f2s");
    set_val(ctx, res_id, alu_from_int_vec(ctx, res, false));
}

void handle_op_convert_f_to_u(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef op = get_val(ctx, operands[0]);
    LLVMValueRef res = LLVMBuildFPToUI(ctx->builder, op, LLVMVectorType(ctx->int_type, SIMT_WIDTH), "v_f2u");
    set_val(ctx, res_id, alu_from_int_vec(ctx, res, true));
}

void handle_op_convert_u_to_f(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    if (ctx->shader_type == TASK_SHADER || ctx->shader_type == MESH_SHADER) 
    {
        set_val(ctx, res_id, get_val(ctx, operands[0]));
        return;
    }
    LLVMValueRef op = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMValueRef res = LLVMBuildUIToFP(ctx->builder, op, ctx->vec_float_type, "v_u2f");
    set_val(ctx, res_id, res);
}

void handle_op_inot_equal(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = alu_to_int_vec(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = alu_to_int_vec(ctx, get_val(ctx, operands[1]));
    LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntNE, lhs, rhs, "v_ine");
    set_val(ctx, res_id, cmp);
}

void handle_op_sgreater_than(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = alu_to_int_vec(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = alu_to_int_vec(ctx, get_val(ctx, operands[1]));
    LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSGT, lhs, rhs, "v_sgt");
    set_val(ctx, res_id, cmp);
}

void handle_op_ugreater_than(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = alu_to_int_vec(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = alu_to_int_vec(ctx, get_val(ctx, operands[1]));
    LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntUGT, lhs, rhs, "v_ugt");
    set_val(ctx, res_id, cmp);
}

void handle_op_sgreater_than_equal(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = alu_to_int_vec(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = alu_to_int_vec(ctx, get_val(ctx, operands[1]));
    LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSGE, lhs, rhs, "v_sge");
    set_val(ctx, res_id, cmp);
}

void handle_op_ugreater_than_equal(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = alu_to_int_vec(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = alu_to_int_vec(ctx, get_val(ctx, operands[1]));
    LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntUGE, lhs, rhs, "v_uge");
    set_val(ctx, res_id, cmp);
}

void handle_op_sless_than_equal(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = alu_to_int_vec(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = alu_to_int_vec(ctx, get_val(ctx, operands[1]));
    LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSLE, lhs, rhs, "v_sle");
    set_val(ctx, res_id, cmp);
}

void handle_op_uless_than_equal(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = alu_to_int_vec(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = alu_to_int_vec(ctx, get_val(ctx, operands[1]));
    LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntULE, lhs, rhs, "v_ule");
    set_val(ctx, res_id, cmp);
}

void handle_op_snegate(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef op = alu_to_int_vec(ctx, get_val(ctx, operands[0]));
    LLVMValueRef res = LLVMBuildNeg(ctx->builder, op, "v_sneg");
    set_val(ctx, res_id, alu_from_int_vec(ctx, res, false));
}

void handle_op_umod(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = jit_to_numeric_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = jit_to_numeric_int_vector(ctx, get_val(ctx, operands[1]));
    LLVMValueRef res = LLVMBuildURem(ctx->builder, lhs, rhs, "v_umod");
    set_val(ctx, res_id, LLVMBuildUIToFP(ctx->builder, res, ctx->vec_float_type, "v_umod_f"));
}

void handle_op_srem(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = alu_to_int_vec(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = alu_to_int_vec(ctx, get_val(ctx, operands[1]));
    LLVMValueRef res = LLVMBuildSRem(ctx->builder, lhs, rhs, "v_srem");
    set_val(ctx, res_id, alu_from_int_vec(ctx, res, false));
}

void handle_op_smod(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = alu_to_int_vec(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = alu_to_int_vec(ctx, get_val(ctx, operands[1]));
    LLVMValueRef srem = LLVMBuildSRem(ctx->builder, lhs, rhs, "v_smod_rem");
    LLVMValueRef srem_plus_rhs = LLVMBuildAdd(ctx->builder, srem, rhs, "v_smod_add");
    LLVMValueRef res = LLVMBuildSRem(ctx->builder, srem_plus_rhs, rhs, "v_smod");
    set_val(ctx, res_id, alu_from_int_vec(ctx, res, false));
}

void handle_op_logical_and(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildAnd(ctx->builder, lhs, rhs, "v_land");
    set_val(ctx, res_id, res);
}

void handle_op_logical_or(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildOr(ctx->builder, lhs, rhs, "v_lor");
    set_val(ctx, res_id, res);
}

void handle_op_logical_not(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef op = get_val(ctx, operands[0]);
    LLVMValueRef res = LLVMBuildNot(ctx->builder, op, "v_lnot");
    set_val(ctx, res_id, res);
}

void handle_op_logical_equal(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildICmp(ctx->builder, LLVMIntEQ, lhs, rhs, "v_leq");
    set_val(ctx, res_id, res);
}

void handle_op_logical_not_equal(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildICmp(ctx->builder, LLVMIntNE, lhs, rhs, "v_lne");
    set_val(ctx, res_id, res);
}

void handle_op_any(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef vec = get_val(ctx, operands[0]);
    LLVMValueRef any_val = LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0);
    for (int i = 0; i < SIMT_WIDTH; i++) {
        LLVMValueRef elem = LLVMBuildExtractElement(ctx->builder, vec, LLVMConstInt(ctx->int_type, i, 0), "elem");
        any_val = LLVMBuildOr(ctx->builder, any_val, elem, "any_acc");
    }
    LLVMValueRef res_vec = LLVMGetUndef(ctx->vec_i1_type);
    for (int i = 0; i < SIMT_WIDTH; i++) {
        res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, any_val, LLVMConstInt(ctx->int_type, i, 0), "any_ins");
    }
    set_val(ctx, res_id, res_vec);
}

void handle_op_all(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef vec = get_val(ctx, operands[0]);
    LLVMValueRef all_val = LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0);
    for (int i = 0; i < SIMT_WIDTH; i++) {
        LLVMValueRef elem = LLVMBuildExtractElement(ctx->builder, vec, LLVMConstInt(ctx->int_type, i, 0), "elem");
        all_val = LLVMBuildAnd(ctx->builder, all_val, elem, "all_acc");
    }
    LLVMValueRef res_vec = LLVMGetUndef(ctx->vec_i1_type);
    for (int i = 0; i < SIMT_WIDTH; i++) {
        res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, all_val, LLVMConstInt(ctx->int_type, i, 0), "all_ins");
    }
    set_val(ctx, res_id, res_vec);
}

void handle_op_is_nan(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef op = get_val(ctx, operands[0]);
    LLVMValueRef res = LLVMBuildFCmp(ctx->builder, LLVMRealUNO, op, op, "v_isnan");
    set_val(ctx, res_id, res);
}

void handle_op_is_inf(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef op = get_val(ctx, operands[0]);
    unsigned fabs_id = LLVMLookupIntrinsicID("llvm.fabs", 9);
    LLVMValueRef fabs_func = LLVMGetIntrinsicDeclaration(ctx->module, fabs_id, &ctx->vec_float_type, 1);
    LLVMValueRef abs_op = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(fabs_func), fabs_func, &op, 1, "abs_op");
    
    LLVMValueRef inf_scalars[SIMT_WIDTH];
    for (int i = 0; i < SIMT_WIDTH; i++) inf_scalars[i] = LLVMConstReal(ctx->float_type, INFINITY);
    LLVMValueRef inf_vec = LLVMConstVector(inf_scalars, SIMT_WIDTH);

    LLVMValueRef cmp = LLVMBuildFCmp(ctx->builder, LLVMRealOEQ, abs_op, inf_vec, "v_isinf");
    set_val(ctx, res_id, cmp);
}
