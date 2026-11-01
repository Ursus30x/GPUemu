#include "jit_ext.h"
#include "jit_alu.h"
#include "jit_mem.h"
#include "jit_atomic.h"
#include "glsl_std_450.h"
#include <llvm-c/Core.h>
#include <math.h>
#include "debug_gpu.h"

void handle_op_ext_instr(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    uint32_t instr_id = operands[1];
    ctx->glsl_handlers[instr_id](ctx, res_id, &operands[2]);
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

    ctx->glsl_handlers[GLSLstd450Round] = handle_ext_round;
    ctx->glsl_handlers[GLSLstd450RoundEven] = handle_ext_round_even;
    ctx->glsl_handlers[GLSLstd450Trunc] = handle_ext_trunc;
    ctx->glsl_handlers[GLSLstd450Floor] = handle_ext_floor;
    ctx->glsl_handlers[GLSLstd450Ceil] = handle_ext_ceil;
    ctx->glsl_handlers[GLSLstd450Fract] = handle_ext_fract;
    ctx->glsl_handlers[GLSLstd450Fma] = handle_ext_fma;
    ctx->glsl_handlers[GLSLstd450Modf] = handle_ext_modf;
    ctx->glsl_handlers[GLSLstd450ModfStruct] = handle_ext_modf_struct;

    ctx->glsl_handlers[GLSLstd450Radians] = handle_ext_radians;
    ctx->glsl_handlers[GLSLstd450Degrees] = handle_ext_degrees;
    ctx->glsl_handlers[GLSLstd450Asin] = handle_ext_asin;
    ctx->glsl_handlers[GLSLstd450Acos] = handle_ext_acos;
    ctx->glsl_handlers[GLSLstd450Atan] = handle_ext_atan;
    ctx->glsl_handlers[GLSLstd450Sinh] = handle_ext_sinh;
    ctx->glsl_handlers[GLSLstd450Cosh] = handle_ext_cosh;
    ctx->glsl_handlers[GLSLstd450Tanh] = handle_ext_tanh;
    ctx->glsl_handlers[GLSLstd450Asinh] = handle_ext_asinh;
    ctx->glsl_handlers[GLSLstd450Acosh] = handle_ext_acosh;
    ctx->glsl_handlers[GLSLstd450Atanh] = handle_ext_atanh;

    ctx->glsl_handlers[GLSLstd450Exp2] = handle_ext_exp2;
    ctx->glsl_handlers[GLSLstd450Log2] = handle_ext_log2;
    ctx->glsl_handlers[GLSLstd450InverseSqrt] = handle_ext_inverse_sqrt;
    ctx->glsl_handlers[GLSLstd450Frexp] = handle_ext_frexp;
    ctx->glsl_handlers[GLSLstd450FrexpStruct] = handle_ext_frexp_struct;
    ctx->glsl_handlers[GLSLstd450Ldexp] = handle_ext_ldexp;

    ctx->glsl_handlers[GLSLstd450SAbs] = handle_ext_sabs;
    ctx->glsl_handlers[GLSLstd450SSign] = handle_ext_ssign;
    ctx->glsl_handlers[GLSLstd450UMin] = handle_ext_umin;
    ctx->glsl_handlers[GLSLstd450SMin] = handle_ext_smin;
    ctx->glsl_handlers[GLSLstd450UMax] = handle_ext_umax;
    ctx->glsl_handlers[GLSLstd450SMax] = handle_ext_smax;
    ctx->glsl_handlers[GLSLstd450UClamp] = handle_ext_uclamp;
    ctx->glsl_handlers[GLSLstd450SClamp] = handle_ext_sclamp;
    ctx->glsl_handlers[GLSLstd450IMix] = handle_ext_imix;

    ctx->glsl_handlers[GLSLstd450NMin] = handle_ext_nmin;
    ctx->glsl_handlers[GLSLstd450NMax] = handle_ext_nmax;
    ctx->glsl_handlers[GLSLstd450NClamp] = handle_ext_nclamp;

    ctx->glsl_handlers[GLSLstd450Determinant] = handle_ext_determinant;
    ctx->glsl_handlers[GLSLstd450MatrixInverse] = handle_ext_matrix_inverse;

    ctx->glsl_handlers[GLSLstd450FaceForward] = handle_ext_face_forward;

    ctx->glsl_handlers[GLSLstd450PackSnorm4x8] = handle_ext_pack_snorm4x8;
    ctx->glsl_handlers[GLSLstd450PackUnorm4x8] = handle_ext_pack_unorm4x8;
    ctx->glsl_handlers[GLSLstd450PackSnorm2x16] = handle_ext_pack_snorm2x16;
    ctx->glsl_handlers[GLSLstd450PackUnorm2x16] = handle_ext_pack_unorm2x16;
    ctx->glsl_handlers[GLSLstd450PackHalf2x16] = handle_ext_pack_half2x16;
    ctx->glsl_handlers[GLSLstd450PackDouble2x32] = handle_ext_pack_double2x32;
    ctx->glsl_handlers[GLSLstd450UnpackSnorm2x16] = handle_ext_unpack_snorm2x16;
    ctx->glsl_handlers[GLSLstd450UnpackUnorm2x16] = handle_ext_unpack_unorm2x16;
    ctx->glsl_handlers[GLSLstd450UnpackHalf2x16] = handle_ext_unpack_half2x16;
    ctx->glsl_handlers[GLSLstd450UnpackSnorm4x8] = handle_ext_unpack_snorm4x8;
    ctx->glsl_handlers[GLSLstd450UnpackUnorm4x8] = handle_ext_unpack_unorm4x8;
    ctx->glsl_handlers[GLSLstd450UnpackDouble2x32] = handle_ext_unpack_double2x32;

    ctx->glsl_handlers[GLSLstd450FindILsb] = handle_ext_find_ilsb;
    ctx->glsl_handlers[GLSLstd450FindSMsb] = handle_ext_find_smsb;
    ctx->glsl_handlers[GLSLstd450FindUMsb] = handle_ext_find_umsb;

    ctx->glsl_handlers[GLSLstd450InterpolateAtCentroid] = handle_ext_interpolate_at_centroid;
    ctx->glsl_handlers[GLSLstd450InterpolateAtSample] = handle_ext_interpolate_at_sample;
    ctx->glsl_handlers[GLSLstd450InterpolateAtOffset] = handle_ext_interpolate_at_offset;
}

static inline LLVMValueRef const_simt_float(JitContext* ctx, float val)
{
    LLVMValueRef scalars[SIMT_WIDTH];
    for (int i = 0; i < SIMT_WIDTH; i++) {
        scalars[i] = LLVMConstReal(ctx->float_type, val);
    }
    return LLVMConstVector(scalars, SIMT_WIDTH);
}

/* Executes a 1-argument LLVM intrinsic across a single SIMT register (<16 x float>) */
static LLVMValueRef build_unary_simt_intrinsic(JitContext* ctx, LLVMValueRef simt_val, const char* intrinsic_name)
{
    LLVMTypeRef vec_float_type = ctx->vec_float_type;
    unsigned intrinsic_id = LLVMLookupIntrinsicID(intrinsic_name, strlen(intrinsic_name));
    LLVMValueRef func = LLVMGetIntrinsicDeclaration(ctx->module, intrinsic_id, &vec_float_type, 1);
    LLVMTypeRef func_sig = LLVMGlobalGetValueType(func);
    return LLVMBuildCall2(ctx->builder, func_sig, func, &simt_val, 1, "unary_intr_res");
}

/* Dispatches unary intrinsics component-wise for float (<16 x float>) or vec2/vec3/vec4 ([N x <16 x float>]) */
static LLVMValueRef build_unary_intrinsic_op(JitContext* ctx, LLVMValueRef val, const char* intrinsic_name)
{
    LLVMTypeRef type = LLVMTypeOf(val);
    if (LLVMGetTypeKind(type) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(type);
        LLVMValueRef result = LLVMGetUndef(type);
        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem = LLVMBuildExtractValue(ctx->builder, val, i, "comp_val");
            LLVMValueRef elem_res = build_unary_simt_intrinsic(ctx, elem, intrinsic_name);
            result = LLVMBuildInsertValue(ctx->builder, result, elem_res, i, "comp_res");
        }
        return result;
    }
    return build_unary_simt_intrinsic(ctx, val, intrinsic_name);
}

/* Executes a 2-argument LLVM intrinsic across a single SIMT register (<16 x float>) */
static LLVMValueRef build_binary_simt_intrinsic(JitContext* ctx, LLVMValueRef simt_a, LLVMValueRef simt_b, const char* intrinsic_name)
{
    LLVMTypeRef vec_float_type = ctx->vec_float_type;
    unsigned intrinsic_id = LLVMLookupIntrinsicID(intrinsic_name, strlen(intrinsic_name));
    LLVMValueRef func = LLVMGetIntrinsicDeclaration(ctx->module, intrinsic_id, &vec_float_type, 1);
    LLVMTypeRef func_sig = LLVMGlobalGetValueType(func);
    LLVMValueRef args[] = { simt_a, simt_b };
    return LLVMBuildCall2(ctx->builder, func_sig, func, args, 2, "bin_intr_res");
}

/* Dispatches binary intrinsics component-wise for float or vec2/vec3/vec4 */
static LLVMValueRef build_binary_intrinsic_op(JitContext* ctx, LLVMValueRef a, LLVMValueRef b, const char* intrinsic_name)
{
    LLVMTypeRef type_a = LLVMTypeOf(a);
    LLVMTypeRef type_b = LLVMTypeOf(b);

    if (LLVMGetTypeKind(type_a) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(type_a);
        LLVMValueRef result = LLVMGetUndef(type_a);
        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem_a = LLVMBuildExtractValue(ctx->builder, a, i, "elem_a");
            LLVMValueRef elem_b = (LLVMGetTypeKind(type_b) == LLVMArrayTypeKind)
                ? LLVMBuildExtractValue(ctx->builder, b, i, "elem_b")
                : b;
            LLVMValueRef elem_res = build_binary_simt_intrinsic(ctx, elem_a, elem_b, intrinsic_name);
            result = LLVMBuildInsertValue(ctx->builder, result, elem_res, i, "comp_res");
        }
        return result;
    }
    return build_binary_simt_intrinsic(ctx, a, b, intrinsic_name);
}

/* Clamp helper */
static LLVMValueRef build_clamp_simt(JitContext* ctx, LLVMValueRef val, LLVMValueRef min_val, LLVMValueRef max_val)
{
    LLVMTypeRef vec_float_type = ctx->vec_float_type;
    unsigned max_id = LLVMLookupIntrinsicID("llvm.maxnum", 11);
    unsigned min_id = LLVMLookupIntrinsicID("llvm.minnum", 11);

    LLVMValueRef max_func = LLVMGetIntrinsicDeclaration(ctx->module, max_id, &vec_float_type, 1);
    LLVMValueRef min_func = LLVMGetIntrinsicDeclaration(ctx->module, min_id, &vec_float_type, 1);

    LLVMValueRef args_max[] = { val, min_val };
    LLVMValueRef clamped_min = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(max_func), max_func, args_max, 2, "clamp_max_tmp");

    LLVMValueRef args_min[] = { clamped_min, max_val };
    return LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(min_func), min_func, args_min, 2, "fclamp_res");
}

static LLVMValueRef build_clamp_op(JitContext* ctx, LLVMValueRef val, LLVMValueRef min_val, LLVMValueRef max_val)
{
    LLVMTypeRef type_val = LLVMTypeOf(val);

    if (LLVMGetTypeKind(type_val) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(type_val);
        LLVMValueRef result = LLVMGetUndef(type_val);

        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem_val = LLVMBuildExtractValue(ctx->builder, val, i, "elem_val");
            LLVMValueRef elem_min = (LLVMGetTypeKind(LLVMTypeOf(min_val)) == LLVMArrayTypeKind)
                ? LLVMBuildExtractValue(ctx->builder, min_val, i, "elem_min")
                : min_val;
            LLVMValueRef elem_max = (LLVMGetTypeKind(LLVMTypeOf(max_val)) == LLVMArrayTypeKind)
                ? LLVMBuildExtractValue(ctx->builder, max_val, i, "elem_max")
                : max_val;

            LLVMValueRef elem_res = build_clamp_simt(ctx, elem_val, elem_min, elem_max);
            result = LLVMBuildInsertValue(ctx->builder, result, elem_res, i, "pack_clamp");
        }
        return result;
    }

    return build_clamp_simt(ctx, val, min_val, max_val);
}

/* Smoothstep helper */
static LLVMValueRef build_smoothstep_simt(JitContext* ctx, LLVMValueRef edge0, LLVMValueRef edge1, LLVMValueRef x)
{
    LLVMValueRef num = LLVMBuildFSub(ctx->builder, x, edge0, "sub_x_e0");
    LLVMValueRef den = LLVMBuildFSub(ctx->builder, edge1, edge0, "sub_e1_e0");
    LLVMValueRef t_raw = LLVMBuildFDiv(ctx->builder, num, den, "t_raw");

    LLVMValueRef zero_vec = const_simt_float(ctx, 0.0f);
    LLVMValueRef one_vec = const_simt_float(ctx, 1.0f);
    LLVMValueRef three_v = const_simt_float(ctx, 3.0f);
    LLVMValueRef two_v = const_simt_float(ctx, 2.0f);

    LLVMValueRef t = build_clamp_simt(ctx, t_raw, zero_vec, one_vec);

    LLVMValueRef two_t = LLVMBuildFMul(ctx->builder, two_v, t, "two_t");
    LLVMValueRef term3 = LLVMBuildFSub(ctx->builder, three_v, two_t, "term3");
    LLVMValueRef t_sq = LLVMBuildFMul(ctx->builder, t, t, "t_sq");
    return LLVMBuildFMul(ctx->builder, t_sq, term3, "smoothstep_v");
}

static LLVMValueRef build_smoothstep_op(JitContext* ctx, LLVMValueRef edge0, LLVMValueRef edge1, LLVMValueRef x)
{
    LLVMTypeRef type_x = LLVMTypeOf(x);
    if (LLVMGetTypeKind(type_x) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(type_x);
        LLVMValueRef result = LLVMGetUndef(type_x);
        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem_x = LLVMBuildExtractValue(ctx->builder, x, i, "elem_x");
            LLVMValueRef elem_e0 = (LLVMGetTypeKind(LLVMTypeOf(edge0)) == LLVMArrayTypeKind)
                ? LLVMBuildExtractValue(ctx->builder, edge0, i, "elem_e0")
                : edge0;
            LLVMValueRef elem_e1 = (LLVMGetTypeKind(LLVMTypeOf(edge1)) == LLVMArrayTypeKind)
                ? LLVMBuildExtractValue(ctx->builder, edge1, i, "elem_e1")
                : edge1;
            LLVMValueRef elem_res = build_smoothstep_simt(ctx, elem_e0, elem_e1, elem_x);
            result = LLVMBuildInsertValue(ctx->builder, result, elem_res, i, "pack_smoothstep");
        }
        return result;
    }
    return build_smoothstep_simt(ctx, edge0, edge1, x);
}

/* Fmix helper */
static LLVMValueRef build_fmix_simt(JitContext* ctx, LLVMValueRef x, LLVMValueRef y, LLVMValueRef a)
{
    LLVMValueRef one_vec = const_simt_float(ctx, 1.0f);
    LLVMValueRef one_minus_a = LLVMBuildFSub(ctx->builder, one_vec, a, "one_minus_a");
    LLVMValueRef x_part = LLVMBuildFMul(ctx->builder, x, one_minus_a, "x_part");
    LLVMValueRef y_part = LLVMBuildFMul(ctx->builder, y, a, "y_part");
    return LLVMBuildFAdd(ctx->builder, x_part, y_part, "fmix_v");
}

static LLVMValueRef build_fmix_op(JitContext* ctx, LLVMValueRef x, LLVMValueRef y, LLVMValueRef a)
{
    LLVMTypeRef type_x = LLVMTypeOf(x);
    if (LLVMGetTypeKind(type_x) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(type_x);
        LLVMValueRef result = LLVMGetUndef(type_x);
        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem_x = LLVMBuildExtractValue(ctx->builder, x, i, "elem_x");
            LLVMValueRef elem_y = (LLVMGetTypeKind(LLVMTypeOf(y)) == LLVMArrayTypeKind)
                ? LLVMBuildExtractValue(ctx->builder, y, i, "elem_y")
                : y;
            LLVMValueRef elem_a = (LLVMGetTypeKind(LLVMTypeOf(a)) == LLVMArrayTypeKind)
                ? LLVMBuildExtractValue(ctx->builder, a, i, "elem_a")
                : a;
            LLVMValueRef elem_res = build_fmix_simt(ctx, elem_x, elem_y, elem_a);
            result = LLVMBuildInsertValue(ctx->builder, result, elem_res, i, "pack_fmix");
        }
        return result;
    }
    return build_fmix_simt(ctx, x, y, a);
}

/* Fsign helper */
static LLVMValueRef build_fsign_simt(JitContext* ctx, LLVMValueRef x)
{
    LLVMTypeRef vec_float_type = ctx->vec_float_type;
    LLVMValueRef one_v = const_simt_float(ctx, 1.0f);
    LLVMValueRef zero_v = const_simt_float(ctx, 0.0f);

    unsigned copy_id = LLVMLookupIntrinsicID("llvm.copysign", 13);
    LLVMValueRef copy_f = LLVMGetIntrinsicDeclaration(ctx->module, copy_id, &vec_float_type, 1);
    
    LLVMValueRef args[] = { one_v, x };
    LLVMValueRef sign_bits = LLVMBuildCall2(ctx->builder, 
                                           LLVMGlobalGetValueType(copy_f), 
                                           copy_f, 
                                           args, 2, 
                                           "sign_bits");

    LLVMValueRef is_zero = LLVMBuildFCmp(ctx->builder, LLVMRealOEQ, x, zero_v, "is_zero_mask");
    return LLVMBuildSelect(ctx->builder, is_zero, zero_v, sign_bits, "fsign_res");
}

static LLVMValueRef build_fsign_op(JitContext* ctx, LLVMValueRef x)
{
    LLVMTypeRef type = LLVMTypeOf(x);
    if (LLVMGetTypeKind(type) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(type);
        LLVMValueRef result = LLVMGetUndef(type);
        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem = LLVMBuildExtractValue(ctx->builder, x, i, "elem");
            LLVMValueRef elem_res = build_fsign_simt(ctx, elem);
            result = LLVMBuildInsertValue(ctx->builder, result, elem_res, i, "pack_fsign");
        }
        return result;
    }
    return build_fsign_simt(ctx, x);
}

/* Step helper */
static LLVMValueRef build_step_simt(JitContext* ctx, LLVMValueRef edge, LLVMValueRef x)
{
    LLVMValueRef one_v = const_simt_float(ctx, 1.0f);
    LLVMValueRef zero_v = const_simt_float(ctx, 0.0f);
    LLVMValueRef cmp = LLVMBuildFCmp(ctx->builder, LLVMRealOLT, x, edge, "step_cmp");
    return LLVMBuildSelect(ctx->builder, cmp, zero_v, one_v, "step_res");
}

static LLVMValueRef build_step_op(JitContext* ctx, LLVMValueRef edge, LLVMValueRef x)
{
    LLVMTypeRef type_x = LLVMTypeOf(x);
    if (LLVMGetTypeKind(type_x) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(type_x);
        LLVMValueRef result = LLVMGetUndef(type_x);
        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem_x = LLVMBuildExtractValue(ctx->builder, x, i, "elem_x");
            LLVMValueRef elem_edge = (LLVMGetTypeKind(LLVMTypeOf(edge)) == LLVMArrayTypeKind)
                ? LLVMBuildExtractValue(ctx->builder, edge, i, "elem_edge")
                : edge;
            LLVMValueRef elem_res = build_step_simt(ctx, elem_edge, elem_x);
            result = LLVMBuildInsertValue(ctx->builder, result, elem_res, i, "pack_step");
        }
        return result;
    }
    return build_step_simt(ctx, edge, x);
}

/* Fract helper */
static LLVMValueRef build_fract_simt(JitContext* ctx, LLVMValueRef x)
{
    LLVMValueRef floor_x = build_unary_simt_intrinsic(ctx, x, "llvm.floor");
    return LLVMBuildFSub(ctx->builder, x, floor_x, "fract_v");
}

static LLVMValueRef build_fract_op(JitContext* ctx, LLVMValueRef x)
{
    LLVMTypeRef type = LLVMTypeOf(x);
    if (LLVMGetTypeKind(type) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(type);
        LLVMValueRef result = LLVMGetUndef(type);
        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem = LLVMBuildExtractValue(ctx->builder, x, i, "elem");
            LLVMValueRef elem_res = build_fract_simt(ctx, elem);
            result = LLVMBuildInsertValue(ctx->builder, result, elem_res, i, "pack_fract");
        }
        return result;
    }
    return build_fract_simt(ctx, x);
}

/* Fma helper */
static LLVMValueRef build_fma_simt(JitContext* ctx, LLVMValueRef a, LLVMValueRef b, LLVMValueRef c)
{
    LLVMTypeRef vec_float_type = ctx->vec_float_type;
    unsigned fma_id = LLVMLookupIntrinsicID("llvm.fma", 8);
    LLVMValueRef func = LLVMGetIntrinsicDeclaration(ctx->module, fma_id, &vec_float_type, 1);
    LLVMValueRef args[] = { a, b, c };
    return LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(func), func, args, 3, "fma_res");
}

static LLVMValueRef build_fma_op(JitContext* ctx, LLVMValueRef a, LLVMValueRef b, LLVMValueRef c)
{
    LLVMTypeRef type = LLVMTypeOf(a);
    if (LLVMGetTypeKind(type) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(type);
        LLVMValueRef result = LLVMGetUndef(type);
        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem_a = LLVMBuildExtractValue(ctx->builder, a, i, "elem_a");
            LLVMValueRef elem_b = (LLVMGetTypeKind(LLVMTypeOf(b)) == LLVMArrayTypeKind) ? LLVMBuildExtractValue(ctx->builder, b, i, "elem_b") : b;
            LLVMValueRef elem_c = (LLVMGetTypeKind(LLVMTypeOf(c)) == LLVMArrayTypeKind) ? LLVMBuildExtractValue(ctx->builder, c, i, "elem_c") : c;
            LLVMValueRef elem_res = build_fma_simt(ctx, elem_a, elem_b, elem_c);
            result = LLVMBuildInsertValue(ctx->builder, result, elem_res, i, "pack_fma");
        }
        return result;
    }
    return build_fma_simt(ctx, a, b, c);
}

/* Inverse Sqrt helper */
static LLVMValueRef build_inverse_sqrt_simt(JitContext* ctx, LLVMValueRef x)
{
    LLVMValueRef sqrt_x = build_unary_simt_intrinsic(ctx, x, "llvm.sqrt");
    LLVMValueRef one_vec = const_simt_float(ctx, 1.0f);
    return LLVMBuildFDiv(ctx->builder, one_vec, sqrt_x, "inversesqrt_res");
}

static LLVMValueRef build_inverse_sqrt_op(JitContext* ctx, LLVMValueRef x)
{
    LLVMTypeRef type = LLVMTypeOf(x);
    if (LLVMGetTypeKind(type) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(type);
        LLVMValueRef result = LLVMGetUndef(type);
        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem = LLVMBuildExtractValue(ctx->builder, x, i, "elem");
            LLVMValueRef elem_res = build_inverse_sqrt_simt(ctx, elem);
            result = LLVMBuildInsertValue(ctx->builder, result, elem_res, i, "pack_inversesqrt");
        }
        return result;
    }
    return build_inverse_sqrt_simt(ctx, x);
}

void handle_ext_round(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_unary_intrinsic_op(ctx, x, "llvm.round"));
}

void handle_ext_round_even(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_unary_intrinsic_op(ctx, x, "llvm.roundeven"));
}

void handle_ext_trunc(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_unary_intrinsic_op(ctx, x, "llvm.trunc"));
}

void handle_ext_floor(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_unary_intrinsic_op(ctx, x, "llvm.floor"));
}

void handle_ext_ceil(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_unary_intrinsic_op(ctx, x, "llvm.ceil"));
}

void handle_ext_fract(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_fract_op(ctx, x));
}

void handle_ext_fma(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef a = get_val(ctx, operands[0]);
    LLVMValueRef b = get_val(ctx, operands[1]);
    LLVMValueRef c = get_val(ctx, operands[2]);
    set_val(ctx, res_id, build_fma_op(ctx, a, b, c));
}

static void build_modf_simt(JitContext* ctx, LLVMValueRef x_simt, LLVMValueRef* out_fract, LLVMValueRef* out_i)
{
    *out_i = build_unary_simt_intrinsic(ctx, x_simt, "llvm.trunc");
    *out_fract = LLVMBuildFSub(ctx->builder, x_simt, *out_i, "modf_fract");
}

void handle_ext_modf(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef i_ptr = get_val(ctx, operands[1]);
    LLVMTypeRef type_x = LLVMTypeOf(x);

    if (LLVMGetTypeKind(type_x) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(type_x);
        LLVMValueRef fract_result = LLVMGetUndef(type_x);
        LLVMValueRef i_result = LLVMGetUndef(type_x);

        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem_x = LLVMBuildExtractValue(ctx->builder, x, i, "elem_x");
            LLVMValueRef elem_fract = NULL;
            LLVMValueRef elem_i = NULL;
            build_modf_simt(ctx, elem_x, &elem_fract, &elem_i);

            fract_result = LLVMBuildInsertValue(ctx->builder, fract_result, elem_fract, i, "pack_fract");
            i_result = LLVMBuildInsertValue(ctx->builder, i_result, elem_i, i, "pack_i");
        }

        set_val(ctx, res_id, fract_result);
        if (i_ptr && !LLVMIsNull(i_ptr))
        {
            build_recursive_store(ctx, i_result, i_ptr, ctx->emask);
        }
    }
    else
    {
        LLVMValueRef fract = NULL;
        LLVMValueRef i_val = NULL;
        build_modf_simt(ctx, x, &fract, &i_val);

        set_val(ctx, res_id, fract);
        if (i_ptr && !LLVMIsNull(i_ptr))
        {
            build_recursive_store(ctx, i_val, i_ptr, ctx->emask);
        }
    }
}

void handle_ext_modf_struct(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMTypeRef type_x = LLVMTypeOf(x);

    if (LLVMGetTypeKind(type_x) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(type_x);
        LLVMValueRef fract_result = LLVMGetUndef(type_x);
        LLVMValueRef i_result = LLVMGetUndef(type_x);

        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem_x = LLVMBuildExtractValue(ctx->builder, x, i, "elem_x");
            LLVMValueRef elem_fract = NULL;
            LLVMValueRef elem_i = NULL;
            build_modf_simt(ctx, elem_x, &elem_fract, &elem_i);

            fract_result = LLVMBuildInsertValue(ctx->builder, fract_result, elem_fract, i, "pack_fract");
            i_result = LLVMBuildInsertValue(ctx->builder, i_result, elem_i, i, "pack_i");
        }

        LLVMTypeRef struct_members[] = { type_x, type_x };
        LLVMTypeRef struct_type = LLVMStructTypeInContext(ctx->context, struct_members, 2, 0);

        LLVMValueRef res_struct = LLVMGetUndef(struct_type);
        res_struct = LLVMBuildInsertValue(ctx->builder, res_struct, fract_result, 0, "modf_res_fract");
        res_struct = LLVMBuildInsertValue(ctx->builder, res_struct, i_result, 1, "modf_res_i");

        set_val(ctx, res_id, res_struct);
    }
    else
    {
        LLVMValueRef fract = NULL;
        LLVMValueRef i_val = NULL;
        build_modf_simt(ctx, x, &fract, &i_val);

        LLVMTypeRef struct_members[] = { ctx->vec_float_type, ctx->vec_float_type };
        LLVMTypeRef struct_type = LLVMStructTypeInContext(ctx->context, struct_members, 2, 0);

        LLVMValueRef res_struct = LLVMGetUndef(struct_type);
        res_struct = LLVMBuildInsertValue(ctx->builder, res_struct, fract, 0, "modf_res_fract");
        res_struct = LLVMBuildInsertValue(ctx->builder, res_struct, i_val, 1, "modf_res_i");

        set_val(ctx, res_id, res_struct);
    }
}

void handle_ext_radians(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef degrees = get_val(ctx, operands[0]);
    LLVMValueRef factor = const_simt_float(ctx, 0.017453292519943295f);
    LLVMValueRef res = mat_operation_helper(ctx, degrees, factor, LLVMBuildFMul, "radians_mul");
    set_val(ctx, res_id, res);
}

void handle_ext_degrees(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef radians = get_val(ctx, operands[0]);
    LLVMValueRef factor = const_simt_float(ctx, 57.29577951308232f);
    LLVMValueRef res = mat_operation_helper(ctx, radians, factor, LLVMBuildFMul, "degrees_mul");
    set_val(ctx, res_id, res);
}

void handle_ext_asin(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef a = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_unary_intrinsic_op(ctx, a, "llvm.asin"));
}

void handle_ext_acos(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
     LLVMValueRef a = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_unary_intrinsic_op(ctx, a, "llvm.acos"));
}

void handle_ext_atan(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
     LLVMValueRef a = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_unary_intrinsic_op(ctx, a, "llvm.atan"));
}

void handle_ext_sinh(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
     LLVMValueRef a = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_unary_intrinsic_op(ctx, a, "llvm.sinh"));
}

void handle_ext_cosh(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef a = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_unary_intrinsic_op(ctx, a, "llvm.cosh"));
}

void handle_ext_tanh(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef a = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_unary_intrinsic_op(ctx, a, "llvm.tanh"));
}

static LLVMValueRef build_asinh_simt(JitContext* ctx, LLVMValueRef a)
{
    LLVMValueRef a_sq = LLVMBuildFMul(ctx->builder, a, a, "a_sq");
    LLVMValueRef one = const_simt_float(ctx, 1.0f);
    LLVMValueRef a_sq_plus_one = LLVMBuildFAdd(ctx->builder, a_sq, one, "a_sq_plus_one");
    LLVMValueRef sqrt_term = build_unary_simt_intrinsic(ctx, a_sq_plus_one, "llvm.sqrt");
    LLVMValueRef sum = LLVMBuildFAdd(ctx->builder, a, sqrt_term, "sum");
    return build_unary_simt_intrinsic(ctx, sum, "llvm.log");
}

static LLVMValueRef build_acosh_simt(JitContext* ctx, LLVMValueRef a)
{
    LLVMValueRef a_sq = LLVMBuildFMul(ctx->builder, a, a, "a_sq");
    LLVMValueRef one = const_simt_float(ctx, 1.0f);
    LLVMValueRef a_sq_minus_one = LLVMBuildFSub(ctx->builder, a_sq, one, "a_sq_minus_one");
    LLVMValueRef sqrt_term = build_unary_simt_intrinsic(ctx, a_sq_minus_one, "llvm.sqrt");
    LLVMValueRef sum = LLVMBuildFAdd(ctx->builder, a, sqrt_term, "sum");
    return build_unary_simt_intrinsic(ctx, sum, "llvm.log");
}

static LLVMValueRef build_atanh_simt(JitContext* ctx, LLVMValueRef a)
{
    LLVMValueRef one = const_simt_float(ctx, 1.0f);
    LLVMValueRef one_plus_x = LLVMBuildFAdd(ctx->builder, one, a, "one_plus_x");
    LLVMValueRef one_minus_x = LLVMBuildFSub(ctx->builder, one, a, "one_minus_x");
    LLVMValueRef div = LLVMBuildFDiv(ctx->builder, one_plus_x, one_minus_x, "div");
    LLVMValueRef log_result = build_unary_simt_intrinsic(ctx, div, "llvm.log");
    LLVMValueRef half = const_simt_float(ctx, 0.5f);
    return LLVMBuildFMul(ctx->builder, half, log_result, "atanh_res");
}

//asinh = ln(x + sqrt(x^2 + 1))
void handle_ext_asinh(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef a = get_val(ctx, operands[0]);
    LLVMTypeRef type = LLVMTypeOf(a);
    if (LLVMGetTypeKind(type) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(type);
        LLVMValueRef res = LLVMGetUndef(type);
        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem = LLVMBuildExtractValue(ctx->builder, a, i, "elem");
            res = LLVMBuildInsertValue(ctx->builder, res, build_asinh_simt(ctx, elem), i, "pack");
        }
        set_val(ctx, res_id, res);
    }
    else
    {
        set_val(ctx, res_id, build_asinh_simt(ctx, a));
    }
}

// acosh = ln(x + sqrt(x^2 - 1))
void handle_ext_acosh(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef a = get_val(ctx, operands[0]);
    LLVMTypeRef type = LLVMTypeOf(a);
    if (LLVMGetTypeKind(type) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(type);
        LLVMValueRef res = LLVMGetUndef(type);
        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem = LLVMBuildExtractValue(ctx->builder, a, i, "elem");
            res = LLVMBuildInsertValue(ctx->builder, res, build_acosh_simt(ctx, elem), i, "pack");
        }
        set_val(ctx, res_id, res);
    }
    else
    {
        set_val(ctx, res_id, build_acosh_simt(ctx, a));
    }
}

// atanh = 0.5 * ln((1 + x) / (1 - x))
void handle_ext_atanh(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef a = get_val(ctx, operands[0]);
    LLVMTypeRef type = LLVMTypeOf(a);
    if (LLVMGetTypeKind(type) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(type);
        LLVMValueRef res = LLVMGetUndef(type);
        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem = LLVMBuildExtractValue(ctx->builder, a, i, "elem");
            res = LLVMBuildInsertValue(ctx->builder, res, build_atanh_simt(ctx, elem), i, "pack");
        }
        set_val(ctx, res_id, res);
    }
    else
    {
        set_val(ctx, res_id, build_atanh_simt(ctx, a));
    }
}

void handle_ext_exp2(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_unary_intrinsic_op(ctx, x, "llvm.exp2"));
}

void handle_ext_log2(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_unary_intrinsic_op(ctx, x, "llvm.log2"));
}

void handle_ext_inverse_sqrt(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_inverse_sqrt_op(ctx, x));
}

static void build_frexp_simt(JitContext* ctx, LLVMValueRef x_simt, LLVMValueRef* out_sig, LLVMValueRef* out_exp)
{
    unsigned frexp_id = LLVMLookupIntrinsicID("llvm.frexp", 10);
    LLVMTypeRef i32_vec_type = LLVMVectorType(ctx->int_type, SIMT_WIDTH);
    LLVMTypeRef frexp_param_types[] = { ctx->vec_float_type, i32_vec_type };
    LLVMValueRef frexp_func = LLVMGetIntrinsicDeclaration(ctx->module, frexp_id, frexp_param_types, 2);

    LLVMValueRef args[] = { x_simt };
    LLVMValueRef pair = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(frexp_func), frexp_func, args, 1, "frexp_pair");
    *out_sig = LLVMBuildExtractValue(ctx->builder, pair, 0, "frexp_sig");
    *out_exp = LLVMBuildExtractValue(ctx->builder, pair, 1, "frexp_exp");
}

static LLVMValueRef build_ldexp_simt(JitContext* ctx, LLVMValueRef x_simt, LLVMValueRef exp_simt)
{
    unsigned ldexp_id = LLVMLookupIntrinsicID("llvm.ldexp", 10);
    LLVMTypeRef i32_vec_type = LLVMVectorType(ctx->int_type, SIMT_WIDTH);
    LLVMTypeRef ldexp_param_types[] = { ctx->vec_float_type, i32_vec_type };
    LLVMValueRef ldexp_func = LLVMGetIntrinsicDeclaration(ctx->module, ldexp_id, ldexp_param_types, 2);

    LLVMValueRef exp_i32 = jit_to_int_vector(ctx, exp_simt);
    LLVMValueRef args[] = { x_simt, exp_i32 };
    return LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(ldexp_func), ldexp_func, args, 2, "ldexp_res");
}

void handle_ext_frexp(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef exp_ptr = get_val(ctx, operands[1]);
    LLVMTypeRef type_x = LLVMTypeOf(x);

    if (LLVMGetTypeKind(type_x) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(type_x);
        LLVMValueRef sig_result = LLVMGetUndef(type_x);
        LLVMValueRef exp_result = LLVMGetUndef(type_x);

        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem_x = LLVMBuildExtractValue(ctx->builder, x, i, "elem_x");
            LLVMValueRef elem_sig = NULL;
            LLVMValueRef elem_exp = NULL;
            build_frexp_simt(ctx, elem_x, &elem_sig, &elem_exp);
            LLVMValueRef elem_exp_f = jit_to_float_vector(ctx, elem_exp);

            sig_result = LLVMBuildInsertValue(ctx->builder, sig_result, elem_sig, i, "pack_sig");
            exp_result = LLVMBuildInsertValue(ctx->builder, exp_result, elem_exp_f, i, "pack_exp");
        }

        set_val(ctx, res_id, sig_result);
        if (exp_ptr && !LLVMIsNull(exp_ptr))
        {
            build_recursive_store(ctx, exp_result, exp_ptr, ctx->emask);
        }
    }
    else
    {
        LLVMValueRef sig = NULL;
        LLVMValueRef exp = NULL;
        build_frexp_simt(ctx, x, &sig, &exp);
        LLVMValueRef exp_f = jit_to_float_vector(ctx, exp);

        set_val(ctx, res_id, sig);
        if (exp_ptr && !LLVMIsNull(exp_ptr))
        {
            build_recursive_store(ctx, exp_f, exp_ptr, ctx->emask);
        }
    }
}

void handle_ext_frexp_struct(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMTypeRef type_x = LLVMTypeOf(x);

    if (LLVMGetTypeKind(type_x) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(type_x);
        LLVMValueRef sig_result = LLVMGetUndef(type_x);
        LLVMValueRef exp_result = LLVMGetUndef(type_x);

        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem_x = LLVMBuildExtractValue(ctx->builder, x, i, "elem_x");
            LLVMValueRef elem_sig = NULL;
            LLVMValueRef elem_exp = NULL;
            build_frexp_simt(ctx, elem_x, &elem_sig, &elem_exp);
            LLVMValueRef elem_exp_f = jit_to_float_vector(ctx, elem_exp);

            sig_result = LLVMBuildInsertValue(ctx->builder, sig_result, elem_sig, i, "pack_sig");
            exp_result = LLVMBuildInsertValue(ctx->builder, exp_result, elem_exp_f, i, "pack_exp");
        }

        LLVMTypeRef struct_members[] = { type_x, type_x };
        LLVMTypeRef struct_type = LLVMStructTypeInContext(ctx->context, struct_members, 2, 0);

        LLVMValueRef res_struct = LLVMGetUndef(struct_type);
        res_struct = LLVMBuildInsertValue(ctx->builder, res_struct, sig_result, 0, "frexp_res_sig");
        res_struct = LLVMBuildInsertValue(ctx->builder, res_struct, exp_result, 1, "frexp_res_exp");

        set_val(ctx, res_id, res_struct);
    }
    else
    {
        LLVMValueRef sig = NULL;
        LLVMValueRef exp = NULL;
        build_frexp_simt(ctx, x, &sig, &exp);
        LLVMValueRef exp_f = jit_to_float_vector(ctx, exp);

        LLVMTypeRef struct_members[] = { ctx->vec_float_type, ctx->vec_float_type };
        LLVMTypeRef struct_type = LLVMStructTypeInContext(ctx->context, struct_members, 2, 0);

        LLVMValueRef res_struct = LLVMGetUndef(struct_type);
        res_struct = LLVMBuildInsertValue(ctx->builder, res_struct, sig, 0, "frexp_res_sig");
        res_struct = LLVMBuildInsertValue(ctx->builder, res_struct, exp_f, 1, "frexp_res_exp");

        set_val(ctx, res_id, res_struct);
    }
}

void handle_ext_ldexp(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef exp = get_val(ctx, operands[1]);
    LLVMTypeRef type_x = LLVMTypeOf(x);

    if (LLVMGetTypeKind(type_x) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(type_x);
        LLVMValueRef result = LLVMGetUndef(type_x);
        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem_x = LLVMBuildExtractValue(ctx->builder, x, i, "elem_x");
            LLVMValueRef elem_exp = (LLVMGetTypeKind(LLVMTypeOf(exp)) == LLVMArrayTypeKind)
                ? LLVMBuildExtractValue(ctx->builder, exp, i, "elem_exp")
                : exp;
            LLVMValueRef elem_res = build_ldexp_simt(ctx, elem_x, elem_exp);
            result = LLVMBuildInsertValue(ctx->builder, result, elem_res, i, "pack_ldexp");
        }
        set_val(ctx, res_id, result);
    }
    else
    {
        set_val(ctx, res_id, build_ldexp_simt(ctx, x, exp));
    }
}

static inline LLVMValueRef const_simt_int(JitContext* ctx, int32_t val)
{
    LLVMValueRef scalars[SIMT_WIDTH];
    for (int i = 0; i < SIMT_WIDTH; i++) {
        scalars[i] = LLVMConstInt(ctx->int_type, val, 1);
    }
    return LLVMConstVector(scalars, SIMT_WIDTH);
}

static inline LLVMValueRef const_simt_i64(JitContext* ctx, int64_t val)
{
    LLVMTypeRef i64_type = LLVMInt64TypeInContext(ctx->context);
    LLVMValueRef scalars[SIMT_WIDTH];
    for (int i = 0; i < SIMT_WIDTH; i++) {
        scalars[i] = LLVMConstInt(i64_type, val, 1);
    }
    return LLVMConstVector(scalars, SIMT_WIDTH);
}

typedef LLVMValueRef (*UnaryIntSimtFunc)(JitContext* ctx, LLVMValueRef simt_val);

static LLVMValueRef dispatch_unary_int_op(JitContext* ctx, LLVMValueRef val, UnaryIntSimtFunc func)
{
    LLVMTypeRef type = LLVMTypeOf(val);
    if (LLVMGetTypeKind(type) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(type);
        LLVMValueRef result = LLVMGetUndef(type);
        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem = LLVMBuildExtractValue(ctx->builder, val, i, "comp_val");
            LLVMValueRef elem_res = func(ctx, elem);
            result = LLVMBuildInsertValue(ctx->builder, result, elem_res, i, "comp_res");
        }
        return result;
    }
    return func(ctx, val);
}

typedef LLVMValueRef (*BinaryIntSimtFunc)(JitContext* ctx, LLVMValueRef simt_a, LLVMValueRef simt_b);

static LLVMValueRef dispatch_binary_int_op(JitContext* ctx, LLVMValueRef a, LLVMValueRef b, BinaryIntSimtFunc func)
{
    LLVMTypeRef type_a = LLVMTypeOf(a);
    LLVMTypeRef type_b = LLVMTypeOf(b);

    if (LLVMGetTypeKind(type_a) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(type_a);
        LLVMValueRef result = LLVMGetUndef(type_a);
        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem_a = LLVMBuildExtractValue(ctx->builder, a, i, "elem_a");
            LLVMValueRef elem_b = (LLVMGetTypeKind(type_b) == LLVMArrayTypeKind)
                ? LLVMBuildExtractValue(ctx->builder, b, i, "elem_b")
                : b;
            LLVMValueRef elem_res = func(ctx, elem_a, elem_b);
            result = LLVMBuildInsertValue(ctx->builder, result, elem_res, i, "comp_res");
        }
        return result;
    }
    return func(ctx, a, b);
}

typedef LLVMValueRef (*TernaryIntSimtFunc)(JitContext* ctx, LLVMValueRef simt_x, LLVMValueRef simt_min, LLVMValueRef simt_max);

static LLVMValueRef dispatch_ternary_int_op(JitContext* ctx, LLVMValueRef x, LLVMValueRef min_val, LLVMValueRef max_val, TernaryIntSimtFunc func)
{
    LLVMTypeRef type_x = LLVMTypeOf(x);
    if (LLVMGetTypeKind(type_x) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(type_x);
        LLVMValueRef result = LLVMGetUndef(type_x);
        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem_x = LLVMBuildExtractValue(ctx->builder, x, i, "elem_x");
            LLVMValueRef elem_min = (LLVMGetTypeKind(LLVMTypeOf(min_val)) == LLVMArrayTypeKind)
                ? LLVMBuildExtractValue(ctx->builder, min_val, i, "elem_min")
                : min_val;
            LLVMValueRef elem_max = (LLVMGetTypeKind(LLVMTypeOf(max_val)) == LLVMArrayTypeKind)
                ? LLVMBuildExtractValue(ctx->builder, max_val, i, "elem_max")
                : max_val;
            LLVMValueRef elem_res = func(ctx, elem_x, elem_min, elem_max);
            result = LLVMBuildInsertValue(ctx->builder, result, elem_res, i, "comp_res");
        }
        return result;
    }
    return func(ctx, x, min_val, max_val);
}

static LLVMValueRef build_sabs_simt(JitContext* ctx, LLVMValueRef simt_x)
{
    LLVMValueRef x_int = jit_to_int_vector(ctx, simt_x);
    LLVMValueRef zero = const_simt_int(ctx, 0);
    LLVMValueRef neg = LLVMBuildNeg(ctx->builder, x_int, "sabs_neg");
    LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntSLT, x_int, zero, "sabs_lt0");
    LLVMValueRef res = LLVMBuildSelect(ctx->builder, cond, neg, x_int, "sabs_res");
    return jit_to_float_vector(ctx, res);
}

void handle_ext_sabs(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    set_val(ctx, res_id, dispatch_unary_int_op(ctx, x, build_sabs_simt));
}

static LLVMValueRef build_ssign_simt(JitContext* ctx, LLVMValueRef simt_x)
{
    LLVMValueRef x_int = jit_to_int_vector(ctx, simt_x);
    LLVMValueRef zero = const_simt_int(ctx, 0);
    LLVMValueRef one = const_simt_int(ctx, 1);
    LLVMValueRef neg_one = const_simt_int(ctx, -1);

    LLVMValueRef lt0 = LLVMBuildICmp(ctx->builder, LLVMIntSLT, x_int, zero, "ssign_lt0");
    LLVMValueRef gt0 = LLVMBuildICmp(ctx->builder, LLVMIntSGT, x_int, zero, "ssign_gt0");

    LLVMValueRef res = LLVMBuildSelect(ctx->builder, gt0, one, zero, "ssign_tmp");
    res = LLVMBuildSelect(ctx->builder, lt0, neg_one, res, "ssign_res");
    return jit_to_float_vector(ctx, res);
}

void handle_ext_ssign(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    set_val(ctx, res_id, dispatch_unary_int_op(ctx, x, build_ssign_simt));
}

static LLVMValueRef build_umin_simt(JitContext* ctx, LLVMValueRef simt_a, LLVMValueRef simt_b)
{
    LLVMValueRef a_int = jit_to_int_vector(ctx, simt_a);
    LLVMValueRef b_int = jit_to_int_vector(ctx, simt_b);
    LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntULT, a_int, b_int, "umin_cmp");
    LLVMValueRef res = LLVMBuildSelect(ctx->builder, cond, a_int, b_int, "umin_res");
    return jit_to_float_vector(ctx, res);
}

void handle_ext_umin(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef a = get_val(ctx, operands[0]);
    LLVMValueRef b = get_val(ctx, operands[1]);
    set_val(ctx, res_id, dispatch_binary_int_op(ctx, a, b, build_umin_simt));
}

static LLVMValueRef build_smin_simt(JitContext* ctx, LLVMValueRef simt_a, LLVMValueRef simt_b)
{
    LLVMValueRef a_int = jit_to_int_vector(ctx, simt_a);
    LLVMValueRef b_int = jit_to_int_vector(ctx, simt_b);
    LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntSLT, a_int, b_int, "smin_cmp");
    LLVMValueRef res = LLVMBuildSelect(ctx->builder, cond, a_int, b_int, "smin_res");
    return jit_to_float_vector(ctx, res);
}

void handle_ext_smin(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef a = get_val(ctx, operands[0]);
    LLVMValueRef b = get_val(ctx, operands[1]);
    set_val(ctx, res_id, dispatch_binary_int_op(ctx, a, b, build_smin_simt));
}

static LLVMValueRef build_umax_simt(JitContext* ctx, LLVMValueRef simt_a, LLVMValueRef simt_b)
{
    LLVMValueRef a_int = jit_to_int_vector(ctx, simt_a);
    LLVMValueRef b_int = jit_to_int_vector(ctx, simt_b);
    LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntUGT, a_int, b_int, "umax_cmp");
    LLVMValueRef res = LLVMBuildSelect(ctx->builder, cond, a_int, b_int, "umax_res");
    return jit_to_float_vector(ctx, res);
}

void handle_ext_umax(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef a = get_val(ctx, operands[0]);
    LLVMValueRef b = get_val(ctx, operands[1]);
    set_val(ctx, res_id, dispatch_binary_int_op(ctx, a, b, build_umax_simt));
}

static LLVMValueRef build_smax_simt(JitContext* ctx, LLVMValueRef simt_a, LLVMValueRef simt_b)
{
    LLVMValueRef a_int = jit_to_int_vector(ctx, simt_a);
    LLVMValueRef b_int = jit_to_int_vector(ctx, simt_b);
    LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntSGT, a_int, b_int, "smax_cmp");
    LLVMValueRef res = LLVMBuildSelect(ctx->builder, cond, a_int, b_int, "smax_res");
    return jit_to_float_vector(ctx, res);
}

void handle_ext_smax(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef a = get_val(ctx, operands[0]);
    LLVMValueRef b = get_val(ctx, operands[1]);
    set_val(ctx, res_id, dispatch_binary_int_op(ctx, a, b, build_smax_simt));
}

static LLVMValueRef build_uclamp_simt(JitContext* ctx, LLVMValueRef simt_x, LLVMValueRef simt_min, LLVMValueRef simt_max)
{
    LLVMValueRef x_int = jit_to_int_vector(ctx, simt_x);
    LLVMValueRef min_int = jit_to_int_vector(ctx, simt_min);
    LLVMValueRef max_int = jit_to_int_vector(ctx, simt_max);

    LLVMValueRef cmp_gt = LLVMBuildICmp(ctx->builder, LLVMIntUGT, x_int, min_int, "uclamp_gt");
    LLVMValueRef clamped_min = LLVMBuildSelect(ctx->builder, cmp_gt, x_int, min_int, "uclamp_min");
    LLVMValueRef cmp_lt = LLVMBuildICmp(ctx->builder, LLVMIntULT, clamped_min, max_int, "uclamp_lt");
    LLVMValueRef res = LLVMBuildSelect(ctx->builder, cmp_lt, clamped_min, max_int, "uclamp_res");
    return jit_to_float_vector(ctx, res);
}

void handle_ext_uclamp(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef min_val = get_val(ctx, operands[1]);
    LLVMValueRef max_val = get_val(ctx, operands[2]);
    set_val(ctx, res_id, dispatch_ternary_int_op(ctx, x, min_val, max_val, build_uclamp_simt));
}

static LLVMValueRef build_sclamp_simt(JitContext* ctx, LLVMValueRef simt_x, LLVMValueRef simt_min, LLVMValueRef simt_max)
{
    LLVMValueRef x_int = jit_to_int_vector(ctx, simt_x);
    LLVMValueRef min_int = jit_to_int_vector(ctx, simt_min);
    LLVMValueRef max_int = jit_to_int_vector(ctx, simt_max);

    LLVMValueRef cmp_gt = LLVMBuildICmp(ctx->builder, LLVMIntSGT, x_int, min_int, "sclamp_gt");
    LLVMValueRef clamped_min = LLVMBuildSelect(ctx->builder, cmp_gt, x_int, min_int, "sclamp_min");
    LLVMValueRef cmp_lt = LLVMBuildICmp(ctx->builder, LLVMIntSLT, clamped_min, max_int, "sclamp_lt");
    LLVMValueRef res = LLVMBuildSelect(ctx->builder, cmp_lt, clamped_min, max_int, "sclamp_res");
    return jit_to_float_vector(ctx, res);
}

void handle_ext_sclamp(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef min_val = get_val(ctx, operands[1]);
    LLVMValueRef max_val = get_val(ctx, operands[2]);
    set_val(ctx, res_id, dispatch_ternary_int_op(ctx, x, min_val, max_val, build_sclamp_simt));
}

static LLVMValueRef ensure_bool_simt(JitContext* ctx, LLVMValueRef cond)
{
    LLVMTypeRef type = LLVMTypeOf(cond);
    if (LLVMGetTypeKind(type) == LLVMVectorTypeKind)
    {
        LLVMTypeRef elem_type = LLVMGetElementType(type);
        if (LLVMGetTypeKind(elem_type) == LLVMIntegerTypeKind && LLVMGetIntTypeWidth(elem_type) == 1)
        {
            return cond;
        }
        if (LLVMGetTypeKind(elem_type) == LLVMIntegerTypeKind)
        {
            LLVMValueRef zero = LLVMConstNull(type);
            return LLVMBuildICmp(ctx->builder, LLVMIntNE, cond, zero, "to_i1");
        }
        if (LLVMGetTypeKind(elem_type) == LLVMFloatTypeKind)
        {
            LLVMValueRef cond_int = jit_to_int_vector(ctx, cond);
            LLVMValueRef zero = LLVMConstNull(LLVMTypeOf(cond_int));
            return LLVMBuildICmp(ctx->builder, LLVMIntNE, cond_int, zero, "to_i1");
        }
    }
    return cond;
}

void handle_ext_imix(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef y = get_val(ctx, operands[1]);
    LLVMValueRef a = get_val(ctx, operands[2]);
    LLVMTypeRef type_x = LLVMTypeOf(x);

    if (LLVMGetTypeKind(type_x) == LLVMArrayTypeKind)
    {
        uint32_t count = LLVMGetArrayLength(type_x);
        LLVMValueRef result = LLVMGetUndef(type_x);
        for (uint32_t i = 0; i < count; i++)
        {
            LLVMValueRef elem_x = LLVMBuildExtractValue(ctx->builder, x, i, "elem_x");
            LLVMValueRef elem_y = (LLVMGetTypeKind(LLVMTypeOf(y)) == LLVMArrayTypeKind)
                ? LLVMBuildExtractValue(ctx->builder, y, i, "elem_y")
                : y;
            LLVMValueRef elem_a = (LLVMGetTypeKind(LLVMTypeOf(a)) == LLVMArrayTypeKind)
                ? LLVMBuildExtractValue(ctx->builder, a, i, "elem_a")
                : a;
            LLVMValueRef cond = ensure_bool_simt(ctx, elem_a);
            LLVMValueRef elem_res = LLVMBuildSelect(ctx->builder, cond, elem_y, elem_x, "imix_elem");
            result = LLVMBuildInsertValue(ctx->builder, result, elem_res, i, "comp_res");
        }
        set_val(ctx, res_id, result);
    }
    else
    {
        LLVMValueRef cond = ensure_bool_simt(ctx, a);
        set_val(ctx, res_id, LLVMBuildSelect(ctx->builder, cond, y, x, "imix_res"));
    }
}

void handle_ext_nmin(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef y = get_val(ctx, operands[1]);
    set_val(ctx, res_id, build_binary_intrinsic_op(ctx, x, y, "llvm.minnum"));
}

void handle_ext_nmax(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef y = get_val(ctx, operands[1]);
    set_val(ctx, res_id, build_binary_intrinsic_op(ctx, x, y, "llvm.maxnum"));
}

void handle_ext_nclamp(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef val = get_val(ctx, operands[0]);
    LLVMValueRef min_val = get_val(ctx, operands[1]);
    LLVMValueRef max_val = get_val(ctx, operands[2]);
    set_val(ctx, res_id, build_clamp_op(ctx, val, min_val, max_val));
}

static inline LLVMValueRef m_elem(JitContext* ctx, LLVMValueRef matrix, uint32_t c, uint32_t r)
{
    LLVMValueRef col = LLVMBuildExtractValue(ctx->builder, matrix, c, "col");
    return LLVMBuildExtractValue(ctx->builder, col, r, "elem");
}

static LLVMValueRef build_matrix_det(JitContext* ctx, LLVMValueRef mat)
{
    LLVMTypeRef mat_type = LLVMTypeOf(mat);
    uint32_t n = LLVMGetArrayLength(mat_type);

    if (n == 2)
    {
        LLVMValueRef p0 = LLVMBuildFMul(ctx->builder, m_elem(ctx, mat, 0, 0), m_elem(ctx, mat, 1, 1), "p0");
        LLVMValueRef p1 = LLVMBuildFMul(ctx->builder, m_elem(ctx, mat, 1, 0), m_elem(ctx, mat, 0, 1), "p1");
        return LLVMBuildFSub(ctx->builder, p0, p1, "det2");
    }
    else if (n == 3)
    {
        LLVMValueRef m00 = m_elem(ctx, mat, 0, 0);
        LLVMValueRef m01 = m_elem(ctx, mat, 0, 1);
        LLVMValueRef m02 = m_elem(ctx, mat, 0, 2);
        LLVMValueRef m10 = m_elem(ctx, mat, 1, 0);
        LLVMValueRef m11 = m_elem(ctx, mat, 1, 1);
        LLVMValueRef m12 = m_elem(ctx, mat, 1, 2);
        LLVMValueRef m20 = m_elem(ctx, mat, 2, 0);
        LLVMValueRef m21 = m_elem(ctx, mat, 2, 1);
        LLVMValueRef m22 = m_elem(ctx, mat, 2, 2);

        LLVMValueRef c00 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, m11, m22, ""), LLVMBuildFMul(ctx->builder, m21, m12, ""), "c00");
        LLVMValueRef c10 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, m20, m12, ""), LLVMBuildFMul(ctx->builder, m10, m22, ""), "c10");
        LLVMValueRef c20 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, m10, m21, ""), LLVMBuildFMul(ctx->builder, m20, m11, ""), "c20");

        LLVMValueRef t0 = LLVMBuildFMul(ctx->builder, m00, c00, "t0");
        LLVMValueRef t1 = LLVMBuildFMul(ctx->builder, m01, c10, "t1");
        LLVMValueRef t2 = LLVMBuildFMul(ctx->builder, m02, c20, "t2");
        return LLVMBuildFAdd(ctx->builder, LLVMBuildFAdd(ctx->builder, t0, t1, ""), t2, "det3");
    }
    else if (n == 4)
    {
        LLVMValueRef M[4][4];
        for (uint32_t c = 0; c < 4; c++)
            for (uint32_t r = 0; r < 4; r++)
                M[c][r] = m_elem(ctx, mat, c, r);

        LLVMValueRef s0 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[0][0], M[1][1], ""), LLVMBuildFMul(ctx->builder, M[1][0], M[0][1], ""), "s0");
        LLVMValueRef s1 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[0][0], M[1][2], ""), LLVMBuildFMul(ctx->builder, M[1][0], M[0][2], ""), "s1");
        LLVMValueRef s2 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[0][0], M[1][3], ""), LLVMBuildFMul(ctx->builder, M[1][0], M[0][3], ""), "s2");
        LLVMValueRef s3 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[0][1], M[1][2], ""), LLVMBuildFMul(ctx->builder, M[1][1], M[0][2], ""), "s3");
        LLVMValueRef s4 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[0][1], M[1][3], ""), LLVMBuildFMul(ctx->builder, M[1][1], M[0][3], ""), "s4");
        LLVMValueRef s5 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[0][2], M[1][3], ""), LLVMBuildFMul(ctx->builder, M[1][2], M[0][3], ""), "s5");

        LLVMValueRef c5 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[2][2], M[3][3], ""), LLVMBuildFMul(ctx->builder, M[3][2], M[2][3], ""), "c5");
        LLVMValueRef c4 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[2][1], M[3][3], ""), LLVMBuildFMul(ctx->builder, M[3][1], M[2][3], ""), "c4");
        LLVMValueRef c3 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[2][1], M[3][2], ""), LLVMBuildFMul(ctx->builder, M[3][1], M[2][2], ""), "c3");
        LLVMValueRef c2 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[2][0], M[3][3], ""), LLVMBuildFMul(ctx->builder, M[3][0], M[2][3], ""), "c2");
        LLVMValueRef c1 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[2][0], M[3][2], ""), LLVMBuildFMul(ctx->builder, M[3][0], M[2][2], ""), "c1");
        LLVMValueRef c0 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[2][0], M[3][1], ""), LLVMBuildFMul(ctx->builder, M[3][0], M[2][1], ""), "c0");

        LLVMValueRef t0 = LLVMBuildFMul(ctx->builder, s0, c5, "t0");
        LLVMValueRef t1 = LLVMBuildFMul(ctx->builder, s1, c4, "t1");
        LLVMValueRef t2 = LLVMBuildFMul(ctx->builder, s2, c3, "t2");
        LLVMValueRef t3 = LLVMBuildFMul(ctx->builder, s3, c2, "t3");
        LLVMValueRef t4 = LLVMBuildFMul(ctx->builder, s4, c1, "t4");
        LLVMValueRef t5 = LLVMBuildFMul(ctx->builder, s5, c0, "t5");

        LLVMValueRef det = LLVMBuildFSub(ctx->builder, t0, t1, "");
        det = LLVMBuildFAdd(ctx->builder, det, t2, "");
        det = LLVMBuildFAdd(ctx->builder, det, t3, "");
        det = LLVMBuildFSub(ctx->builder, det, t4, "");
        det = LLVMBuildFAdd(ctx->builder, det, t5, "det4");
        return det;
    }
    return const_simt_float(ctx, 1.0f);
}

void handle_ext_determinant(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef mat = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_matrix_det(ctx, mat));
}

void handle_ext_matrix_inverse(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef mat = get_val(ctx, operands[0]);
    LLVMTypeRef mat_type = LLVMTypeOf(mat);
    uint32_t n = LLVMGetArrayLength(mat_type);

    LLVMValueRef det = build_matrix_det(ctx, mat);
    LLVMValueRef one_vec = const_simt_float(ctx, 1.0f);
    LLVMValueRef invDet = LLVMBuildFDiv(ctx->builder, one_vec, det, "invDet");

    LLVMValueRef inv[4][4];

    if (n == 2)
    {
        LLVMValueRef m00 = m_elem(ctx, mat, 0, 0);
        LLVMValueRef m01 = m_elem(ctx, mat, 0, 1);
        LLVMValueRef m10 = m_elem(ctx, mat, 1, 0);
        LLVMValueRef m11 = m_elem(ctx, mat, 1, 1);

        inv[0][0] = LLVMBuildFMul(ctx->builder, m11, invDet, "i00");
        inv[0][1] = LLVMBuildFMul(ctx->builder, LLVMBuildFNeg(ctx->builder, m01, ""), invDet, "i01");
        inv[1][0] = LLVMBuildFMul(ctx->builder, LLVMBuildFNeg(ctx->builder, m10, ""), invDet, "i10");
        inv[1][1] = LLVMBuildFMul(ctx->builder, m00, invDet, "i11");
    }
    else if (n == 3)
    {
        LLVMValueRef m00 = m_elem(ctx, mat, 0, 0);
        LLVMValueRef m01 = m_elem(ctx, mat, 0, 1);
        LLVMValueRef m02 = m_elem(ctx, mat, 0, 2);
        LLVMValueRef m10 = m_elem(ctx, mat, 1, 0);
        LLVMValueRef m11 = m_elem(ctx, mat, 1, 1);
        LLVMValueRef m12 = m_elem(ctx, mat, 1, 2);
        LLVMValueRef m20 = m_elem(ctx, mat, 2, 0);
        LLVMValueRef m21 = m_elem(ctx, mat, 2, 1);
        LLVMValueRef m22 = m_elem(ctx, mat, 2, 2);

        LLVMValueRef c00 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, m11, m22, ""), LLVMBuildFMul(ctx->builder, m21, m12, ""), "c00");
        LLVMValueRef c10 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, m20, m12, ""), LLVMBuildFMul(ctx->builder, m10, m22, ""), "c10");
        LLVMValueRef c20 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, m10, m21, ""), LLVMBuildFMul(ctx->builder, m20, m11, ""), "c20");

        LLVMValueRef c01 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, m02, m21, ""), LLVMBuildFMul(ctx->builder, m01, m22, ""), "c01");
        LLVMValueRef c11 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, m00, m22, ""), LLVMBuildFMul(ctx->builder, m02, m20, ""), "c11");
        LLVMValueRef c21 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, m01, m20, ""), LLVMBuildFMul(ctx->builder, m00, m21, ""), "c21");

        LLVMValueRef c02 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, m01, m12, ""), LLVMBuildFMul(ctx->builder, m02, m11, ""), "c02");
        LLVMValueRef c12 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, m02, m10, ""), LLVMBuildFMul(ctx->builder, m00, m12, ""), "c12");
        LLVMValueRef c22 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, m00, m11, ""), LLVMBuildFMul(ctx->builder, m01, m10, ""), "c22");

        inv[0][0] = LLVMBuildFMul(ctx->builder, c00, invDet, "");
        inv[0][1] = LLVMBuildFMul(ctx->builder, c10, invDet, "");
        inv[0][2] = LLVMBuildFMul(ctx->builder, c20, invDet, "");

        inv[1][0] = LLVMBuildFMul(ctx->builder, c01, invDet, "");
        inv[1][1] = LLVMBuildFMul(ctx->builder, c11, invDet, "");
        inv[1][2] = LLVMBuildFMul(ctx->builder, c21, invDet, "");

        inv[2][0] = LLVMBuildFMul(ctx->builder, c02, invDet, "");
        inv[2][1] = LLVMBuildFMul(ctx->builder, c12, invDet, "");
        inv[2][2] = LLVMBuildFMul(ctx->builder, c22, invDet, "");
    }
    else if (n == 4)
    {
        LLVMValueRef M[4][4];
        for (uint32_t c = 0; c < 4; c++)
            for (uint32_t r = 0; r < 4; r++)
                M[c][r] = m_elem(ctx, mat, c, r);

        LLVMValueRef s0 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[0][0], M[1][1], ""), LLVMBuildFMul(ctx->builder, M[1][0], M[0][1], ""), "s0");
        LLVMValueRef s1 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[0][0], M[1][2], ""), LLVMBuildFMul(ctx->builder, M[1][0], M[0][2], ""), "s1");
        LLVMValueRef s2 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[0][0], M[1][3], ""), LLVMBuildFMul(ctx->builder, M[1][0], M[0][3], ""), "s2");
        LLVMValueRef s3 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[0][1], M[1][2], ""), LLVMBuildFMul(ctx->builder, M[1][1], M[0][2], ""), "s3");
        LLVMValueRef s4 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[0][1], M[1][3], ""), LLVMBuildFMul(ctx->builder, M[1][1], M[0][3], ""), "s4");
        LLVMValueRef s5 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[0][2], M[1][3], ""), LLVMBuildFMul(ctx->builder, M[1][2], M[0][3], ""), "s5");

        LLVMValueRef c5 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[2][2], M[3][3], ""), LLVMBuildFMul(ctx->builder, M[3][2], M[2][3], ""), "c5");
        LLVMValueRef c4 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[2][1], M[3][3], ""), LLVMBuildFMul(ctx->builder, M[3][1], M[2][3], ""), "c4");
        LLVMValueRef c3 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[2][1], M[3][2], ""), LLVMBuildFMul(ctx->builder, M[3][1], M[2][2], ""), "c3");
        LLVMValueRef c2 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[2][0], M[3][3], ""), LLVMBuildFMul(ctx->builder, M[3][0], M[2][3], ""), "c2");
        LLVMValueRef c1 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[2][0], M[3][2], ""), LLVMBuildFMul(ctx->builder, M[3][0], M[2][2], ""), "c1");
        LLVMValueRef c0 = LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, M[2][0], M[3][1], ""), LLVMBuildFMul(ctx->builder, M[3][0], M[2][1], ""), "c0");

        #define TERM3(a, b, c, d, e, f, sign1, sign2) \
            LLVMBuildFAdd(ctx->builder, \
                sign1 ? LLVMBuildFSub(ctx->builder, LLVMBuildFMul(ctx->builder, a, b, ""), LLVMBuildFMul(ctx->builder, c, d, ""), "") \
                      : LLVMBuildFAdd(ctx->builder, LLVMBuildFMul(ctx->builder, a, b, ""), LLVMBuildFMul(ctx->builder, c, d, ""), ""), \
                sign2 ? LLVMBuildFNeg(ctx->builder, LLVMBuildFMul(ctx->builder, e, f, ""), "") \
                      : LLVMBuildFMul(ctx->builder, e, f, ""), "")

        inv[0][0] = LLVMBuildFMul(ctx->builder, TERM3(M[1][1], c5, M[1][2], c4, M[1][3], c3, 1, 0), invDet, "");
        inv[0][1] = LLVMBuildFMul(ctx->builder, TERM3(LLVMBuildFNeg(ctx->builder, M[0][1], ""), c5, M[0][2], c4, M[0][3], c3, 0, 1), invDet, "");
        inv[0][2] = LLVMBuildFMul(ctx->builder, TERM3(M[3][1], s5, M[3][2], s4, M[3][3], s3, 1, 0), invDet, "");
        inv[0][3] = LLVMBuildFMul(ctx->builder, TERM3(LLVMBuildFNeg(ctx->builder, M[2][1], ""), s5, M[2][2], s4, M[2][3], s3, 0, 1), invDet, "");

        inv[1][0] = LLVMBuildFMul(ctx->builder, TERM3(LLVMBuildFNeg(ctx->builder, M[1][0], ""), c5, M[1][2], c2, M[1][3], c1, 0, 1), invDet, "");
        inv[1][1] = LLVMBuildFMul(ctx->builder, TERM3(M[0][0], c5, M[0][2], c2, M[0][3], c1, 1, 0), invDet, "");
        inv[1][2] = LLVMBuildFMul(ctx->builder, TERM3(LLVMBuildFNeg(ctx->builder, M[3][0], ""), s5, M[3][2], s2, M[3][3], s1, 0, 1), invDet, "");
        inv[1][3] = LLVMBuildFMul(ctx->builder, TERM3(M[2][0], s5, M[2][2], s2, M[2][3], s1, 1, 0), invDet, "");

        inv[2][0] = LLVMBuildFMul(ctx->builder, TERM3(M[1][0], c4, M[1][1], c2, M[1][3], c0, 1, 0), invDet, "");
        inv[2][1] = LLVMBuildFMul(ctx->builder, TERM3(LLVMBuildFNeg(ctx->builder, M[0][0], ""), c4, M[0][1], c2, M[0][3], c0, 0, 1), invDet, "");
        inv[2][2] = LLVMBuildFMul(ctx->builder, TERM3(M[3][0], s4, M[3][1], s2, M[3][3], s0, 1, 0), invDet, "");
        inv[2][3] = LLVMBuildFMul(ctx->builder, TERM3(LLVMBuildFNeg(ctx->builder, M[2][0], ""), s4, M[2][1], s2, M[2][3], s0, 0, 1), invDet, "");

        inv[3][0] = LLVMBuildFMul(ctx->builder, TERM3(LLVMBuildFNeg(ctx->builder, M[1][0], ""), c3, M[1][1], c1, M[1][2], c0, 0, 1), invDet, "");
        inv[3][1] = LLVMBuildFMul(ctx->builder, TERM3(M[0][0], c3, M[0][1], c1, M[0][2], c0, 1, 0), invDet, "");
        inv[3][2] = LLVMBuildFMul(ctx->builder, TERM3(LLVMBuildFNeg(ctx->builder, M[3][0], ""), s3, M[3][1], s1, M[3][2], s0, 0, 1), invDet, "");
        inv[3][3] = LLVMBuildFMul(ctx->builder, TERM3(M[2][0], s3, M[2][1], s1, M[2][2], s0, 1, 0), invDet, "");

        #undef TERM3
    }

    LLVMTypeRef col_type = LLVMGetElementType(mat_type);
    LLVMValueRef res_mat = LLVMGetUndef(mat_type);
    for (uint32_t c = 0; c < n; c++)
    {
        LLVMValueRef col = LLVMGetUndef(col_type);
        for (uint32_t r = 0; r < n; r++)
        {
            col = LLVMBuildInsertValue(ctx->builder, col, inv[c][r], r, "col_elem");
        }
        res_mat = LLVMBuildInsertValue(ctx->builder, res_mat, col, c, "mat_col");
    }
    set_val(ctx, res_id, res_mat);
}

void handle_ext_face_forward(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef N = get_val(ctx, operands[0]);
    LLVMValueRef I = get_val(ctx, operands[1]);
    LLVMValueRef Nref = get_val(ctx, operands[2]);
    LLVMTypeRef vec_type = LLVMTypeOf(N);

    LLVMValueRef dot = (LLVMGetTypeKind(vec_type) == LLVMArrayTypeKind)
        ? calculate_dot_product(ctx, Nref, I)
        : LLVMBuildFMul(ctx->builder, Nref, I, "dot");

    LLVMValueRef zero = const_simt_float(ctx, 0.0f);
    LLVMValueRef cond = LLVMBuildFCmp(ctx->builder, LLVMRealOLT, dot, zero, "ff_cond");

    if (LLVMGetTypeKind(vec_type) == LLVMArrayTypeKind)
    {
        uint32_t num_elements = LLVMGetArrayLength(vec_type);
        LLVMValueRef result_vec = LLVMGetUndef(vec_type);
        for (uint32_t i = 0; i < num_elements; i++)
        {
            LLVMValueRef n_elem = LLVMBuildExtractValue(ctx->builder, N, i, "n_elem");
            LLVMValueRef neg_n = LLVMBuildFNeg(ctx->builder, n_elem, "neg_n");
            LLVMValueRef res_elem = LLVMBuildSelect(ctx->builder, cond, n_elem, neg_n, "ff_elem");
            result_vec = LLVMBuildInsertValue(ctx->builder, result_vec, res_elem, i, "res_vec");
        }
        set_val(ctx, res_id, result_vec);
    }
    else
    {
        LLVMValueRef neg_n = LLVMBuildFNeg(ctx->builder, N, "neg_n");
        set_val(ctx, res_id, LLVMBuildSelect(ctx->builder, cond, N, neg_n, "ff_res"));
    }
}

void handle_ext_pack_snorm4x8(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef v = get_val(ctx, operands[0]);
    LLVMValueRef neg_one_f = const_simt_float(ctx, -1.0f);
    LLVMValueRef one_f = const_simt_float(ctx, 1.0f);
    LLVMValueRef scale_f = const_simt_float(ctx, 127.0f);
    LLVMTypeRef v16i32 = LLVMVectorType(ctx->int_type, SIMT_WIDTH);

    LLVMValueRef packed = const_simt_int(ctx, 0);

    for (uint32_t i = 0; i < 4; i++)
    {
        LLVMValueRef comp = LLVMBuildExtractValue(ctx->builder, v, i, "pack_c");
        LLVMValueRef clamped = build_clamp_simt(ctx, comp, neg_one_f, one_f);
        LLVMValueRef scaled = LLVMBuildFMul(ctx->builder, clamped, scale_f, "scaled");
        LLVMValueRef rounded = build_unary_simt_intrinsic(ctx, scaled, "llvm.round");
        LLVMValueRef comp_int = LLVMBuildFPToSI(ctx->builder, rounded, v16i32, "to_s32");
        LLVMValueRef mask = const_simt_int(ctx, 0xFF);
        comp_int = LLVMBuildAnd(ctx->builder, comp_int, mask, "masked");
        if (i > 0)
        {
            LLVMValueRef sh = const_simt_int(ctx, i * 8);
            comp_int = LLVMBuildShl(ctx->builder, comp_int, sh, "shl");
        }
        packed = LLVMBuildOr(ctx->builder, packed, comp_int, "or");
    }
    set_val(ctx, res_id, jit_to_float_vector(ctx, packed));
}

void handle_ext_pack_unorm4x8(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef v = get_val(ctx, operands[0]);
    LLVMValueRef zero_f = const_simt_float(ctx, 0.0f);
    LLVMValueRef one_f = const_simt_float(ctx, 1.0f);
    LLVMValueRef scale_f = const_simt_float(ctx, 255.0f);
    LLVMTypeRef v16i32 = LLVMVectorType(ctx->int_type, SIMT_WIDTH);

    LLVMValueRef packed = const_simt_int(ctx, 0);

    for (uint32_t i = 0; i < 4; i++)
    {
        LLVMValueRef comp = LLVMBuildExtractValue(ctx->builder, v, i, "pack_c");
        LLVMValueRef clamped = build_clamp_simt(ctx, comp, zero_f, one_f);
        LLVMValueRef scaled = LLVMBuildFMul(ctx->builder, clamped, scale_f, "scaled");
        LLVMValueRef rounded = build_unary_simt_intrinsic(ctx, scaled, "llvm.round");
        LLVMValueRef comp_int = LLVMBuildFPToUI(ctx->builder, rounded, v16i32, "to_u32");
        LLVMValueRef mask = const_simt_int(ctx, 0xFF);
        comp_int = LLVMBuildAnd(ctx->builder, comp_int, mask, "masked");
        if (i > 0)
        {
            LLVMValueRef sh = const_simt_int(ctx, i * 8);
            comp_int = LLVMBuildShl(ctx->builder, comp_int, sh, "shl");
        }
        packed = LLVMBuildOr(ctx->builder, packed, comp_int, "or");
    }
    set_val(ctx, res_id, jit_to_float_vector(ctx, packed));
}

void handle_ext_pack_snorm2x16(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef v = get_val(ctx, operands[0]);
    LLVMValueRef neg_one_f = const_simt_float(ctx, -1.0f);
    LLVMValueRef one_f = const_simt_float(ctx, 1.0f);
    LLVMValueRef scale_f = const_simt_float(ctx, 32767.0f);
    LLVMTypeRef v16i32 = LLVMVectorType(ctx->int_type, SIMT_WIDTH);

    LLVMValueRef c0 = LLVMBuildExtractValue(ctx->builder, v, 0, "c0");
    LLVMValueRef c1 = LLVMBuildExtractValue(ctx->builder, v, 1, "c1");

    LLVMValueRef s0 = LLVMBuildFPToSI(ctx->builder, build_unary_simt_intrinsic(ctx, LLVMBuildFMul(ctx->builder, build_clamp_simt(ctx, c0, neg_one_f, one_f), scale_f, ""), "llvm.round"), v16i32, "s0");
    LLVMValueRef s1 = LLVMBuildFPToSI(ctx->builder, build_unary_simt_intrinsic(ctx, LLVMBuildFMul(ctx->builder, build_clamp_simt(ctx, c1, neg_one_f, one_f), scale_f, ""), "llvm.round"), v16i32, "s1");

    LLVMValueRef mask = const_simt_int(ctx, 0xFFFF);
    s0 = LLVMBuildAnd(ctx->builder, s0, mask, "s0_m");
    s1 = LLVMBuildAnd(ctx->builder, s1, mask, "s1_m");
    s1 = LLVMBuildShl(ctx->builder, s1, const_simt_int(ctx, 16), "s1_shl");

    LLVMValueRef packed = LLVMBuildOr(ctx->builder, s0, s1, "packed_snorm2");
    set_val(ctx, res_id, jit_to_float_vector(ctx, packed));
}

void handle_ext_pack_unorm2x16(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef v = get_val(ctx, operands[0]);
    LLVMValueRef zero_f = const_simt_float(ctx, 0.0f);
    LLVMValueRef one_f = const_simt_float(ctx, 1.0f);
    LLVMValueRef scale_f = const_simt_float(ctx, 65535.0f);
    LLVMTypeRef v16i32 = LLVMVectorType(ctx->int_type, SIMT_WIDTH);

    LLVMValueRef c0 = LLVMBuildExtractValue(ctx->builder, v, 0, "c0");
    LLVMValueRef c1 = LLVMBuildExtractValue(ctx->builder, v, 1, "c1");

    LLVMValueRef u0 = LLVMBuildFPToUI(ctx->builder, build_unary_simt_intrinsic(ctx, LLVMBuildFMul(ctx->builder, build_clamp_simt(ctx, c0, zero_f, one_f), scale_f, ""), "llvm.round"), v16i32, "u0");
    LLVMValueRef u1 = LLVMBuildFPToUI(ctx->builder, build_unary_simt_intrinsic(ctx, LLVMBuildFMul(ctx->builder, build_clamp_simt(ctx, c1, zero_f, one_f), scale_f, ""), "llvm.round"), v16i32, "u1");

    LLVMValueRef mask = const_simt_int(ctx, 0xFFFF);
    u0 = LLVMBuildAnd(ctx->builder, u0, mask, "u0_m");
    u1 = LLVMBuildAnd(ctx->builder, u1, mask, "u1_m");
    u1 = LLVMBuildShl(ctx->builder, u1, const_simt_int(ctx, 16), "u1_shl");

    LLVMValueRef packed = LLVMBuildOr(ctx->builder, u0, u1, "packed_unorm2");
    set_val(ctx, res_id, jit_to_float_vector(ctx, packed));
}

void handle_ext_pack_half2x16(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef v = get_val(ctx, operands[0]);
    LLVMValueRef v0 = LLVMBuildExtractValue(ctx->builder, v, 0, "v0");
    LLVMValueRef v1 = LLVMBuildExtractValue(ctx->builder, v, 1, "v1");

    LLVMTypeRef half_type = LLVMHalfTypeInContext(ctx->context);
    LLVMTypeRef v16half = LLVMVectorType(half_type, SIMT_WIDTH);
    LLVMTypeRef i16_type = LLVMInt16TypeInContext(ctx->context);
    LLVMTypeRef v16i16 = LLVMVectorType(i16_type, SIMT_WIDTH);
    LLVMTypeRef v16i32 = LLVMVectorType(ctx->int_type, SIMT_WIDTH);

    LLVMValueRef h0 = LLVMBuildFPTrunc(ctx->builder, v0, v16half, "h0");
    LLVMValueRef b0 = LLVMBuildBitCast(ctx->builder, h0, v16i16, "b0");
    LLVMValueRef u0 = LLVMBuildZExt(ctx->builder, b0, v16i32, "u0");

    LLVMValueRef h1 = LLVMBuildFPTrunc(ctx->builder, v1, v16half, "h1");
    LLVMValueRef b1 = LLVMBuildBitCast(ctx->builder, h1, v16i16, "b1");
    LLVMValueRef u1 = LLVMBuildZExt(ctx->builder, b1, v16i32, "u1");
    u1 = LLVMBuildShl(ctx->builder, u1, const_simt_int(ctx, 16), "sh1");

    LLVMValueRef packed = LLVMBuildOr(ctx->builder, u0, u1, "packed_half");
    set_val(ctx, res_id, jit_to_float_vector(ctx, packed));
}

void handle_ext_pack_double2x32(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef v = get_val(ctx, operands[0]);
    LLVMValueRef v0 = LLVMBuildExtractValue(ctx->builder, v, 0, "v0");
    LLVMValueRef v1 = LLVMBuildExtractValue(ctx->builder, v, 1, "v1");

    LLVMValueRef u0 = jit_to_int_vector(ctx, v0);
    LLVMValueRef u1 = jit_to_int_vector(ctx, v1);

    LLVMTypeRef i64_type = LLVMInt64TypeInContext(ctx->context);
    LLVMTypeRef v16i64 = LLVMVectorType(i64_type, SIMT_WIDTH);
    LLVMTypeRef f64_type = LLVMDoubleTypeInContext(ctx->context);
    LLVMTypeRef v16f64 = LLVMVectorType(f64_type, SIMT_WIDTH);

    LLVMValueRef z0 = LLVMBuildZExt(ctx->builder, u0, v16i64, "z0");
    LLVMValueRef z1 = LLVMBuildZExt(ctx->builder, u1, v16i64, "z1");
    z1 = LLVMBuildShl(ctx->builder, z1, const_simt_i64(ctx, 32), "sh1");

    LLVMValueRef packed_i64 = LLVMBuildOr(ctx->builder, z0, z1, "packed_i64");
    LLVMValueRef res_d = LLVMBuildBitCast(ctx->builder, packed_i64, v16f64, "packed_d");
    set_val(ctx, res_id, res_d);
}

void handle_ext_unpack_snorm2x16(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef u = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMTypeRef res_type = LLVMArrayType(ctx->vec_float_type, 2);
    LLVMValueRef div_f = const_simt_float(ctx, 32767.0f);
    LLVMValueRef neg_one_f = const_simt_float(ctx, -1.0f);
    LLVMValueRef one_f = const_simt_float(ctx, 1.0f);

    LLVMValueRef s0 = LLVMBuildAShr(ctx->builder, LLVMBuildShl(ctx->builder, u, const_simt_int(ctx, 16), "shl"), const_simt_int(ctx, 16), "s0");
    LLVMValueRef s1 = LLVMBuildAShr(ctx->builder, u, const_simt_int(ctx, 16), "s1");

    LLVMValueRef f0 = build_clamp_simt(ctx, LLVMBuildFDiv(ctx->builder, LLVMBuildSIToFP(ctx->builder, s0, ctx->vec_float_type, "f0"), div_f, ""), neg_one_f, one_f);
    LLVMValueRef f1 = build_clamp_simt(ctx, LLVMBuildFDiv(ctx->builder, LLVMBuildSIToFP(ctx->builder, s1, ctx->vec_float_type, "f1"), div_f, ""), neg_one_f, one_f);

    LLVMValueRef result = LLVMGetUndef(res_type);
    result = LLVMBuildInsertValue(ctx->builder, result, f0, 0, "res0");
    result = LLVMBuildInsertValue(ctx->builder, result, f1, 1, "res1");
    set_val(ctx, res_id, result);
}

void handle_ext_unpack_unorm2x16(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef u = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMTypeRef res_type = LLVMArrayType(ctx->vec_float_type, 2);
    LLVMValueRef mask = const_simt_int(ctx, 0xFFFF);
    LLVMValueRef div_f = const_simt_float(ctx, 65535.0f);

    LLVMValueRef u0 = LLVMBuildAnd(ctx->builder, u, mask, "u0");
    LLVMValueRef u1 = LLVMBuildAnd(ctx->builder, LLVMBuildLShr(ctx->builder, u, const_simt_int(ctx, 16), "sh1"), mask, "u1");

    LLVMValueRef f0 = LLVMBuildFDiv(ctx->builder, LLVMBuildUIToFP(ctx->builder, u0, ctx->vec_float_type, "f0"), div_f, "");
    LLVMValueRef f1 = LLVMBuildFDiv(ctx->builder, LLVMBuildUIToFP(ctx->builder, u1, ctx->vec_float_type, "f1"), div_f, "");

    LLVMValueRef result = LLVMGetUndef(res_type);
    result = LLVMBuildInsertValue(ctx->builder, result, f0, 0, "res0");
    result = LLVMBuildInsertValue(ctx->builder, result, f1, 1, "res1");
    set_val(ctx, res_id, result);
}

void handle_ext_unpack_half2x16(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef u = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMTypeRef half_type = LLVMHalfTypeInContext(ctx->context);
    LLVMTypeRef v16half = LLVMVectorType(half_type, SIMT_WIDTH);
    LLVMTypeRef i16_type = LLVMInt16TypeInContext(ctx->context);
    LLVMTypeRef v16i16 = LLVMVectorType(i16_type, SIMT_WIDTH);

    LLVMValueRef b0 = LLVMBuildTrunc(ctx->builder, u, v16i16, "b0");
    LLVMValueRef h0 = LLVMBuildBitCast(ctx->builder, b0, v16half, "h0");
    LLVMValueRef f0 = LLVMBuildFPExt(ctx->builder, h0, ctx->vec_float_type, "f0");

    LLVMValueRef u1 = LLVMBuildLShr(ctx->builder, u, const_simt_int(ctx, 16), "sh1");
    LLVMValueRef b1 = LLVMBuildTrunc(ctx->builder, u1, v16i16, "b1");
    LLVMValueRef h1 = LLVMBuildBitCast(ctx->builder, b1, v16half, "h1");
    LLVMValueRef f1 = LLVMBuildFPExt(ctx->builder, h1, ctx->vec_float_type, "f1");

    LLVMTypeRef res_type = LLVMArrayType(ctx->vec_float_type, 2);
    LLVMValueRef result = LLVMGetUndef(res_type);
    result = LLVMBuildInsertValue(ctx->builder, result, f0, 0, "res0");
    result = LLVMBuildInsertValue(ctx->builder, result, f1, 1, "res1");
    set_val(ctx, res_id, result);
}

void handle_ext_unpack_snorm4x8(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef u = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMTypeRef res_type = LLVMArrayType(ctx->vec_float_type, 4);
    LLVMValueRef result = LLVMGetUndef(res_type);
    LLVMValueRef div_f = const_simt_float(ctx, 127.0f);
    LLVMValueRef neg_one_f = const_simt_float(ctx, -1.0f);
    LLVMValueRef one_f = const_simt_float(ctx, 1.0f);

    for (uint32_t i = 0; i < 4; i++)
    {
        uint32_t shl_amt = 24 - i * 8;
        LLVMValueRef comp = u;
        if (shl_amt > 0)
            comp = LLVMBuildShl(ctx->builder, comp, const_simt_int(ctx, shl_amt), "shl");
        comp = LLVMBuildAShr(ctx->builder, comp, const_simt_int(ctx, 24), "ashr");

        LLVMValueRef comp_f = LLVMBuildSIToFP(ctx->builder, comp, ctx->vec_float_type, "to_fp");
        comp_f = LLVMBuildFDiv(ctx->builder, comp_f, div_f, "snorm_f");
        comp_f = build_clamp_simt(ctx, comp_f, neg_one_f, one_f);
        result = LLVMBuildInsertValue(ctx->builder, result, comp_f, i, "unpack_elem");
    }
    set_val(ctx, res_id, result);
}

void handle_ext_unpack_unorm4x8(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef u = jit_to_int_vector(ctx, get_val(ctx, operands[0]));
    LLVMTypeRef res_type = LLVMArrayType(ctx->vec_float_type, 4);
    LLVMValueRef result = LLVMGetUndef(res_type);
    LLVMValueRef mask = const_simt_int(ctx, 0xFF);
    LLVMValueRef div_f = const_simt_float(ctx, 255.0f);

    for (uint32_t i = 0; i < 4; i++)
    {
        LLVMValueRef comp = u;
        if (i > 0)
        {
            LLVMValueRef sh = const_simt_int(ctx, i * 8);
            comp = LLVMBuildLShr(ctx->builder, comp, sh, "shr");
        }
        comp = LLVMBuildAnd(ctx->builder, comp, mask, "masked");
        LLVMValueRef comp_f = LLVMBuildUIToFP(ctx->builder, comp, ctx->vec_float_type, "to_fp");
        comp_f = LLVMBuildFDiv(ctx->builder, comp_f, div_f, "unorm_f");
        result = LLVMBuildInsertValue(ctx->builder, result, comp_f, i, "unpack_elem");
    }
    set_val(ctx, res_id, result);
}

void handle_ext_unpack_double2x32(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef d = get_val(ctx, operands[0]);
    LLVMTypeRef i64_type = LLVMInt64TypeInContext(ctx->context);
    LLVMTypeRef v16i64 = LLVMVectorType(i64_type, SIMT_WIDTH);
    LLVMTypeRef v16i32 = LLVMVectorType(ctx->int_type, SIMT_WIDTH);

    LLVMValueRef bits = LLVMBuildBitCast(ctx->builder, d, v16i64, "d2i64");
    LLVMValueRef u0 = LLVMBuildTrunc(ctx->builder, bits, v16i32, "u0");
    LLVMValueRef sh = LLVMBuildLShr(ctx->builder, bits, const_simt_i64(ctx, 32), "sh");
    LLVMValueRef u1 = LLVMBuildTrunc(ctx->builder, sh, v16i32, "u1");

    LLVMTypeRef res_type = LLVMArrayType(ctx->vec_float_type, 2);
    LLVMValueRef result = LLVMGetUndef(res_type);
    result = LLVMBuildInsertValue(ctx->builder, result, jit_to_float_vector(ctx, u0), 0, "res0");
    result = LLVMBuildInsertValue(ctx->builder, result, jit_to_float_vector(ctx, u1), 1, "res1");
    set_val(ctx, res_id, result);
}

static LLVMValueRef build_find_ilsb_simt(JitContext* ctx, LLVMValueRef simt_val)
{
    LLVMValueRef val_int = jit_to_int_vector(ctx, simt_val);
    LLVMTypeRef v16i32 = LLVMTypeOf(val_int);
    unsigned id = LLVMLookupIntrinsicID("llvm.cttz", 9);
    LLVMValueRef func = LLVMGetIntrinsicDeclaration(ctx->module, id, &v16i32, 1);
    LLVMValueRef args[] = { val_int, LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0) };
    LLVMValueRef cttz_val = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(func), func, args, 2, "cttz_val");

    LLVMValueRef zero = const_simt_int(ctx, 0);
    LLVMValueRef is_zero = LLVMBuildICmp(ctx->builder, LLVMIntEQ, val_int, zero, "is_zero");
    LLVMValueRef neg_one = const_simt_int(ctx, -1);
    LLVMValueRef res = LLVMBuildSelect(ctx->builder, is_zero, neg_one, cttz_val, "find_ilsb");
    return jit_to_float_vector(ctx, res);
}

void handle_ext_find_ilsb(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    set_val(ctx, res_id, dispatch_unary_int_op(ctx, x, build_find_ilsb_simt));
}

static LLVMValueRef build_find_smsb_simt(JitContext* ctx, LLVMValueRef simt_val)
{
    LLVMValueRef val_int = jit_to_int_vector(ctx, simt_val);
    LLVMValueRef zero = const_simt_int(ctx, 0);
    LLVMValueRef is_neg = LLVMBuildICmp(ctx->builder, LLVMIntSLT, val_int, zero, "is_neg");
    LLVMValueRef not_val = LLVMBuildNot(ctx->builder, val_int, "not_val");
    LLVMValueRef norm_val = LLVMBuildSelect(ctx->builder, is_neg, not_val, val_int, "norm_val");

    LLVMTypeRef v16i32 = LLVMTypeOf(norm_val);
    unsigned id = LLVMLookupIntrinsicID("llvm.ctlz", 9);
    LLVMValueRef func = LLVMGetIntrinsicDeclaration(ctx->module, id, &v16i32, 1);
    LLVMValueRef args[] = { norm_val, LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0) };
    LLVMValueRef ctlz_val = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(func), func, args, 2, "ctlz_val");

    LLVMValueRef thirty_one = const_simt_int(ctx, 31);
    LLVMValueRef msb = LLVMBuildSub(ctx->builder, thirty_one, ctlz_val, "sub_ctlz");

    LLVMValueRef is_zero = LLVMBuildICmp(ctx->builder, LLVMIntEQ, norm_val, zero, "is_zero");
    LLVMValueRef neg_one = const_simt_int(ctx, -1);
    LLVMValueRef res = LLVMBuildSelect(ctx->builder, is_zero, neg_one, msb, "find_smsb");
    return jit_to_float_vector(ctx, res);
}

void handle_ext_find_smsb(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    set_val(ctx, res_id, dispatch_unary_int_op(ctx, x, build_find_smsb_simt));
}

static LLVMValueRef build_find_umsb_simt(JitContext* ctx, LLVMValueRef simt_val)
{
    LLVMValueRef val_int = jit_to_int_vector(ctx, simt_val);
    LLVMTypeRef v16i32 = LLVMTypeOf(val_int);
    unsigned id = LLVMLookupIntrinsicID("llvm.ctlz", 9);
    LLVMValueRef func = LLVMGetIntrinsicDeclaration(ctx->module, id, &v16i32, 1);
    LLVMValueRef args[] = { val_int, LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0) };
    LLVMValueRef ctlz_val = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(func), func, args, 2, "ctlz_val");

    LLVMValueRef thirty_one = const_simt_int(ctx, 31);
    LLVMValueRef msb = LLVMBuildSub(ctx->builder, thirty_one, ctlz_val, "sub_ctlz");

    LLVMValueRef zero = const_simt_int(ctx, 0);
    LLVMValueRef is_zero = LLVMBuildICmp(ctx->builder, LLVMIntEQ, val_int, zero, "is_zero");
    LLVMValueRef neg_one = const_simt_int(ctx, -1);
    LLVMValueRef res = LLVMBuildSelect(ctx->builder, is_zero, neg_one, msb, "find_umsb");
    return jit_to_float_vector(ctx, res);
}

void handle_ext_find_umsb(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    set_val(ctx, res_id, dispatch_unary_int_op(ctx, x, build_find_umsb_simt));
}

static LLVMValueRef load_interpolant(JitContext* ctx, uint32_t operand_id)
{
    LLVMValueRef val = get_val(ctx, operand_id);
    if (!val) return NULL;
    if (LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMPointerTypeKind)
    {
        uint32_t ptr_type_id = ctx->type_kind_map[operand_id];
        if (ptr_type_id > 0 && ctx->type_info[ptr_type_id].opcode == SpvOpTypePointer)
        {
            uint32_t base_type = ctx->type_info[ptr_type_id].base_type_id;
            LLVMTypeRef loaded_type = map_spv_to_llvm_type(ctx, base_type);
            return build_recursive_load(ctx, loaded_type, val);
        }
    }
    return val;
}

void handle_ext_interpolate_at_centroid(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef val = load_interpolant(ctx, operands[0]);
    if (val) set_val(ctx, res_id, val);
}

void handle_ext_interpolate_at_sample(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    (void)operands;
    LLVMValueRef val = load_interpolant(ctx, operands[0]);
    if (val) set_val(ctx, res_id, val);
}

void handle_ext_interpolate_at_offset(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    (void)operands;
    LLVMValueRef val = load_interpolant(ctx, operands[0]);
    if (val) set_val(ctx, res_id, val);
}

void handle_ext_exp(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_unary_intrinsic_op(ctx, x, "llvm.exp"));
}

void handle_ext_tan(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_unary_intrinsic_op(ctx, x, "llvm.tan"));
}

void handle_ext_sin(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_unary_intrinsic_op(ctx, x, "llvm.sin"));
}

void handle_ext_cos(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_unary_intrinsic_op(ctx, x, "llvm.cos"));
}

void handle_ext_sqrt(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_unary_intrinsic_op(ctx, x, "llvm.sqrt"));
}

void handle_ext_pow(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef y = get_val(ctx, operands[1]);
    set_val(ctx, res_id, build_binary_intrinsic_op(ctx, x, y, "llvm.pow"));
}

void handle_ext_atan2(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef y = get_val(ctx, operands[1]);
    set_val(ctx, res_id, build_binary_intrinsic_op(ctx, x, y, "llvm.atan2"));
}

void handle_ext_log(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_unary_intrinsic_op(ctx, x, "llvm.log"));
}

void handle_ext_fabs(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_unary_intrinsic_op(ctx, x, "llvm.fabs"));
}

void handle_ext_fmax(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef y = get_val(ctx, operands[1]);
    set_val(ctx, res_id, build_binary_intrinsic_op(ctx, x, y, "llvm.maxnum"));
}

void handle_ext_fmin(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef y = get_val(ctx, operands[1]);
    set_val(ctx, res_id, build_binary_intrinsic_op(ctx, x, y, "llvm.minnum"));
}

void handle_ext_fclamp(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef val = get_val(ctx, operands[0]);
    LLVMValueRef min = get_val(ctx, operands[1]);
    LLVMValueRef max = get_val(ctx, operands[2]);
    set_val(ctx, res_id, build_clamp_op(ctx, val, min, max));
}

void handle_ext_smoothstep(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef edge0 = get_val(ctx, operands[0]);
    LLVMValueRef edge1 = get_val(ctx, operands[1]);
    LLVMValueRef x = get_val(ctx, operands[2]);
    set_val(ctx, res_id, build_smoothstep_op(ctx, edge0, edge1, x));
}

void handle_ext_fmix(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    LLVMValueRef y = get_val(ctx, operands[1]);
    LLVMValueRef a = get_val(ctx, operands[2]);
    set_val(ctx, res_id, build_fmix_op(ctx, x, y, a));
}

void handle_ext_fsign(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef x = get_val(ctx, operands[0]);
    set_val(ctx, res_id, build_fsign_op(ctx, x));
}

void handle_ext_step(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef edge = get_val(ctx, operands[0]);
    LLVMValueRef x = get_val(ctx, operands[1]);
    set_val(ctx, res_id, build_step_op(ctx, edge, x));
}

void handle_ext_length(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef vec = get_val(ctx, operands[0]);
    LLVMTypeRef vec_type = LLVMTypeOf(vec);

    if (LLVMGetTypeKind(vec_type) == LLVMArrayTypeKind)
    {
        uint32_t num_elements = LLVMGetArrayLength(vec_type);
        LLVMValueRef result = NULL;

        for (uint32_t i = 0; i < num_elements; i++)
        {
            LLVMValueRef elem = LLVMBuildExtractValue(ctx->builder, vec, i, "elem");
            LLVMValueRef elem_sq = LLVMBuildFMul(ctx->builder, elem, elem, "elem_sq");
            result = (i == 0) ? elem_sq : LLVMBuildFAdd(ctx->builder, result, elem_sq, "sum_tmp");
        }
        set_val(ctx, res_id, build_unary_simt_intrinsic(ctx, result, "llvm.sqrt"));
    }
    else
    {
        set_val(ctx, res_id, build_unary_simt_intrinsic(ctx, vec, "llvm.fabs"));
    }
}

void handle_ext_normalize(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef vec = get_val(ctx, operands[0]);
    LLVMTypeRef vec_type = LLVMTypeOf(vec);

    if (LLVMGetTypeKind(vec_type) == LLVMArrayTypeKind)
    {
        uint32_t num_elements = LLVMGetArrayLength(vec_type);
        LLVMValueRef sum_sq = NULL;

        for (uint32_t i = 0; i < num_elements; i++)
        {
            LLVMValueRef elem = LLVMBuildExtractValue(ctx->builder, vec, i, "elem");
            LLVMValueRef elem_sq = LLVMBuildFMul(ctx->builder, elem, elem, "elem_sq");
            sum_sq = (i == 0) ? elem_sq : LLVMBuildFAdd(ctx->builder, sum_sq, elem_sq, "sum_tmp");
        }

        LLVMValueRef len = build_unary_simt_intrinsic(ctx, sum_sq, "llvm.sqrt");
        LLVMValueRef normalized_vec = LLVMGetUndef(vec_type);

        for (uint32_t i = 0; i < num_elements; i++)
        {
            LLVMValueRef elem = LLVMBuildExtractValue(ctx->builder, vec, i, "elem");
            LLVMValueRef norm_elem = LLVMBuildFDiv(ctx->builder, elem, len, "norm_elem");
            normalized_vec = LLVMBuildInsertValue(ctx->builder, normalized_vec, norm_elem, i, "normalized_vec");
        }
        set_val(ctx, res_id, normalized_vec);
    }
    else
    {
        set_val(ctx, res_id, build_fsign_simt(ctx, vec));
    }
}

void handle_ext_reflect(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef I = get_val(ctx, operands[0]);
    LLVMValueRef N = get_val(ctx, operands[1]);
    LLVMTypeRef vec_type = LLVMTypeOf(I);
    uint32_t num_elements = (LLVMGetTypeKind(vec_type) == LLVMArrayTypeKind) ? LLVMGetArrayLength(vec_type) : 1;

    // d = dot(N, I)
    LLVMValueRef dot_ni = calculate_dot_product(ctx, N, I);

    // factor = 2.0 * dot(N, I)
    LLVMValueRef two_vec = const_simt_float(ctx, 2.0f);
    LLVMValueRef factor = LLVMBuildFMul(ctx->builder, two_vec, dot_ni, "factor");

    // R = I - (factor * N)
    if (LLVMGetTypeKind(vec_type) == LLVMArrayTypeKind)
    {
        LLVMValueRef result_vec = LLVMGetUndef(vec_type);
        for (uint32_t i = 0; i < num_elements; i++) 
        {
            LLVMValueRef i_elem = LLVMBuildExtractValue(ctx->builder, I, i, "i_elem");
            LLVMValueRef n_elem = LLVMBuildExtractValue(ctx->builder, N, i, "n_elem");
            LLVMValueRef scaled_n = LLVMBuildFMul(ctx->builder, factor, n_elem, "scaled_n");
            LLVMValueRef reflected_elem = LLVMBuildFSub(ctx->builder, i_elem, scaled_n, "reflected_elem");
            result_vec = LLVMBuildInsertValue(ctx->builder, result_vec, reflected_elem, i, "res_vec");
        }
        set_val(ctx, res_id, result_vec);
    }
    else
    {
        LLVMValueRef scaled_n = LLVMBuildFMul(ctx->builder, factor, N, "scaled_n");
        set_val(ctx, res_id, LLVMBuildFSub(ctx->builder, I, scaled_n, "reflected_elem"));
    }
}

void handle_ext_distance(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef vec1 = get_val(ctx, operands[0]);
    LLVMValueRef vec2 = get_val(ctx, operands[1]);
    LLVMTypeRef vec_type = LLVMTypeOf(vec1);

    if (LLVMGetTypeKind(vec_type) == LLVMArrayTypeKind)
    {
        uint32_t num_elements = LLVMGetArrayLength(vec_type);
        LLVMValueRef result = NULL;

        for (uint32_t i = 0; i < num_elements; i++)
        {
            LLVMValueRef elem1 = LLVMBuildExtractValue(ctx->builder, vec1, i, "elem1");
            LLVMValueRef elem2 = LLVMBuildExtractValue(ctx->builder, vec2, i, "elem2");
            LLVMValueRef diff = LLVMBuildFSub(ctx->builder, elem1, elem2, "diff");
            LLVMValueRef diff_sq = LLVMBuildFMul(ctx->builder, diff, diff, "diff_sq");
            result = (i == 0) ? diff_sq : LLVMBuildFAdd(ctx->builder, result, diff_sq, "sum_tmp");
        }
        set_val(ctx, res_id, build_unary_simt_intrinsic(ctx, result, "llvm.sqrt"));
    }
    else
    {
        LLVMValueRef diff = LLVMBuildFSub(ctx->builder, vec1, vec2, "diff");
        set_val(ctx, res_id, build_unary_simt_intrinsic(ctx, diff, "llvm.fabs"));
    }
}

void handle_ext_cross(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef vec_x = get_val(ctx, operands[0]);
    LLVMValueRef vec_y = get_val(ctx, operands[1]);

    LLVMTypeRef vec_type = LLVMTypeOf(vec_x);
    if (LLVMGetTypeKind(vec_type) != LLVMArrayTypeKind || LLVMGetArrayLength(vec_type) != 3) 
    {
        printf("Error: cross function expects a vec3 input\n");
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

    LLVMValueRef res = LLVMGetUndef(vec_type);
    res = LLVMBuildInsertValue(ctx->builder, res, r0, 0, "res0");
    res = LLVMBuildInsertValue(ctx->builder, res, r1, 1, "res1");
    res = LLVMBuildInsertValue(ctx->builder, res, r2, 2, "res2");
    set_val(ctx, res_id, res);
}

void handle_ext_refract(JitContext* ctx, uint32_t res_id, uint32_t* operands)
{
    LLVMValueRef one_vec = const_simt_float(ctx, 1.0f);
    LLVMValueRef zero_vec = const_simt_float(ctx, 0.0f);

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
