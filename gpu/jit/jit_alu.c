#include "jit_alu.h"
#include "glsl_std_450.h"
#include <llvm-c/Core.h>
#define CREATE_CONST_VEC(name, val) \
    for(int i = 0; i < SIMT_WIDTH; i++) scalars[i] = LLVMConstReal(f32_type, val); \
    LLVMValueRef name = LLVMConstVector(scalars, SIMT_WIDTH);
   
#define CREATE_CONST_VEC_N(name, val,size) \
    for(int i = 0; i < size; i++) scalars[i] = LLVMConstReal(f32_type, val); \
    LLVMValueRef name = LLVMConstVector(scalars, size);
    
void handle_op_constant(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands) 
{
    uint8_t kind = ctx->type_kind_map[type_id];
    LLVMValueRef const_val;

    if (kind == SpvOpTypeFloat)
    {
        float val = *(float*)&operands[0];
        const_val = LLVMConstReal(ctx->float_type, val);
    } 
    else 
    {
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

void handle_op_fadd(JitContext* ctx, uint32_t res_id, uint32_t* operands) 
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    
    LLVMValueRef res = LLVMBuildFAdd(ctx->builder, lhs, rhs, "v_fadd");
    set_val(ctx, res_id, res);
}

void handle_op_fmul(JitContext* ctx, uint32_t res_id, uint32_t* operands) 
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildFMul(ctx->builder, lhs, rhs, "v_fmul");
    set_val(ctx, res_id, res);
}

void handle_op_fdiv(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildFDiv(ctx->builder, lhs, rhs, "v_fdiv");
    set_val(ctx, res_id, res);
}
void handle_op_fsub(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildFSub(ctx->builder, lhs, rhs, "v_fsub");
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
    LLVMValueRef res = LLVMBuildSub(ctx->builder, lhs, rhs, "v_isub");
    set_val(ctx, res_id, res);
}
void handle_op_imul(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildMul(ctx->builder, lhs, rhs, "v_imul");
    set_val(ctx, res_id, res);
}
void handle_op_sdiv(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildSDiv(ctx->builder, lhs, rhs, "v_sdiv");
    set_val(ctx, res_id, res);
}
void handle_op_udiv(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildUDiv(ctx->builder, lhs, rhs, "v_udiv");
    set_val(ctx, res_id, res);
}
void handle_op_iadd(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef lhs = get_val(ctx, operands[0]);
    LLVMValueRef rhs = get_val(ctx, operands[1]);
    LLVMValueRef res = LLVMBuildAdd(ctx->builder, lhs, rhs, "v_iadd");
    set_val(ctx, res_id, res);
}

void handle_op_sitof(JitContext* ctx, uint32_t res_id, uint32_t* operands) 
{
    LLVMValueRef op = get_val(ctx, operands[0]); 
    LLVMValueRef res = LLVMBuildSIToFP(ctx->builder,op, ctx->vec_float_type, "v_sitof");
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

    LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSLT, lhs, rhs, "v_slt");

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
    printf("Handling extended instruction with ID %d\n", instr_id);
    ctx->glsl_handlers[instr_id](ctx, res_id, &operands[2]);
}

void handle_op_composite_construct(JitContext* ctx, uint32_t res_id, uint32_t type_id,  uint32_t* operands)
{
    SpvTypeInfo* info = &ctx->type_info[type_id];

    if (info->opcode == SpvOpTypeVector)
    {
        uint32_t num_components = info->member_count;

        LLVMTypeRef lane_vec_type = LLVMVectorType(ctx->float_type, SIMT_WIDTH);
        LLVMTypeRef array_type = LLVMArrayType(lane_vec_type, num_components);

        LLVMValueRef composite = LLVMGetUndef(array_type);

        for (uint32_t i = 0; i < num_components; i++)
        {
            LLVMValueRef component_vec = get_val(ctx, operands[i]);
            composite = LLVMBuildInsertValue(ctx->builder, composite, component_vec, i, "pack");
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

        printf("  Matrix %u x %u\n", num_cols, num_rows);

        LLVMTypeRef lane_vec_type = LLVMVectorType(ctx->float_type, SIMT_WIDTH);
        LLVMTypeRef column_type = LLVMArrayType(lane_vec_type, num_rows);
        LLVMTypeRef matrix_type = LLVMArrayType(column_type, num_cols);

        LLVMValueRef matrix = LLVMGetUndef(matrix_type);

        for (uint32_t c = 0; c < num_cols; c++)
        {
            LLVMValueRef column = get_val(ctx, operands[c]);

            matrix = LLVMBuildInsertValue(
                ctx->builder,
                matrix,
                column,
                c,
                "insert_col");
        }

        set_val(ctx, res_id, matrix);
        return;
    }
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
    printf("Extracting composite with %u indices\n", num_indices);

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
    unsigned intrinsic_id = LLVMLookupIntrinsicID("llvm.atan2", 9); 
    LLVMValueRef atan2_func = LLVMGetIntrinsicDeclaration(ctx->module, intrinsic_id, &vec_float_type, 2);
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