#include "jit_alu.h"
#include "glsl_std_450.h"
#include <llvm-c/Core.h>
#include <math.h>
#include "debug_gpu.h"

static LLVMValueRef jit_to_numeric_int_vector(JitContext* ctx, LLVMValueRef val);

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
    uint8_t kind = ctx->type_kind_map[type_id];
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
        float val = (float)(*(int32_t*)&operands[0]);
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
    LLVMValueRef cond = get_val(ctx, operands[0]); // Vector of i1
    LLVMValueRef true_val = get_val(ctx, operands[1]);
    LLVMValueRef false_val = get_val(ctx, operands[2]);
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
void handle_op_ext_instr(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    uint32_t instr_id = operands[1];
    ctx->glsl_handlers[instr_id](ctx, res_id, &operands[2]);
}

void handle_op_composite_construct(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands)
{
    SpvTypeInfo* info = &ctx->type_info[type_id];

    if (info->opcode == SpvOpTypeVector)
    {
        uint32_t num_components = info->member_count;
        
        LLVMTypeRef lane_vec_type = LLVMVectorType(ctx->float_type, SIMT_WIDTH);
        LLVMTypeRef array_type = LLVMArrayType(lane_vec_type, num_components);
        LLVMValueRef composite = LLVMGetUndef(array_type);

        // Each operand should be a <SIMT_WIDTH x float> vector
        // Build the array by inserting each component vector
        for (uint32_t i = 0; i < num_components; i++)
        {
            LLVMValueRef component_vec = get_val(ctx, operands[i]);
            
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

void create_glsl_std_450_map(JitContext* ctx)
{
    ctx->glsl_handlers[GLSLstd450Sin] = handle_ext_sin;
    ctx->glsl_handlers[GLSLstd450Cos] = handle_ext_cos;
    ctx->glsl_handlers[GLSLstd450Sqrt] = handle_ext_sqrt;
    ctx->glsl_handlers[GLSLstd450Pow] = handle_ext_pow;
    ctx->glsl_handlers[GLSLstd450Atan2] = handle_ext_atan2;
    ctx->glsl_handlers[GLSLstd450Log] = handle_ext_log;
    ctx->glsl_handlers[GLSLstd450FAbs] = handle_ext_fabs;
    ctx->glsl_handlers[GLSLstd450FMax] = handle_ext_fmax;
    ctx->glsl_handlers[GLSLstd450FMin] = handle_ext_fmin;
    ctx->glsl_handlers[GLSLstd450FClamp] = handle_ext_fclamp;
    ctx->glsl_handlers[GLSLstd450SmoothStep] = handle_ext_smoothstep;
    ctx->glsl_handlers[GLSLstd450FMix] = handle_ext_fmix;
    ctx->glsl_handlers[GLSLstd450FSign] = handle_ext_fsign;
    ctx->glsl_handlers[GLSLstd450Step] = handle_ext_step;
    ctx->glsl_handlers[GLSLstd450Length] = handle_ext_length;
    ctx->glsl_handlers[GLSLstd450Normalize] = handle_ext_normalize;
    ctx->glsl_handlers[GLSLstd450Reflect] = handle_ext_reflect;
    ctx->glsl_handlers[GLSLstd450Distance] = handle_ext_distance;
    ctx->glsl_handlers[GLSLstd450Cross] = handle_ext_cross;
    ctx->glsl_handlers[GLSLstd450Refract] = handle_ext_refract;
    ctx->glsl_handlers[GLSLstd450Tan] = handle_ext_tan;
    ctx->glsl_handlers[GLSLstd450Exp] = handle_ext_exp;

}
void handle_ext_exp(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMTypeRef vec_float_type = ctx->vec_float_type;

    unsigned intrinsic_id = LLVMLookupIntrinsicID("llvm.exp", 8); 
    LLVMValueRef exp_func = LLVMGetIntrinsicDeclaration(ctx->module, intrinsic_id, &vec_float_type, 1);

    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef args[] = { x };

    LLVMTypeRef func_sig = LLVMGlobalGetValueType(exp_func);
    LLVMValueRef result = LLVMBuildCall2(ctx->builder, 
                                        func_sig,
                                        exp_func, 
                                        args, 1, 
                                        "exp_v");

    set_val(ctx, res_id, result);
}
void handle_ext_tan(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMTypeRef vec_float_type = ctx->vec_float_type;

    unsigned intrinsic_id = LLVMLookupIntrinsicID("llvm.tan", 8); 
    LLVMValueRef sin_func = LLVMGetIntrinsicDeclaration(ctx->module, intrinsic_id, &vec_float_type, 1);
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef args[] = { x };
    LLVMTypeRef func_sig = LLVMGlobalGetValueType(sin_func);
    LLVMValueRef result = LLVMBuildCall2(ctx->builder, 
                                    func_sig,
                                    sin_func, 
                                    args, 1, 
                                    "tan_v");
    set_val(ctx, res_id, result);
}
void handle_ext_sin(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMTypeRef vec_float_type = ctx->vec_float_type;

    unsigned intrinsic_id = LLVMLookupIntrinsicID("llvm.sin", 8); 
    LLVMValueRef sin_func = LLVMGetIntrinsicDeclaration(ctx->module, intrinsic_id, &vec_float_type, 1);
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef args[] = { x };
    LLVMTypeRef func_sig = LLVMGlobalGetValueType(sin_func);
    LLVMValueRef result = LLVMBuildCall2(ctx->builder, 
                                    func_sig,
                                    sin_func, 
                                    args, 1, 
                                    "sin_v");
    set_val(ctx, res_id, result);
}     
void handle_ext_cos(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMTypeRef vec_float_type = ctx->vec_float_type;

    unsigned intrinsic_id = LLVMLookupIntrinsicID("llvm.cos", 8); 
    LLVMValueRef cos_func = LLVMGetIntrinsicDeclaration(ctx->module, intrinsic_id, &vec_float_type, 1);
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef args[] = { x };
    LLVMTypeRef func_sig = LLVMGlobalGetValueType(cos_func);
    LLVMValueRef result = LLVMBuildCall2(ctx->builder, 
                                    func_sig,
                                    cos_func, 
                                    args, 1, 
                                    "cos_v");
    set_val(ctx, res_id, result);
}      
void handle_ext_sqrt(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMTypeRef vec_float_type = ctx->vec_float_type;

    unsigned intrinsic_id = LLVMLookupIntrinsicID("llvm.sqrt", 9); 
    LLVMValueRef sqrt_func = LLVMGetIntrinsicDeclaration(ctx->module, intrinsic_id, &vec_float_type, 1);
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef args[] = { x };
    LLVMTypeRef func_sig = LLVMGlobalGetValueType(sqrt_func);
    LLVMValueRef result = LLVMBuildCall2(ctx->builder, 
                                    func_sig,
                                    sqrt_func, 
                                    args, 1, 
                                    "sqrt_v");
    set_val(ctx, res_id, result);
}     
void handle_ext_pow(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMTypeRef vec_float_type = ctx->vec_float_type;
    unsigned intrinsic_id = LLVMLookupIntrinsicID("llvm.pow", 8); 
    LLVMValueRef pow_func = LLVMGetIntrinsicDeclaration(ctx->module, intrinsic_id, &vec_float_type, 2);
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef y = get_val(ctx, operands[1]);

    LLVMValueRef args[] = { x,y };
    LLVMTypeRef func_sig = LLVMGlobalGetValueType(pow_func);
    LLVMValueRef result = LLVMBuildCall2(ctx->builder, 
                                    func_sig,
                                    pow_func, 
                                    args, 2, 
                                    "pow_v");
    set_val(ctx, res_id, result);

}     
void handle_ext_atan2(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMTypeRef vec_float_type = ctx->vec_float_type;
    unsigned intrinsic_id = LLVMLookupIntrinsicID("llvm.atan2", 10); 
    LLVMValueRef atan2_func = LLVMGetIntrinsicDeclaration(ctx->module, intrinsic_id, &vec_float_type, 1);
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef y = get_val(ctx, operands[1]);

    LLVMValueRef args[] = { x, y };
    LLVMTypeRef func_sig = LLVMGlobalGetValueType(atan2_func);
    LLVMValueRef result = LLVMBuildCall2(ctx->builder, 
                                    func_sig,
                                    atan2_func, 
                                    args, 2, 
                                    "atan2_v");
    set_val(ctx, res_id, result);

}    
void handle_ext_log(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMTypeRef vec_float_type = ctx->vec_float_type;
    unsigned intrinsic_id = LLVMLookupIntrinsicID("llvm.log", 8); 
    LLVMValueRef log_func = LLVMGetIntrinsicDeclaration(ctx->module, intrinsic_id, &vec_float_type, 1);
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef args[] = { x };
    LLVMTypeRef func_sig = LLVMGlobalGetValueType(log_func);
    LLVMValueRef result = LLVMBuildCall2(ctx->builder, 
                                    func_sig,
                                    log_func, 
                                    args, 1, 
                                    "log_v");
    set_val(ctx, res_id, result);

}
void handle_ext_fabs(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMTypeRef vec_float_type = ctx->vec_float_type;
    unsigned intrinsic_id = LLVMLookupIntrinsicID("llvm.fabs", 9); 
    LLVMValueRef fabs_func = LLVMGetIntrinsicDeclaration(ctx->module, intrinsic_id, &vec_float_type, 1);
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef args[] = { x };
    LLVMTypeRef func_sig = LLVMGlobalGetValueType(fabs_func);
    LLVMValueRef result = LLVMBuildCall2(ctx->builder, 
                                    func_sig,
                                    fabs_func, 
                                    args, 1, 
                                    "fabs_v");
    set_val(ctx, res_id, result);
}
void handle_ext_fmax(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMTypeRef vec_float_type = ctx->vec_float_type;
    unsigned intrinsic_id = LLVMLookupIntrinsicID("llvm.maxnum", 11); 
    LLVMValueRef max_func = LLVMGetIntrinsicDeclaration(ctx->module, intrinsic_id, &vec_float_type, 2);
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef y = get_val(ctx, operands[1]);

    LLVMValueRef args[] = { x, y };
    LLVMTypeRef func_sig = LLVMGlobalGetValueType(max_func);
    LLVMValueRef result = LLVMBuildCall2(ctx->builder, 
                                    func_sig,
                                    max_func, 
                                    args, 2, 
                                    "max_v");
    set_val(ctx, res_id, result);

}
void handle_ext_fmin(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMTypeRef vec_float_type = ctx->vec_float_type;
    unsigned intrinsic_id = LLVMLookupIntrinsicID("llvm.minnum", 11); 
    LLVMValueRef min_func = LLVMGetIntrinsicDeclaration(ctx->module, intrinsic_id, &vec_float_type, 2);
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef y = get_val(ctx, operands[1]);

    LLVMValueRef args[] = { x, y };
    LLVMTypeRef func_sig = LLVMGlobalGetValueType(min_func);
    LLVMValueRef result = LLVMBuildCall2(ctx->builder, 
                                    func_sig,
                                    min_func, 
                                    args, 2, 
                                    "min_v");
    set_val(ctx, res_id, result);
}
void handle_ext_fclamp(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMTypeRef vec_float_type = ctx->vec_float_type;
    
    unsigned max_id = LLVMLookupIntrinsicID("llvm.maxnum", 11);
    unsigned min_id = LLVMLookupIntrinsicID("llvm.minnum", 11);

    LLVMValueRef max_func = LLVMGetIntrinsicDeclaration(ctx->module, max_id, &vec_float_type, 1);
    LLVMValueRef min_func = LLVMGetIntrinsicDeclaration(ctx->module, min_id, &vec_float_type, 1);

    LLVMValueRef val = get_val(ctx, operands[0]);
    LLVMValueRef min = get_val(ctx, operands[1]);
    LLVMValueRef max = get_val(ctx, operands[2]);

    LLVMValueRef args_max[] = { val, min };
    LLVMValueRef clamped_min = LLVMBuildCall2(ctx->builder, 
                                            LLVMGlobalGetValueType(max_func), 
                                            max_func, 
                                            args_max, 2, 
                                            "clamp_max_tmp");

    LLVMValueRef args_min[] = { clamped_min, max };
    LLVMValueRef final_result = LLVMBuildCall2(ctx->builder, 
                                            LLVMGlobalGetValueType(min_func), 
                                            min_func, 
                                            args_min, 2, 
                                            "fclamp_res");

    set_val(ctx, res_id, final_result);
}
void handle_ext_smoothstep(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef scalars[SIMT_WIDTH];
   
    LLVMTypeRef f32_type = ctx->float_type;
    LLVMTypeRef vec_float_type = ctx->vec_float_type;

    LLVMValueRef edge0 = get_val(ctx, operands[0]);
    LLVMValueRef edge1 = get_val(ctx, operands[1]);
    LLVMValueRef x     = get_val(ctx, operands[2]);

    // t = (x - edge0) / (edge1 - edge0)
    LLVMValueRef num   = LLVMBuildFSub(ctx->builder, x, edge0, "sub_x_e0");
    LLVMValueRef den   = LLVMBuildFSub(ctx->builder, edge1, edge0, "sub_e1_e0");
    LLVMValueRef t_raw = LLVMBuildFDiv(ctx->builder, num, den, "t_raw");

    // Clamp t between 0.0 and 1.0
    CREATE_CONST_VEC(zero_vec, (float)0.0f);
    CREATE_CONST_VEC(one_vec, (float)1.0f);


    unsigned max_id = LLVMLookupIntrinsicID("llvm.maxnum", 11);
    unsigned min_id = LLVMLookupIntrinsicID("llvm.minnum", 11);
    LLVMValueRef max_f = LLVMGetIntrinsicDeclaration(ctx->module, max_id, &vec_float_type, 1);
    LLVMValueRef min_f = LLVMGetIntrinsicDeclaration(ctx->module, min_id, &vec_float_type, 1);

    LLVMValueRef args_max[] = { t_raw, zero_vec };
    LLVMValueRef t_clamped_min = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(max_f), max_f, args_max, 2, "t_max");

    LLVMValueRef args_min[] = { t_clamped_min, one_vec };
    LLVMValueRef t = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(min_f), min_f, args_min, 2, "t_clamped");

    //result = t * t * (3.0 - 2.0 * t)
    CREATE_CONST_VEC(three_v, (float)3.0f);
    CREATE_CONST_VEC(two_v, (float)2.0f);

    // (2.0 * t)
    LLVMValueRef two_t   = LLVMBuildFMul(ctx->builder, two_v, t, "two_t");
    // (3.0 - 2.0 * t)
    LLVMValueRef term3   = LLVMBuildFSub(ctx->builder, three_v, two_t, "term3");
    // t * t
    LLVMValueRef t_sq    = LLVMBuildFMul(ctx->builder, t, t, "t_sq");
    // t_sq * term3
    LLVMValueRef final_res = LLVMBuildFMul(ctx->builder, t_sq, term3, "smoothstep_v");

    set_val(ctx, res_id, final_res);
}
void handle_ext_fmix(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    //x * (1 - a) + y * a.
    LLVMValueRef scalars[SIMT_WIDTH];
   
    LLVMTypeRef f32_type = ctx->float_type;

    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef y = get_val(ctx, operands[1]);
    LLVMValueRef a = get_val(ctx, operands[2]);

    CREATE_CONST_VEC(one_vec, (float)1.0f);
    LLVMValueRef one_minus_a = LLVMBuildFSub(ctx->builder, one_vec, a, "one_minus_a");
    LLVMValueRef x_times_one_minus_a = LLVMBuildFMul(ctx->builder, x, one_minus_a, "x_times_one_minus_a");

    LLVMValueRef y_times_a = LLVMBuildFMul(ctx->builder, y, a, "y_times_a");
    LLVMValueRef final_result = LLVMBuildFAdd(ctx->builder, x_times_one_minus_a, y_times_a, "fmix_v");

    set_val(ctx, res_id, final_result);
}
void handle_ext_fsign(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef scalars[SIMT_WIDTH];
    LLVMTypeRef f32_type = ctx->float_type;
    LLVMTypeRef vec_float_type = ctx->vec_float_type;

    LLVMValueRef x = get_val(ctx, operands[0]);
  
    CREATE_CONST_VEC(one_v, (float)1.0f);
    CREATE_CONST_VEC(zero_v, (float)0.0f);

    unsigned copy_id = LLVMLookupIntrinsicID("llvm.copysign", 13);
    LLVMValueRef copy_f = LLVMGetIntrinsicDeclaration(ctx->module, copy_id, &vec_float_type, 1);
    
    LLVMValueRef args[] = { one_v, x };
    LLVMValueRef sign_bits = LLVMBuildCall2(ctx->builder, 
                                           LLVMGlobalGetValueType(copy_f), 
                                           copy_f, 
                                           args, 2, 
                                           "sign_bits");

    LLVMValueRef is_zero = LLVMBuildFCmp(ctx->builder, LLVMRealOEQ, x, zero_v, "is_zero_mask");

   
    LLVMValueRef final_res = LLVMBuildSelect(ctx->builder, 
                                             is_zero, 
                                             zero_v, 
                                             sign_bits, 
                                             "fsign_res");

    set_val(ctx, res_id, final_res);
}


void handle_ext_step(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    //Result is 0.0 if x < edge; otherwise result is 1.0.
    LLVMValueRef scalars[SIMT_WIDTH];
    LLVMTypeRef f32_type = ctx->float_type;
    LLVMValueRef edge = get_val(ctx, operands[0]);
    LLVMValueRef x = get_val(ctx, operands[1]);

    CREATE_CONST_VEC(one_v, (float)1.0f);
    LLVMValueRef cmp = LLVMBuildFCmp(ctx->builder, LLVMRealOLT, x, edge, "step_cmp");

    LLVMValueRef result = LLVMBuildSelect(ctx->builder, cmp, one_v, LLVMConstNull(ctx->vec_float_type), "step_res");
    set_val(ctx, res_id, result);

}
void handle_ext_length(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef vec = get_val(ctx, operands[0]);
    LLVMTypeRef vec_type = LLVMTypeOf(vec);
    
    uint32_t num_elements = LLVMGetVectorSize(vec_type);
    LLVMValueRef result = NULL;

    for (uint32_t i = 0; i < num_elements; i++)
    {
        LLVMValueRef elem = LLVMBuildExtractValue(ctx->builder, vec, i, "elem");
        LLVMValueRef elem_sq = LLVMBuildFMul(ctx->builder, elem, elem, "elem_sq");
        
        if (i == 0) 
        {
            result = elem_sq;
        } 
        else 
        {
            result = LLVMBuildFAdd(ctx->builder, result, elem_sq, "sum_tmp");
        }
    }

   
    LLVMTypeRef scalar_type = LLVMTypeOf(result);
    unsigned intrinsic_id = LLVMLookupIntrinsicID("llvm.sqrt", 9); 
    LLVMValueRef sqrt_func = LLVMGetIntrinsicDeclaration(ctx->module, intrinsic_id, &scalar_type, 1);
    
    LLVMValueRef args[] = { result };
    LLVMTypeRef sqrt_type = LLVMGlobalGetValueType(sqrt_func);
    
    result = LLVMBuildCall2(ctx->builder, 
                            sqrt_type,
                            sqrt_func, 
                            args, 1, 
                            "sqrt_final");

    set_val(ctx, res_id, result);

}
void handle_ext_normalize(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef vec = get_val(ctx, operands[0]);
    LLVMTypeRef vec_type = LLVMTypeOf(vec);
    
    uint32_t num_elements = LLVMGetVectorSize(vec_type);
    LLVMValueRef result = NULL;

    for (uint32_t i = 0; i < num_elements; i++)
    {
        LLVMValueRef elem = LLVMBuildExtractValue(ctx->builder, vec, i, "elem");
        LLVMValueRef elem_sq = LLVMBuildFMul(ctx->builder, elem, elem, "elem_sq");
        
        if (i == 0) 
        {
            result = elem_sq;
        } 
        else 
        {
            result = LLVMBuildFAdd(ctx->builder, result, elem_sq, "sum_tmp");
        }
    }

   
    LLVMTypeRef scalar_type = LLVMTypeOf(result);
    unsigned intrinsic_id = LLVMLookupIntrinsicID("llvm.sqrt", 9); 
    LLVMValueRef sqrt_func = LLVMGetIntrinsicDeclaration(ctx->module, intrinsic_id, &scalar_type, 1);
    
    LLVMValueRef args[] = { result };
    LLVMTypeRef sqrt_type = LLVMGlobalGetValueType(sqrt_func);
    
    result = LLVMBuildCall2(ctx->builder, 
                            sqrt_type,
                            sqrt_func, 
                            args, 1, 
                            "sqrt_final");

    LLVMValueRef normalized_vec = LLVMGetUndef(vec_type);
    for (uint32_t i = 0; i < num_elements; i++)
    {
        LLVMValueRef elem = LLVMBuildExtractValue(ctx->builder, vec, i, "elem");
        LLVMValueRef norm_elem = LLVMBuildFDiv(ctx->builder, elem, result, "norm_elem");

        normalized_vec = LLVMBuildInsertValue(ctx->builder, normalized_vec, norm_elem, i, "normalized_vec");
    
    }
    set_val(ctx, res_id, normalized_vec);

}
void handle_ext_reflect(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef I = get_val(ctx, operands[0]);
    LLVMValueRef N = get_val(ctx, operands[1]);
    LLVMTypeRef vec_type = LLVMTypeOf(I);
    uint32_t num_elements = LLVMGetArrayLength(vec_type);

    // d = dot(N, I)
    LLVMValueRef dot_ni = calculate_dot_product(ctx, N, I);

    // factor = 2.0 * dot(N, I)
    LLVMValueRef scalars[SIMT_WIDTH];
    LLVMTypeRef f32_type = ctx->float_type;
    CREATE_CONST_VEC(two_vec, (float)2.0f);
    LLVMValueRef factor = LLVMBuildFMul(ctx->builder, two_vec, dot_ni, "factor");

    // R = I - (factor * N)
    LLVMTypeRef lane_vec_type = LLVMVectorType(ctx->float_type, SIMT_WIDTH);
    LLVMTypeRef array_type = LLVMArrayType(lane_vec_type, num_elements);
    LLVMValueRef result_vec = LLVMGetUndef(array_type);
    for (uint32_t i = 0; i < num_elements; i++) 
    {
        LLVMValueRef i_elem = LLVMBuildExtractValue(ctx->builder, I, i, "i_elem");
        LLVMValueRef n_elem = LLVMBuildExtractValue(ctx->builder, N, i, "n_elem");
        
        // (factor * N[i])
        LLVMValueRef scaled_n = LLVMBuildFMul(ctx->builder, factor, n_elem, "scaled_n");
        
        // I[i] - scaled_n
        LLVMValueRef reflected_elem = LLVMBuildFSub(ctx->builder, i_elem, scaled_n, "reflected_elem");
        
        result_vec = LLVMBuildInsertValue(ctx->builder, result_vec, reflected_elem, i, "res_vec");
    }

    set_val(ctx, res_id, result_vec);
}
void handle_ext_distance(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef vec1 = get_val(ctx, operands[0]);
    LLVMValueRef vec2 = get_val(ctx, operands[1]);

    //assume vec1 and vec2 are of the same type and are vectors of floats
    LLVMTypeRef vec_type = LLVMTypeOf(vec1);
    
    uint32_t num_elements = LLVMGetVectorSize(vec_type);
    LLVMValueRef result = NULL;

    for (uint32_t i = 0; i < num_elements; i++)
    {
        LLVMValueRef elem1 = LLVMBuildExtractValue(ctx->builder, vec1, i, "elem1");
        LLVMValueRef elem2 = LLVMBuildExtractValue(ctx->builder, vec2, i, "elem2");
        LLVMValueRef diff = LLVMBuildFSub(ctx->builder, elem1, elem2, "diff");
        LLVMValueRef elem_sq = LLVMBuildFMul(ctx->builder, diff, diff, "elem_sq");
        
        if (i == 0) 
        {
            result = elem_sq;
        } 
        else 
        {
            result = LLVMBuildFAdd(ctx->builder, result, elem_sq, "sum_tmp");
        }
    }

   
    LLVMTypeRef scalar_type = LLVMTypeOf(result);
    unsigned intrinsic_id = LLVMLookupIntrinsicID("llvm.sqrt", 9); 
    LLVMValueRef sqrt_func = LLVMGetIntrinsicDeclaration(ctx->module, intrinsic_id, &scalar_type, 1);
    
    LLVMValueRef args[] = { result };
    LLVMTypeRef sqrt_type = LLVMGlobalGetValueType(sqrt_func);
    
    result = LLVMBuildCall2(ctx->builder, 
                            sqrt_type,
                            sqrt_func, 
                            args, 1, 
                            "sqrt_final");

    set_val(ctx, res_id, result);
}
void handle_ext_cross(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef vec_x = get_val(ctx, operands[0]);
    LLVMValueRef vec_y = get_val(ctx, operands[1]);

    LLVMTypeRef vec_type = LLVMTypeOf(vec_x);
    
    uint32_t num_elements = LLVMGetVectorSize(vec_type);
    if(num_elements != 3) 
    {
        printf("Error: reflect function expects a vec3 input\n");
        return;
    }
    LLVMValueRef x0 = LLVMBuildExtractValue(ctx->builder, vec_x, 0, "x0");
    LLVMValueRef x1 = LLVMBuildExtractValue(ctx->builder, vec_x, 1, "x1");
    LLVMValueRef x2 = LLVMBuildExtractValue(ctx->builder, vec_x, 2, "x2");

    LLVMValueRef y0 = LLVMBuildExtractValue(ctx->builder, vec_y, 0, "y0");
    LLVMValueRef y1 = LLVMBuildExtractValue(ctx->builder, vec_y, 1, "y1");
    LLVMValueRef y2 = LLVMBuildExtractValue(ctx->builder, vec_y, 2, "y2");
 
    LLVMValueRef r0 = LLVMBuildFSub(ctx->builder, 
        LLVMBuildFMul(ctx->builder, x1, y2, "m1"), 
        LLVMBuildFMul(ctx->builder, y1, x2, "m2"), "r0");

    LLVMValueRef r1 = LLVMBuildFSub(ctx->builder, 
        LLVMBuildFMul(ctx->builder, x2, y0, "m3"), 
        LLVMBuildFMul(ctx->builder, y2, x0, "m4"), "r1");

    LLVMValueRef r2 = LLVMBuildFSub(ctx->builder, 
        LLVMBuildFMul(ctx->builder, x0, y1, "m5"), 
        LLVMBuildFMul(ctx->builder, y0, x1, "m6"), "r2");

    LLVMTypeRef lane_vec_type = LLVMVectorType(ctx->float_type, SIMT_WIDTH);
    LLVMTypeRef array_type = LLVMArrayType(lane_vec_type, 3);
    LLVMValueRef res = LLVMGetUndef(array_type);
    res = LLVMBuildInsertValue(ctx->builder, res, r0, 0, "res0");
    res = LLVMBuildInsertValue(ctx->builder, res, r1, 1, "res1");
    res = LLVMBuildInsertValue(ctx->builder, res, r2, 2, "res2");
    set_val(ctx, res_id, res);
}
void handle_ext_refract(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef scalars[SIMT_WIDTH];
    LLVMTypeRef f32_type = ctx->float_type;
    CREATE_CONST_VEC(one_vec, 1.0f);
    CREATE_CONST_VEC(zero_vec, 0.0f);

    LLVMValueRef eta = get_val(ctx, operands[2]);

    LLVMValueRef I = get_val(ctx, operands[0]);
    LLVMValueRef N = get_val(ctx, operands[1]);

    LLVMValueRef dotNI = calculate_dot_product(ctx, N, I);

    // k = 1.0 - eta * eta * (1.0 - dotNI * dotNI)
  
    LLVMValueRef eta2 = LLVMBuildFMul(ctx->builder, eta, eta, "eta2");
    LLVMValueRef dotNI2 = LLVMBuildFMul(ctx->builder, dotNI, dotNI, "dotNI2");
    LLVMValueRef oneMinusDot2 = LLVMBuildFSub(ctx->builder, one_vec, dotNI2, "tmp");
    LLVMValueRef k = LLVMBuildFSub(ctx->builder, one_vec, 
                                    LLVMBuildFMul(ctx->builder, eta2, oneMinusDot2, ""), "k");

    // k < 0.0 ?
    LLVMValueRef cond = LLVMBuildFCmp(ctx->builder, LLVMRealULT, k, zero_vec, "is_internal_reflection");

    //  sqrt(k)
    LLVMTypeRef vec_float_type = ctx->vec_float_type;
    unsigned intrinsic_id = LLVMLookupIntrinsicID("llvm.sqrt", 9); 
    LLVMValueRef sqrt_func = LLVMGetIntrinsicDeclaration(ctx->module, intrinsic_id, &vec_float_type, 1);
    LLVMValueRef args[] = { k };
    LLVMTypeRef func_sig = LLVMGlobalGetValueType(sqrt_func);
    LLVMValueRef sqrtK = LLVMBuildCall2(ctx->builder, 
                                    func_sig,
                                    sqrt_func, 
                                    args, 1, 
                                    "sqrt_v");

    // n_factor = (eta * dotNI + sqrt(k))
    LLVMValueRef n_factor = LLVMBuildFAdd(ctx->builder, 
                                        LLVMBuildFMul(ctx->builder, eta, dotNI, ""), 
                                        sqrtK, "n_factor");

    LLVMTypeRef lane_vec_type = LLVMVectorType(ctx->float_type, SIMT_WIDTH);
    LLVMTypeRef array_type = LLVMArrayType(lane_vec_type, 3);
    LLVMValueRef res = LLVMGetUndef(array_type);
    for (uint32_t i = 0; i < 3; i++) 
    {
        LLVMValueRef i_elem = LLVMBuildExtractValue(ctx->builder, I, i, "");
        LLVMValueRef n_elem = LLVMBuildExtractValue(ctx->builder, N, i, "");

        // Term: eta * I[i] - n_factor * N[i]
        LLVMValueRef p1 = LLVMBuildFMul(ctx->builder, eta, i_elem, "");
        LLVMValueRef p2 = LLVMBuildFMul(ctx->builder, n_factor, n_elem, "");
        LLVMValueRef refracted = LLVMBuildFSub(ctx->builder, p1, p2, "refr_tmp");

        // if k < 0, return 0.0 else return refracted
        LLVMValueRef final_val = LLVMBuildSelect(ctx->builder, cond, zero_vec, refracted, "select_res");
        
        res = LLVMBuildInsertValue(ctx->builder, res, final_val, i, "");
    }
    set_val(ctx, res_id, res);

}

static LLVMValueRef jit_to_int_vector(JitContext* ctx, LLVMValueRef val) {
    if (!val) return LLVMConstNull(LLVMVectorType(ctx->int_type, SIMT_WIDTH));
    LLVMTypeRef type = LLVMTypeOf(val);
    if (LLVMGetTypeKind(type) == LLVMVectorTypeKind) {
        LLVMTypeRef elem_type = LLVMGetElementType(type);
        if (LLVMGetTypeKind(elem_type) == LLVMFloatTypeKind || LLVMGetTypeKind(elem_type) == LLVMDoubleTypeKind) {
            return LLVMBuildBitCast(ctx->builder, val, LLVMVectorType(ctx->int_type, SIMT_WIDTH), "f2i_bc");
        }
        return val;
    }
    if (LLVMGetTypeKind(type) == LLVMFloatTypeKind || LLVMGetTypeKind(type) == LLVMDoubleTypeKind) {
        val = LLVMBuildBitCast(ctx->builder, val, ctx->int_type, "s_f2i_bc");
    }
    LLVMValueRef vec = LLVMGetUndef(LLVMVectorType(ctx->int_type, SIMT_WIDTH));
    for (int i = 0; i < SIMT_WIDTH; i++) {
        vec = LLVMBuildInsertElement(ctx->builder, vec, val, LLVMConstInt(ctx->int_type, i, 0), "splat_i");
    }
    return vec;
}

static LLVMValueRef jit_to_numeric_int_vector(JitContext* ctx, LLVMValueRef val) {
    if (!val) return LLVMConstNull(LLVMVectorType(ctx->int_type, SIMT_WIDTH));
    LLVMTypeRef type = LLVMTypeOf(val);
    LLVMTypeRef int_vec_type = LLVMVectorType(ctx->int_type, SIMT_WIDTH);

    if (LLVMGetTypeKind(type) == LLVMVectorTypeKind) {
        LLVMTypeRef elem_type = LLVMGetElementType(type);
        if (LLVMGetTypeKind(elem_type) == LLVMFloatTypeKind ||
            LLVMGetTypeKind(elem_type) == LLVMDoubleTypeKind) {
            return LLVMBuildFPToSI(ctx->builder, val, int_vec_type, "numeric_f2i");
        }
        return val;
    }

    if (LLVMGetTypeKind(type) == LLVMFloatTypeKind ||
        LLVMGetTypeKind(type) == LLVMDoubleTypeKind) {
        val = LLVMBuildFPToSI(ctx->builder, val, ctx->int_type, "numeric_s_f2i");
    }

    LLVMValueRef vec = LLVMGetUndef(int_vec_type);
    for (int i = 0; i < SIMT_WIDTH; i++) {
        vec = LLVMBuildInsertElement(ctx->builder, vec, val,
                                     LLVMConstInt(ctx->int_type, i, 0), "numeric_splat_i");
    }
    return vec;
}

static LLVMValueRef jit_to_float_vector(JitContext* ctx, LLVMValueRef val) {
    if (!val) return LLVMConstNull(ctx->vec_float_type);
    LLVMTypeRef type = LLVMTypeOf(val);
    if (LLVMGetTypeKind(type) == LLVMVectorTypeKind) {
        LLVMTypeRef elem_type = LLVMGetElementType(type);
        if (LLVMGetTypeKind(elem_type) == LLVMIntegerTypeKind) {
            return LLVMBuildBitCast(ctx->builder, val, ctx->vec_float_type, "i2f_bc");
        }
        return val;
    }
    if (LLVMGetTypeKind(type) == LLVMIntegerTypeKind) {
        val = LLVMBuildBitCast(ctx->builder, val, ctx->float_type, "s_i2f_bc");
    }
    LLVMValueRef vec = LLVMGetUndef(ctx->vec_float_type);
    for (int i = 0; i < SIMT_WIDTH; i++) {
        vec = LLVMBuildInsertElement(ctx->builder, vec, val, LLVMConstInt(ctx->int_type, i, 0), "splat_f");
    }
    return vec;
}

void handle_op_bitwise_and(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = jit_to_int_vector(ctx, get_val(ctx, operands[1]));
    LLVMValueRef res = LLVMBuildAnd(ctx->builder, lhs, rhs, "v_band");
    set_val(ctx, res_id, jit_to_float_vector(ctx, res));
}

void handle_op_bitwise_or(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = jit_to_int_vector(ctx, get_val(ctx, operands[1]));
    LLVMValueRef res = LLVMBuildOr(ctx->builder, lhs, rhs, "v_bor");
    set_val(ctx, res_id, jit_to_float_vector(ctx, res));
}

void handle_op_bitwise_xor(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = jit_to_int_vector(ctx, get_val(ctx, operands[1]));
    LLVMValueRef res = LLVMBuildXor(ctx->builder, lhs, rhs, "v_bxor");
    set_val(ctx, res_id, jit_to_float_vector(ctx, res));
}

void handle_op_not(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef val = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMValueRef res = LLVMBuildNot(ctx->builder, val, "v_not");
    set_val(ctx, res_id, jit_to_float_vector(ctx, res));
}

void handle_op_shift_left_logical(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = jit_to_numeric_int_vector(ctx, get_val(ctx, operands[1]));
    LLVMValueRef res = LLVMBuildShl(ctx->builder, lhs, rhs, "v_shl");
    set_val(ctx, res_id, jit_to_float_vector(ctx, res));
}

void handle_op_shift_right_logical(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = jit_to_numeric_int_vector(ctx, get_val(ctx, operands[1]));
    LLVMValueRef res = LLVMBuildLShr(ctx->builder, lhs, rhs, "v_lshr");
    set_val(ctx, res_id, jit_to_float_vector(ctx, res));
}

void handle_op_shift_right_arithmetic(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = jit_to_numeric_int_vector(ctx, get_val(ctx, operands[1]));
    LLVMValueRef res = LLVMBuildAShr(ctx->builder, lhs, rhs, "v_ashr");
    set_val(ctx, res_id, jit_to_float_vector(ctx, res));
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
    set_val(ctx, res_id, LLVMBuildBitCast(ctx->builder, res, ctx->vec_float_type, "v_f2s_bc"));
}

void handle_op_convert_f_to_u(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef op = get_val(ctx, operands[0]);
    LLVMValueRef res = LLVMBuildFPToUI(ctx->builder, op, LLVMVectorType(ctx->int_type, SIMT_WIDTH), "v_f2u");
    set_val(ctx, res_id, LLVMBuildBitCast(ctx->builder, res, ctx->vec_float_type, "v_f2u_bc"));
}

void handle_op_convert_u_to_f(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef op = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMValueRef res = LLVMBuildUIToFP(ctx->builder, op, ctx->vec_float_type, "v_u2f");
    set_val(ctx, res_id, res);
}

void handle_op_inot_equal(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = jit_to_int_vector(ctx, get_val(ctx, operands[1]));
    LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntNE, lhs, rhs, "v_ine");
    set_val(ctx, res_id, cmp);
}

void handle_op_sgreater_than(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = jit_to_int_vector(ctx, get_val(ctx, operands[1]));
    LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSGT, lhs, rhs, "v_sgt");
    set_val(ctx, res_id, cmp);
}

void handle_op_ugreater_than(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = jit_to_int_vector(ctx, get_val(ctx, operands[1]));
    LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntUGT, lhs, rhs, "v_ugt");
    set_val(ctx, res_id, cmp);
}

void handle_op_sgreater_than_equal(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = jit_to_int_vector(ctx, get_val(ctx, operands[1]));
    LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSGE, lhs, rhs, "v_sge");
    set_val(ctx, res_id, cmp);
}

void handle_op_ugreater_than_equal(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = jit_to_int_vector(ctx, get_val(ctx, operands[1]));
    LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntUGE, lhs, rhs, "v_uge");
    set_val(ctx, res_id, cmp);
}

void handle_op_sless_than_equal(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = jit_to_int_vector(ctx, get_val(ctx, operands[1]));
    LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSLE, lhs, rhs, "v_sle");
    set_val(ctx, res_id, cmp);
}

void handle_op_uless_than_equal(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = jit_to_int_vector(ctx, get_val(ctx, operands[1]));
    LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntULE, lhs, rhs, "v_ule");
    set_val(ctx, res_id, cmp);
}

void handle_op_snegate(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef op = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMValueRef res = LLVMBuildNeg(ctx->builder, op, "v_sneg");
    set_val(ctx, res_id, jit_to_float_vector(ctx, res));
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
    LLVMValueRef lhs = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = jit_to_int_vector(ctx, get_val(ctx, operands[1]));
    LLVMValueRef res = LLVMBuildSRem(ctx->builder, lhs, rhs, "v_srem");
    set_val(ctx, res_id, jit_to_float_vector(ctx, res));
}

void handle_op_smod(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMValueRef rhs = jit_to_int_vector(ctx, get_val(ctx, operands[1]));
    LLVMValueRef srem = LLVMBuildSRem(ctx->builder, lhs, rhs, "v_smod_rem");
    LLVMValueRef srem_plus_rhs = LLVMBuildAdd(ctx->builder, srem, rhs, "v_smod_add");
    LLVMValueRef res = LLVMBuildSRem(ctx->builder, srem_plus_rhs, rhs, "v_smod");
    set_val(ctx, res_id, jit_to_float_vector(ctx, res));
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

void handle_op_atomic(JitContext* ctx, uint16_t opcode, uint32_t res_id, uint32_t type_id, uint32_t* operands, int operand_count)
{
    (void)type_id;
    (void)operand_count;
    uint32_t ptr_id = operands[0];
    LLVMValueRef ptr = get_val(ctx, ptr_id);
    if (!ptr) 
    {
        ptr = LLVMConstNull(ctx->ptr_type);
    }
    LLVMTypeRef i32_ptr_type = LLVMPointerType(ctx->int_type, 0);
    LLVMValueRef ptr_i32 = LLVMBuildBitCast(ctx->builder, ptr, i32_ptr_type, "atomic_ptr_i32");

    LLVMTypeRef vec_i32_type = LLVMVectorType(ctx->int_type, SIMT_WIDTH);
    LLVMValueRef res_vec = LLVMGetUndef(vec_i32_type);

    if (opcode == SpvOpAtomicLoad)
    {
        for (int lane = 0; lane < SIMT_WIDTH; lane++)
        {
            LLVMValueRef load_val = LLVMBuildLoad2(ctx->builder, ctx->int_type, ptr_i32, "atomic_load_val");
            LLVMSetOrdering(load_val, LLVMAtomicOrderingSequentiallyConsistent);
            res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, load_val, LLVMConstInt(ctx->int_type, lane, 0), "ins_at_res");
        }
        set_val(ctx, res_id, jit_to_float_vector(ctx, res_vec));
        return;
    }

    if (opcode == SpvOpAtomicStore)
    {
        uint32_t val_id = operands[3];
        LLVMValueRef val_vec = jit_to_int_vector(ctx, get_val(ctx, val_id));
        for (int lane = 0; lane < SIMT_WIDTH; lane++)
        {
            LLVMValueRef mask_lane = LLVMBuildExtractElement(ctx->builder, ctx->emask, LLVMConstInt(ctx->int_type, lane, 0), "m_lane");
            LLVMBasicBlockRef store_bb = LLVMAppendBasicBlockInContext(ctx->context, ctx->func, "at_store_bb");
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, ctx->func, "at_next_bb");

            LLVMBuildCondBr(ctx->builder, mask_lane, store_bb, next_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, store_bb);
            LLVMValueRef val_lane = LLVMBuildExtractElement(ctx->builder, val_vec, LLVMConstInt(ctx->int_type, lane, 0), "at_val_lane");
            LLVMValueRef store_inst = LLVMBuildStore(ctx->builder, val_lane, ptr_i32);
            LLVMSetOrdering(store_inst, LLVMAtomicOrderingSequentiallyConsistent);
            LLVMBuildBr(ctx->builder, next_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
            ctx->current_block = next_bb;
        }
        return;
    }

    if (opcode == SpvOpAtomicCompareExchange || opcode == SpvOpAtomicCompareExchangeWeak)
    {
        uint32_t val_id = operands[4];
        uint32_t cmp_id = operands[5];
        LLVMValueRef val_vec = jit_to_int_vector(ctx, get_val(ctx, val_id));
        LLVMValueRef cmp_vec = jit_to_int_vector(ctx, get_val(ctx, cmp_id));

        for (int lane = 0; lane < SIMT_WIDTH; lane++)
        {
            LLVMValueRef mask_lane = LLVMBuildExtractElement(ctx->builder, ctx->emask, LLVMConstInt(ctx->int_type, lane, 0), "m_lane");
            LLVMBasicBlockRef cur_bb = ctx->current_block;
            LLVMBasicBlockRef cmpxchg_bb = LLVMAppendBasicBlockInContext(ctx->context, ctx->func, "at_cmpxchg_bb");
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, ctx->func, "at_next_bb");

            LLVMBuildCondBr(ctx->builder, mask_lane, cmpxchg_bb, next_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, cmpxchg_bb);
            LLVMValueRef val_lane = LLVMBuildExtractElement(ctx->builder, val_vec, LLVMConstInt(ctx->int_type, lane, 0), "at_val_lane");
            LLVMValueRef cmp_lane = LLVMBuildExtractElement(ctx->builder, cmp_vec, LLVMConstInt(ctx->int_type, lane, 0), "at_cmp_lane");

            LLVMValueRef pair = LLVMBuildAtomicCmpXchg(ctx->builder, ptr_i32, cmp_lane, val_lane,
                                                       LLVMAtomicOrderingSequentiallyConsistent,
                                                       LLVMAtomicOrderingSequentiallyConsistent, false);
            LLVMValueRef old_val = LLVMBuildExtractValue(ctx->builder, pair, 0, "cmpxchg_old");
            LLVMBuildBr(ctx->builder, next_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
            ctx->current_block = next_bb;

            LLVMValueRef phi = LLVMBuildPhi(ctx->builder, ctx->int_type, "cmpxchg_phi");
            LLVMValueRef phi_vals[2] = { LLVMConstInt(ctx->int_type, 0, 0), old_val };
            LLVMBasicBlockRef phi_bbs[2] = { cur_bb, cmpxchg_bb };
            LLVMAddIncoming(phi, phi_vals, phi_bbs, 2);

            res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, phi, LLVMConstInt(ctx->int_type, lane, 0), "ins_at_res");
        }
        set_val(ctx, res_id, jit_to_float_vector(ctx, res_vec));
        return;
    }

    LLVMAtomicRMWBinOp rmw_op = LLVMAtomicRMWBinOpAdd;
    LLVMValueRef val_vec = NULL;

    if (opcode == SpvOpAtomicIIncrement) 
    {
        rmw_op = LLVMAtomicRMWBinOpAdd;
        LLVMValueRef ones[SIMT_WIDTH];
        for (int i = 0; i < SIMT_WIDTH; i++) ones[i] = LLVMConstInt(ctx->int_type, 1, 0);
        val_vec = LLVMConstVector(ones, SIMT_WIDTH);
    } 
    else if (opcode == SpvOpAtomicIDecrement) 
    {
        rmw_op = LLVMAtomicRMWBinOpSub;
        LLVMValueRef ones[SIMT_WIDTH];
        for (int i = 0; i < SIMT_WIDTH; i++) ones[i] = LLVMConstInt(ctx->int_type, 1, 0);
        val_vec = LLVMConstVector(ones, SIMT_WIDTH);
    } 
    else 
    {
        uint32_t val_id = operands[3];
        val_vec = jit_to_int_vector(ctx, get_val(ctx, val_id));
        switch (opcode) 
        {
            case SpvOpAtomicIAdd: rmw_op = LLVMAtomicRMWBinOpAdd; break;
            case SpvOpAtomicISub: rmw_op = LLVMAtomicRMWBinOpSub; break;
            case SpvOpAtomicSMin: rmw_op = LLVMAtomicRMWBinOpMin; break;
            case SpvOpAtomicUMin: rmw_op = LLVMAtomicRMWBinOpUMin; break;
            case SpvOpAtomicSMax: rmw_op = LLVMAtomicRMWBinOpMax; break;
            case SpvOpAtomicUMax: rmw_op = LLVMAtomicRMWBinOpUMax; break;
            case SpvOpAtomicAnd:  rmw_op = LLVMAtomicRMWBinOpAnd; break;
            case SpvOpAtomicOr:   rmw_op = LLVMAtomicRMWBinOpOr; break;
            case SpvOpAtomicXor:  rmw_op = LLVMAtomicRMWBinOpXor; break;
            case SpvOpAtomicExchange: rmw_op = LLVMAtomicRMWBinOpXchg; break;
            default: rmw_op = LLVMAtomicRMWBinOpAdd; break;
        }
    }

    for (int lane = 0; lane < SIMT_WIDTH; lane++)
    {
        LLVMValueRef mask_lane = LLVMBuildExtractElement(ctx->builder, ctx->emask, LLVMConstInt(ctx->int_type, lane, 0), "m_lane");
        LLVMBasicBlockRef cur_bb = ctx->current_block;
        LLVMBasicBlockRef rmw_bb = LLVMAppendBasicBlockInContext(ctx->context, ctx->func, "at_rmw_bb");
        LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, ctx->func, "at_next_bb");

        LLVMBuildCondBr(ctx->builder, mask_lane, rmw_bb, next_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, rmw_bb);
        LLVMValueRef val_lane = LLVMBuildExtractElement(ctx->builder, val_vec, LLVMConstInt(ctx->int_type, lane, 0), "at_val_lane");
        LLVMValueRef old_val = LLVMBuildAtomicRMW(ctx->builder, rmw_op, ptr_i32, val_lane, LLVMAtomicOrderingSequentiallyConsistent, false);
        LLVMBuildBr(ctx->builder, next_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        ctx->current_block = next_bb;

        LLVMValueRef phi = LLVMBuildPhi(ctx->builder, ctx->int_type, "rmw_phi");
        LLVMValueRef phi_vals[2] = { LLVMConstInt(ctx->int_type, 0, 0), old_val };
        LLVMBasicBlockRef phi_bbs[2] = { cur_bb, rmw_bb };
        LLVMAddIncoming(phi, phi_vals, phi_bbs, 2);

        res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, phi, LLVMConstInt(ctx->int_type, lane, 0), "ins_at_res");
    }
    set_val(ctx, res_id, jit_to_float_vector(ctx, res_vec));
}

void handle_op_group_non_uniform(JitContext* ctx, uint16_t opcode, uint32_t res_id, uint32_t type_id, uint32_t* operands, int operand_count)
{
    (void)type_id;
    (void)operand_count;

    if (opcode == SpvOpGroupNonUniformElect)
    {
        LLVMValueRef is_first_vec = LLVMGetUndef(ctx->vec_i1_type);
        LLVMValueRef prev_any = LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0);

        for (int i = 0; i < SIMT_WIDTH; i++) 
        {
            LLVMValueRef mask_i = LLVMBuildExtractElement(ctx->builder, ctx->emask, LLVMConstInt(ctx->int_type, i, 0), "m_i");
            LLVMValueRef not_prev = LLVMBuildNot(ctx->builder, prev_any, "not_prev");
            LLVMValueRef elected = LLVMBuildAnd(ctx->builder, mask_i, not_prev, "elected");

            is_first_vec = LLVMBuildInsertElement(ctx->builder, is_first_vec, elected, LLVMConstInt(ctx->int_type, i, 0), "elect_ins");
            prev_any = LLVMBuildOr(ctx->builder, prev_any, mask_i, "prev_any");
        }
        set_val(ctx, res_id, is_first_vec);
        return;
    }

    if (opcode == SpvOpGroupNonUniformAll || opcode == SpvOpSubgroupAllKHR)
    {
        LLVMValueRef pred_vec = get_val(ctx, operands[1]);
        LLVMValueRef all_val = LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0);
        for (int i = 0; i < SIMT_WIDTH; i++) 
        {
            LLVMValueRef m = LLVMBuildExtractElement(ctx->builder, ctx->emask, LLVMConstInt(ctx->int_type, i, 0), "m_i");
            LLVMValueRef p = LLVMBuildExtractElement(ctx->builder, pred_vec, LLVMConstInt(ctx->int_type, i, 0), "p_i");
            LLVMValueRef not_m = LLVMBuildNot(ctx->builder, m, "not_m");
            LLVMValueRef cond = LLVMBuildOr(ctx->builder, not_m, p, "lane_all_cond");
            all_val = LLVMBuildAnd(ctx->builder, all_val, cond, "all_acc");
        }
        LLVMValueRef res_vec = LLVMGetUndef(ctx->vec_i1_type);
        for (int i = 0; i < SIMT_WIDTH; i++) {
            res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, all_val, LLVMConstInt(ctx->int_type, i, 0), "all_ins");
        }
        set_val(ctx, res_id, res_vec);
        return;
    }

    if (opcode == SpvOpGroupNonUniformAny || opcode == SpvOpSubgroupAnyKHR)
    {
        LLVMValueRef pred_vec = get_val(ctx, operands[1]);
        LLVMValueRef any_val = LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0);
        for (int i = 0; i < SIMT_WIDTH; i++) {
            LLVMValueRef m = LLVMBuildExtractElement(ctx->builder, ctx->emask, LLVMConstInt(ctx->int_type, i, 0), "m_i");
            LLVMValueRef p = LLVMBuildExtractElement(ctx->builder, pred_vec, LLVMConstInt(ctx->int_type, i, 0), "p_i");
            LLVMValueRef cond = LLVMBuildAnd(ctx->builder, m, p, "lane_any_cond");
            any_val = LLVMBuildOr(ctx->builder, any_val, cond, "any_acc");
        }
        LLVMValueRef res_vec = LLVMGetUndef(ctx->vec_i1_type);
        for (int i = 0; i < SIMT_WIDTH; i++) {
            res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, any_val, LLVMConstInt(ctx->int_type, i, 0), "any_ins");
        }
        set_val(ctx, res_id, res_vec);
        return;
    }

    if (opcode == SpvOpGroupNonUniformAllEqual || opcode == SpvOpSubgroupAllEqualKHR)
    {
        LLVMValueRef val_vec = get_val(ctx, operands[1]);
        LLVMValueRef first_val = LLVMBuildExtractElement(ctx->builder, val_vec, LLVMConstInt(ctx->int_type, 0, 0), "first_v");
        LLVMValueRef all_eq = LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0);

        for (int i = 1; i < SIMT_WIDTH; i++)
        {
            LLVMValueRef m = LLVMBuildExtractElement(ctx->builder, ctx->emask, LLVMConstInt(ctx->int_type, i, 0), "m_i");
            LLVMValueRef v = LLVMBuildExtractElement(ctx->builder, val_vec, LLVMConstInt(ctx->int_type, i, 0), "v_i");
            LLVMValueRef eq;
            if (is_float_type(LLVMTypeOf(v))) 
            {
                eq = LLVMBuildFCmp(ctx->builder, LLVMRealOEQ, first_val, v, "cmp_eq");
            } 
            else 
            {
                eq = LLVMBuildICmp(ctx->builder, LLVMIntEQ, first_val, v, "cmp_eq");
            }
            LLVMValueRef not_m = LLVMBuildNot(ctx->builder, m, "not_m");
            LLVMValueRef cond = LLVMBuildOr(ctx->builder, not_m, eq, "lane_eq_cond");
            all_eq = LLVMBuildAnd(ctx->builder, all_eq, cond, "all_eq_acc");
        }
        LLVMValueRef res_vec = LLVMGetUndef(ctx->vec_i1_type);
        for (int i = 0; i < SIMT_WIDTH; i++) 
        {
            res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, all_eq, LLVMConstInt(ctx->int_type, i, 0), "eq_ins");
        }
        set_val(ctx, res_id, res_vec);
        return;
    }

    if (opcode == SpvOpGroupNonUniformBroadcast || opcode == SpvOpSubgroupReadInvocationKHR)
    {
        LLVMValueRef val_vec = get_val(ctx, operands[1]);
        LLVMValueRef id_val = get_val(ctx, operands[2]);
        LLVMValueRef id_i32 = LLVMBuildExtractElement(ctx->builder, jit_to_int_vector(ctx, id_val), LLVMConstInt(ctx->int_type, 0, 0), "bcast_id");

        LLVMValueRef broadcast_val = LLVMBuildExtractElement(ctx->builder, val_vec, id_i32, "bcast_v");
        LLVMValueRef res_vec = LLVMGetUndef(LLVMTypeOf(val_vec));
        for (int i = 0; i < SIMT_WIDTH; i++) 
        {
            res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, broadcast_val, LLVMConstInt(ctx->int_type, i, 0), "bcast_ins");
        }
        set_val(ctx, res_id, res_vec);
        return;
    }

    if (opcode == SpvOpGroupNonUniformBroadcastFirst || opcode == SpvOpSubgroupFirstInvocationKHR)
    {
        LLVMValueRef val_vec = get_val(ctx, operands[1]);
        LLVMValueRef first_val = LLVMBuildExtractElement(ctx->builder, val_vec, LLVMConstInt(ctx->int_type, 0, 0), "first_v");
        LLVMValueRef res_vec = LLVMGetUndef(LLVMTypeOf(val_vec));
        for (int i = 0; i < SIMT_WIDTH; i++)
         {
            res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, first_val, LLVMConstInt(ctx->int_type, i, 0), "bcast_ins");
        }
        set_val(ctx, res_id, res_vec);
        return;
    }

    if (opcode == SpvOpGroupNonUniformBallot || opcode == SpvOpSubgroupBallotKHR)
    {
        LLVMValueRef pred_vec = get_val(ctx, operands[1]);
        LLVMValueRef ballot_mask = LLVMConstInt(ctx->int_type, 0, 0);

        for (int i = 0; i < SIMT_WIDTH; i++) 
        {
            LLVMValueRef m = LLVMBuildExtractElement(ctx->builder, ctx->emask, LLVMConstInt(ctx->int_type, i, 0), "m_i");
            LLVMValueRef p = LLVMBuildExtractElement(ctx->builder, pred_vec, LLVMConstInt(ctx->int_type, i, 0), "p_i");
            LLVMValueRef active_bit = LLVMBuildAnd(ctx->builder, m, p, "act_bit");
            LLVMValueRef bit_i32 = LLVMBuildZExt(ctx->builder, active_bit, ctx->int_type, "bit_i32");
            LLVMValueRef shifted = LLVMBuildShl(ctx->builder, bit_i32, LLVMConstInt(ctx->int_type, i, 0), "shifted");
            ballot_mask = LLVMBuildOr(ctx->builder, ballot_mask, shifted, "ballot_acc");
        }

        LLVMValueRef bcast_x = LLVMGetUndef(ctx->vec_float_type);
        LLVMValueRef zero_v = LLVMConstNull(ctx->vec_float_type);
        LLVMValueRef ballot_float = LLVMBuildBitCast(ctx->builder, ballot_mask, ctx->float_type, "b_f_cast");

        for (int i = 0; i < SIMT_WIDTH; i++) 
        {
            bcast_x = LLVMBuildInsertElement(ctx->builder, bcast_x, ballot_float, LLVMConstInt(ctx->int_type, i, 0), "bcast_b");
        }

        LLVMValueRef uvec4_res = LLVMGetUndef(LLVMArrayType(ctx->vec_float_type, 4));
        uvec4_res = LLVMBuildInsertValue(ctx->builder, uvec4_res, bcast_x, 0, "uvec4_0");
        uvec4_res = LLVMBuildInsertValue(ctx->builder, uvec4_res, zero_v, 1, "uvec4_1");
        uvec4_res = LLVMBuildInsertValue(ctx->builder, uvec4_res, zero_v, 2, "uvec4_2");
        uvec4_res = LLVMBuildInsertValue(ctx->builder, uvec4_res, zero_v, 3, "uvec4_3");

        set_val(ctx, res_id, uvec4_res);
        return;
    }

    if (opcode == SpvOpGroupNonUniformInverseBallot)
    {
        LLVMValueRef ballot_uvec4 = get_val(ctx, operands[1]);
        LLVMValueRef ballot_x = LLVMBuildExtractValue(ctx->builder, ballot_uvec4, 0, "ballot_x");
        LLVMValueRef ballot_i32 = LLVMBuildBitCast(ctx->builder, ballot_x, LLVMVectorType(ctx->int_type, SIMT_WIDTH), "ballot_i32");

        LLVMValueRef res_vec = LLVMGetUndef(ctx->vec_i1_type);
        for (int i = 0; i < SIMT_WIDTH; i++) {
            LLVMValueRef bx = LLVMBuildExtractElement(ctx->builder, ballot_i32, LLVMConstInt(ctx->int_type, i, 0), "bx_i");
            LLVMValueRef bit_mask = LLVMConstInt(ctx->int_type, 1 << i, 0);
            LLVMValueRef is_set = LLVMBuildAnd(ctx->builder, bx, bit_mask, "is_set");
            LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntNE, is_set, LLVMConstInt(ctx->int_type, 0, 0), "cmp_set");
            res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, cmp, LLVMConstInt(ctx->int_type, i, 0), "inv_ins");
        }
        set_val(ctx, res_id, res_vec);
        return;
    }

    if (opcode == SpvOpGroupNonUniformShuffle || opcode == SpvOpGroupNonUniformShuffleXor ||
        opcode == SpvOpGroupNonUniformShuffleUp || opcode == SpvOpGroupNonUniformShuffleDown)
    {
        LLVMValueRef val_vec = get_val(ctx, operands[1]);
        LLVMValueRef param_vec = jit_to_numeric_int_vector(ctx, get_val(ctx, operands[2]));
        LLVMValueRef res_vec = LLVMGetUndef(LLVMTypeOf(val_vec));

        for (int i = 0; i < SIMT_WIDTH; i++) {
            LLVMValueRef p = LLVMBuildExtractElement(ctx->builder, param_vec, LLVMConstInt(ctx->int_type, i, 0), "p_i");
            LLVMValueRef src_idx;
            if (opcode == SpvOpGroupNonUniformShuffle) 
            {
                src_idx = LLVMBuildAnd(ctx->builder, p, LLVMConstInt(ctx->int_type, 15, 0), "shuf_idx");
            } 
            else if (opcode == SpvOpGroupNonUniformShuffleXor) 
            {
                src_idx = LLVMBuildXor(ctx->builder, LLVMConstInt(ctx->int_type, i, 0), p, "shuf_xor");
                src_idx = LLVMBuildAnd(ctx->builder, src_idx, LLVMConstInt(ctx->int_type, 15, 0), "shuf_idx");
            } 
            else if (opcode == SpvOpGroupNonUniformShuffleUp) 
            {
                src_idx = LLVMBuildSub(ctx->builder, LLVMConstInt(ctx->int_type, i, 0), p, "shuf_up");
            } 
            else 
            {
                src_idx = LLVMBuildAdd(ctx->builder, LLVMConstInt(ctx->int_type, i, 0), p, "shuf_down");
            }
            LLVMValueRef val_elem = LLVMBuildExtractElement(ctx->builder, val_vec, src_idx, "shuf_elem");
            res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, val_elem, LLVMConstInt(ctx->int_type, i, 0), "shuf_ins");
        }
        set_val(ctx, res_id, res_vec);
        return;
    }

    uint32_t group_op = 0;
    LLVMValueRef val_vec = NULL;
    if (operand_count >= 3) 
    {
        group_op = operands[1];
        val_vec = get_val(ctx, operands[2]);
    } 
    else 
    {
        val_vec = get_val(ctx, operands[1]);
    }

    bool is_float = is_float_type(LLVMTypeOf(val_vec));
    LLVMValueRef res_vec = LLVMGetUndef(LLVMTypeOf(val_vec));

    LLVMValueRef identity_val = NULL;
    switch (opcode) 
    {
        case SpvOpGroupNonUniformIAdd:
        case SpvOpGroupNonUniformFAdd:
        case SpvOpGroupNonUniformBitwiseOr:
        case SpvOpGroupNonUniformBitwiseXor:
        case SpvOpGroupNonUniformLogicalOr:
        case SpvOpGroupNonUniformLogicalXor:
            identity_val = is_float ? LLVMConstReal(ctx->float_type, 0.0) : LLVMConstInt(ctx->int_type, 0, 0);
            break;
        case SpvOpGroupNonUniformIMul:
        case SpvOpGroupNonUniformFMul:
            identity_val = is_float ? LLVMConstReal(ctx->float_type, 1.0) : LLVMConstInt(ctx->int_type, 1, 0);
            break;
        case SpvOpGroupNonUniformBitwiseAnd:
        case SpvOpGroupNonUniformLogicalAnd:
            identity_val = LLVMConstInt(ctx->int_type, ~0u, 0);
            break;
        case SpvOpGroupNonUniformFMin:
        case SpvOpGroupNonUniformSMin:
        case SpvOpGroupNonUniformUMin:
            identity_val = is_float ? LLVMConstReal(ctx->float_type, 1e38) : LLVMConstInt(ctx->int_type, 0x7FFFFFFF, 0);
            break;
        case SpvOpGroupNonUniformFMax:
        case SpvOpGroupNonUniformSMax:
        case SpvOpGroupNonUniformUMax:
            identity_val = is_float ? LLVMConstReal(ctx->float_type, -1e38) : LLVMConstInt(ctx->int_type, 0x80000000, 0);
            break;
        default:
            identity_val = is_float ? LLVMConstReal(ctx->float_type, 0.0) : LLVMConstInt(ctx->int_type, 0, 0);
            break;
    }

    LLVMValueRef accum = identity_val;
    LLVMValueRef scan_results[SIMT_WIDTH];

    for (int i = 0; i < SIMT_WIDTH; i++)
    {
        LLVMValueRef elem = LLVMBuildExtractElement(ctx->builder, val_vec, LLVMConstInt(ctx->int_type, i, 0), "red_elem");
        LLVMValueRef m = LLVMBuildExtractElement(ctx->builder, ctx->emask, LLVMConstInt(ctx->int_type, i, 0), "m_i");

        LLVMValueRef new_accum = NULL;
        switch (opcode) 
        {
            case SpvOpGroupNonUniformFAdd:
                new_accum = LLVMBuildFAdd(ctx->builder, accum, elem, "fadd_acc");
                break;
            case SpvOpGroupNonUniformIAdd:
                new_accum = LLVMBuildAdd(ctx->builder, accum, elem, "iadd_acc");
                break;
            case SpvOpGroupNonUniformFMul:
                new_accum = LLVMBuildFMul(ctx->builder, accum, elem, "fmul_acc");
                break;
            case SpvOpGroupNonUniformIMul:
                new_accum = LLVMBuildMul(ctx->builder, accum, elem, "imul_acc");
                break;
            case SpvOpGroupNonUniformFMin: 
            {
                unsigned min_id = LLVMLookupIntrinsicID("llvm.minnum", 11);
                LLVMValueRef min_func = LLVMGetIntrinsicDeclaration(ctx->module, min_id, &ctx->float_type, 1);
                LLVMValueRef args[2] = { accum, elem };
                new_accum = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(min_func), min_func, args, 2, "fmin_acc");
                break;
            }
            case SpvOpGroupNonUniformFMax: 
            {
                unsigned max_id = LLVMLookupIntrinsicID("llvm.maxnum", 11);
                LLVMValueRef max_func = LLVMGetIntrinsicDeclaration(ctx->module, max_id, &ctx->float_type, 1);
                LLVMValueRef args[2] = { accum, elem };
                new_accum = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(max_func), max_func, args, 2, "fmax_acc");
                break;
            }
            case SpvOpGroupNonUniformSMin: 
            {
                LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSLT, elem, accum, "smin_cmp");
                new_accum = LLVMBuildSelect(ctx->builder, cmp, elem, accum, "smin_acc");
                break;
            }
            case SpvOpGroupNonUniformSMax:
            {
                LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSGT, elem, accum, "smax_cmp");
                new_accum = LLVMBuildSelect(ctx->builder, cmp, elem, accum, "smax_acc");
                break;
            }
            case SpvOpGroupNonUniformUMin: 
            {
                LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntULT, elem, accum, "umin_cmp");
                new_accum = LLVMBuildSelect(ctx->builder, cmp, elem, accum, "umin_acc");
                break;
            }
            case SpvOpGroupNonUniformUMax: 
            {
                LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntUGT, elem, accum, "umax_cmp");
                new_accum = LLVMBuildSelect(ctx->builder, cmp, elem, accum, "umax_acc");
                break;
            }
            case SpvOpGroupNonUniformBitwiseAnd:
                new_accum = LLVMBuildAnd(ctx->builder, accum, elem, "band_acc");
                break;
            case SpvOpGroupNonUniformBitwiseOr:
                new_accum = LLVMBuildOr(ctx->builder, accum, elem, "bor_acc");
                break;
            case SpvOpGroupNonUniformBitwiseXor:
                new_accum = LLVMBuildXor(ctx->builder, accum, elem, "bxor_acc");
                break;
            default:
                new_accum = is_float ? LLVMBuildFAdd(ctx->builder, accum, elem, "def_acc") : LLVMBuildAdd(ctx->builder, accum, elem, "def_acc");
                break;
        }

        if (group_op == 2) 
        {
            scan_results[i] = accum;
        }

        accum = LLVMBuildSelect(ctx->builder, m, new_accum, accum, "sel_acc");

        if (group_op == 1) 
        {
            scan_results[i] = accum;
        }
    }

    if (group_op == 0) 
    {
        for (int i = 0; i < SIMT_WIDTH; i++) 
        {
            res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, accum, LLVMConstInt(ctx->int_type, i, 0), "red_ins");
        }
    } 
    else 
    {
        for (int i = 0; i < SIMT_WIDTH; i++) 
        {
            res_vec = LLVMBuildInsertElement(ctx->builder, res_vec, scan_results[i], LLVMConstInt(ctx->int_type, i, 0), "scan_ins");
        }
    }

    set_val(ctx, res_id, res_vec);
}