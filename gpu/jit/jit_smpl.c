#include "jit_smpl.h"
#include <math.h>

static inline float clamp_float(float v, float min_v, float max_v)
{
    if (v < min_v) return min_v;
    if (v > max_v) return max_v;
    return v;
}

static inline float wrap_coordinate(float coord, WrapMode wrap)
{
    if (wrap == WRAP_CLAMP)
    {
        return clamp_float(coord, 0.0f, 1.0f);
    }
    else // WRAP_REPEAT
    {
        float c = coord - floorf(coord);
        if (c < 0.0f) c += 1.0f;
        return c;
    }
}

static inline void read_texel_color(const TextureSamplerDescriptor *desc, int x, int y, float *r, float *g, float *b, float *a)
{
    if (!desc || !desc->data || desc->width == 0 || desc->height == 0)
    {
        *r = 0.0f; *g = 0.0f; *b = 0.0f; *a = 1.0f;
        return;
    }

    if (x < 0) x = 0;
    if (x >= (int)desc->width) x = (int)desc->width - 1;
    if (y < 0) y = 0;
    if (y >= (int)desc->height) y = (int)desc->height - 1;

    uint32_t channels = desc->channels ? desc->channels : 4;
    const uint8_t *pixels = (const uint8_t *)desc->data;
    uint32_t idx = ((uint32_t)y * desc->width + (uint32_t)x) * channels;

    switch (channels)
    {
        case 1:
        {
            float v = pixels[idx] / 255.0f;
            *r = v;
            *g = v;
            *b = v;
            *a = 1.0f;
            break;
        }
        case 2:
        {
            *r = pixels[idx + 0] / 255.0f;
            *g = pixels[idx + 1] / 255.0f;
            *b = 0.0f;
            *a = 1.0f;
            break;
        }
        case 3:
        {
            *r = pixels[idx + 0] / 255.0f;
            *g = pixels[idx + 1] / 255.0f;
            *b = pixels[idx + 2] / 255.0f;
            *a = 1.0f;
            break;
        }
        case 4:
        default:
        {
            *r = pixels[idx + 0] / 255.0f;
            *g = pixels[idx + 1] / 255.0f;
            *b = pixels[idx + 2] / 255.0f;
            *a = pixels[idx + 3] / 255.0f;
            break;
        }
    }
}

static inline void sample_texel_single(const TextureSamplerDescriptor *desc, float u, float v, float *r, float *g, float *b, float *a)
{
    if (!desc || !desc->data || desc->width == 0 || desc->height == 0)
    {
        *r = 0.0f; *g = 0.0f; *b = 0.0f; *a = 1.0f;
        return;
    }

    float u_norm = wrap_coordinate(u, desc->wrap);
    float v_norm = wrap_coordinate(v, desc->wrap);

    if (desc->filter == FILTER_LINEAR)
    {
        float u_tex = u_norm * (float)desc->width - 0.5f;
        float v_tex = v_norm * (float)desc->height - 0.5f;

        int x0 = (int)floorf(u_tex);
        int y0 = (int)floorf(v_tex);
        int x1 = x0 + 1;
        int y1 = y0 + 1;

        float fx = u_tex - (float)x0;
        float fy = v_tex - (float)y0;

        if (desc->wrap == WRAP_REPEAT)
        {
            x0 = ((x0 % (int)desc->width) + (int)desc->width) % (int)desc->width;
            x1 = ((x1 % (int)desc->width) + (int)desc->width) % (int)desc->width;
            y0 = ((y0 % (int)desc->height) + (int)desc->height) % (int)desc->height;
            y1 = ((y1 % (int)desc->height) + (int)desc->height) % (int)desc->height;
        }
        else // WRAP_CLAMP
        {
            if (x0 < 0) x0 = 0;
            if (x0 >= (int)desc->width) x0 = (int)desc->width - 1;
            if (x1 < 0) x1 = 0;
            if (x1 >= (int)desc->width) x1 = (int)desc->width - 1;

            if (y0 < 0) y0 = 0;
            if (y0 >= (int)desc->height) y0 = (int)desc->height - 1;
            if (y1 < 0) y1 = 0;
            if (y1 >= (int)desc->height) y1 = (int)desc->height - 1;
        }

        float r00, g00, b00, a00;
        float r10, g10, b10, a10;
        float r01, g01, b01, a01;
        float r11, g11, b11, a11;

        read_texel_color(desc, x0, y0, &r00, &g00, &b00, &a00);
        read_texel_color(desc, x1, y0, &r10, &g10, &b10, &a10);
        read_texel_color(desc, x0, y1, &r01, &g01, &b01, &a01);
        read_texel_color(desc, x1, y1, &r11, &g11, &b11, &a11);

        float r0 = r00 * (1.0f - fx) + r10 * fx;
        float g0 = g00 * (1.0f - fx) + g10 * fx;
        float b0 = b00 * (1.0f - fx) + b10 * fx;
        float a0 = a00 * (1.0f - fx) + a10 * fx;

        float r1 = r01 * (1.0f - fx) + r11 * fx;
        float g1 = g01 * (1.0f - fx) + g11 * fx;
        float b1 = b01 * (1.0f - fx) + b11 * fx;
        float a1 = a01 * (1.0f - fx) + a11 * fx;

        *r = r0 * (1.0f - fy) + r1 * fy;
        *g = g0 * (1.0f - fy) + g1 * fy;
        *b = b0 * (1.0f - fy) + b1 * fy;
        *a = a0 * (1.0f - fy) + a1 * fy;
    }
    else // FILTER_NEAREST
    {
        int x = (int)floorf(u_norm * (float)desc->width);
        int y = (int)floorf(v_norm * (float)desc->height);

        if (desc->wrap == WRAP_REPEAT)
        {
            x = ((x % (int)desc->width) + (int)desc->width) % (int)desc->width;
            y = ((y % (int)desc->height) + (int)desc->height) % (int)desc->height;
        }
        else // WRAP_CLAMP
        {
            if (x < 0) x = 0;
            if (x >= (int)desc->width) x = (int)desc->width - 1;
            if (y < 0) y = 0;
            if (y >= (int)desc->height) y = (int)desc->height - 1;
        }

        read_texel_color(desc, x, y, r, g, b, a);
    }
}

void sample_texture_2d_simt(
    const TextureSamplerDescriptor *desc,
    const float *u_coords,
    const float *v_coords,
    float *out_r,
    float *out_g,
    float *out_b,
    float *out_a
)
{
    for (int lane = 0; lane < SIMT_WIDTH; lane++)
    {
        sample_texel_single(desc, u_coords[lane], v_coords[lane], &out_r[lane], &out_g[lane], &out_b[lane], &out_a[lane]);
    }
}

void fetch_texture_2d_simt(
    const TextureSamplerDescriptor *desc,
    const int32_t *x_coords,
    const int32_t *y_coords,
    float *out_r,
    float *out_g,
    float *out_b,
    float *out_a
)
{
    for (int lane = 0; lane < SIMT_WIDTH; lane++)
    {
        if (!desc || !desc->data || desc->width == 0 || desc->height == 0)
        {
            out_r[lane] = 0.0f;
            out_g[lane] = 0.0f;
            out_b[lane] = 0.0f;
            out_a[lane] = 1.0f;
            continue;
        }

        int x = x_coords[lane];
        int y = y_coords[lane];
        read_texel_color(desc, x, y, &out_r[lane], &out_g[lane], &out_b[lane], &out_a[lane]);
    }
}

void handle_op_type_image(JitContext *ctx, uint32_t res_id, uint32_t *operands)
{
    ctx->type_info[res_id].opcode = SpvOpTypeImage;
    ctx->type_info[res_id].base_type_id = operands[0];

    ctx->type_info[res_id].image_type.sampled_type = operands[1];
    ctx->type_info[res_id].image_type.dim         = operands[2];
    ctx->type_info[res_id].image_type.depth       = operands[3];
    ctx->type_info[res_id].image_type.arrayed     = operands[4];
    ctx->type_info[res_id].image_type.ms          = operands[5];
    ctx->type_info[res_id].image_type.sampled     = operands[6];
    ctx->type_info[res_id].image_type.format      = operands[7];
}

void handle_op_type_sampler(JitContext *ctx, uint32_t res_id)
{
    ctx->type_info[res_id].opcode = SpvOpTypeSampler;
    ctx->type_info[res_id].base_type_id = 0;
}

void handle_op_type_sampled_image(JitContext *ctx, uint32_t res_id, uint32_t image_type_id)
{
    ctx->type_info[res_id].opcode = SpvOpTypeSampledImage;
    ctx->type_info[res_id].sampled_image.image_type_id = image_type_id;
}

void handle_op_sampled_image(JitContext *ctx, uint32_t res_id, uint32_t *operands)
{
    uint32_t image_id = operands[0];
    LLVMValueRef img_val = get_val(ctx, image_id);
    set_val(ctx, res_id, img_val);
}

void handle_op_image(JitContext *ctx, uint32_t res_id, uint32_t *operands)
{
    uint32_t sampled_image_id = operands[0];
    LLVMValueRef img_val = get_val(ctx, sampled_image_id);
    set_val(ctx, res_id, img_val);
}

static void emit_sample_call(JitContext *ctx, uint32_t res_id, LLVMValueRef image_val, LLVMValueRef u_coords, LLVMValueRef v_coords)
{
    LLVMTypeRef ptr_type = ctx->ptr_type;
    LLVMTypeRef vec_type = ctx->vec_float_type;

    if (!image_val)
    {
        image_val = LLVMConstNull(ptr_type);
    }

    LLVMValueRef u_alloca = LLVMBuildAlloca(ctx->builder, vec_type, "u_alloca");
    LLVMValueRef v_alloca = LLVMBuildAlloca(ctx->builder, vec_type, "v_alloca");
    LLVMValueRef r_alloca = LLVMBuildAlloca(ctx->builder, vec_type, "r_alloca");
    LLVMValueRef g_alloca = LLVMBuildAlloca(ctx->builder, vec_type, "g_alloca");
    LLVMValueRef b_alloca = LLVMBuildAlloca(ctx->builder, vec_type, "b_alloca");
    LLVMValueRef a_alloca = LLVMBuildAlloca(ctx->builder, vec_type, "a_alloca");

    LLVMSetAlignment(u_alloca, 64);
    LLVMSetAlignment(v_alloca, 64);
    LLVMSetAlignment(r_alloca, 64);
    LLVMSetAlignment(g_alloca, 64);
    LLVMSetAlignment(b_alloca, 64);
    LLVMSetAlignment(a_alloca, 64);

    LLVMBuildStore(ctx->builder, u_coords, u_alloca);
    LLVMBuildStore(ctx->builder, v_coords, v_alloca);

    LLVMValueRef sample_func = LLVMGetNamedFunction(ctx->module, "sample_texture_2d_simt");
    LLVMTypeRef param_types[7] = {
        ptr_type, ptr_type, ptr_type, ptr_type, ptr_type, ptr_type, ptr_type
    };
    LLVMTypeRef func_type = LLVMFunctionType(LLVMVoidTypeInContext(ctx->context), param_types, 7, 0);

    if (!sample_func)
    {
        sample_func = LLVMAddFunction(ctx->module, "sample_texture_2d_simt", func_type);
        if (ctx->engine)
        {
            LLVMAddGlobalMapping(ctx->engine, sample_func, (void*)&sample_texture_2d_simt);
        }
    }

    LLVMValueRef args[7] = {
        image_val,
        u_alloca,
        v_alloca,
        r_alloca,
        g_alloca,
        b_alloca,
        a_alloca
    };

    LLVMBuildCall2(ctx->builder, func_type, sample_func, args, 7, "");

    LLVMValueRef r_res = LLVMBuildLoad2(ctx->builder, vec_type, r_alloca, "r_res");
    LLVMValueRef g_res = LLVMBuildLoad2(ctx->builder, vec_type, g_alloca, "g_res");
    LLVMValueRef b_res = LLVMBuildLoad2(ctx->builder, vec_type, b_alloca, "b_res");
    LLVMValueRef a_res = LLVMBuildLoad2(ctx->builder, vec_type, a_alloca, "a_res");

    LLVMSetAlignment(r_res, 64);
    LLVMSetAlignment(g_res, 64);
    LLVMSetAlignment(b_res, 64);
    LLVMSetAlignment(a_res, 64);

    LLVMValueRef result_val = LLVMGetUndef(LLVMArrayType(vec_type, 4));
    result_val = LLVMBuildInsertValue(ctx->builder, result_val, r_res, 0, "res_r");
    result_val = LLVMBuildInsertValue(ctx->builder, result_val, g_res, 1, "res_g");
    result_val = LLVMBuildInsertValue(ctx->builder, result_val, b_res, 2, "res_b");
    result_val = LLVMBuildInsertValue(ctx->builder, result_val, a_res, 3, "res_a");

    set_val(ctx, res_id, result_val);
}

void handle_op_image_sample_implicit_lod(JitContext *ctx, uint32_t res_id, uint32_t *operands)
{
    uint32_t image_id = operands[0];
    uint32_t coords_id = operands[1];

    LLVMValueRef coords_val = get_val(ctx, coords_id);
    LLVMValueRef image_val = get_val(ctx, image_id);

    LLVMValueRef u_coords = LLVMBuildExtractValue(ctx->builder, coords_val, 0, "u_coords");
    LLVMValueRef v_coords = LLVMBuildExtractValue(ctx->builder, coords_val, 1, "v_coords");

    // Extract U and V coordinates for spatial derivatives
    LLVMValueRef ddx_mask_elems[SIMT_WIDTH];
    LLVMValueRef ddy_mask_elems[SIMT_WIDTH];

    for (int i = 0; i < SIMT_WIDTH; i++) 
    {
        int x_neighbor = (i % 2 == 0) ? i + 1 : i - 1;
        ddx_mask_elems[i] = LLVMConstInt(ctx->int_type, x_neighbor, 0);

        int quad_base = (i / 4) * 4;
        int local_idx = i % 4;
        int y_neighbor = quad_base + ((local_idx < 2) ? local_idx + 2 : local_idx - 2);
        ddy_mask_elems[i] = LLVMConstInt(ctx->int_type, y_neighbor, 0);
    }

    LLVMValueRef ddx_mask = LLVMConstVector(ddx_mask_elems, SIMT_WIDTH);
    LLVMValueRef ddy_mask = LLVMConstVector(ddy_mask_elems, SIMT_WIDTH);

    LLVMValueRef u_neighbor_x = LLVMBuildShuffleVector(ctx->builder, u_coords, u_coords, ddx_mask, "u_neighbor_x");
    LLVMValueRef v_neighbor_x = LLVMBuildShuffleVector(ctx->builder, v_coords, v_coords, ddx_mask, "v_neighbor_x");

    LLVMValueRef u_neighbor_y = LLVMBuildShuffleVector(ctx->builder, u_coords, u_coords, ddy_mask, "u_neighbor_y");
    LLVMValueRef v_neighbor_y = LLVMBuildShuffleVector(ctx->builder, v_coords, v_coords, ddy_mask, "v_neighbor_y");

    LLVMValueRef du_dx = LLVMBuildFSub(ctx->builder, u_coords, u_neighbor_x, "du_dx");
    LLVMValueRef dv_dx = LLVMBuildFSub(ctx->builder, v_coords, v_neighbor_x, "dv_dx");

    LLVMValueRef du_dy = LLVMBuildFSub(ctx->builder, u_coords, u_neighbor_y, "du_dy");
    LLVMValueRef dv_dy = LLVMBuildFSub(ctx->builder, v_coords, v_neighbor_y, "dv_dy");

    LLVMValueRef du_dx_sq = LLVMBuildFMul(ctx->builder, du_dx, du_dx, "du_dx_sq");
    LLVMValueRef dv_dx_sq = LLVMBuildFMul(ctx->builder, dv_dx, dv_dx, "dv_dx_sq");
    LLVMValueRef ddx_len_sq = LLVMBuildFAdd(ctx->builder, du_dx_sq, dv_dx_sq, "ddx_len_sq");

    LLVMValueRef du_dy_sq = LLVMBuildFMul(ctx->builder, du_dy, du_dy, "du_dy_sq");
    LLVMValueRef dv_dy_sq = LLVMBuildFMul(ctx->builder, dv_dy, dv_dy, "dv_dy_sq");
    LLVMValueRef ddy_len_sq = LLVMBuildFAdd(ctx->builder, du_dy_sq, dv_dy_sq, "ddy_len_sq");

    LLVMValueRef cmp_len = LLVMBuildFCmp(ctx->builder, LLVMRealOGT, ddx_len_sq, ddy_len_sq, "cmp_len");
    LLVMValueRef max_len_sq = LLVMBuildSelect(ctx->builder, cmp_len, ddx_len_sq, ddy_len_sq, "max_len_sq");

    LLVMTypeRef log2_type = LLVMFunctionType(ctx->vec_float_type, &ctx->vec_float_type, 1, 0);
    LLVMValueRef log2_func = LLVMGetNamedFunction(ctx->module, "llvm.log2.v16f32");
    if (!log2_func) 
    {
        log2_func = LLVMAddFunction(ctx->module, "llvm.log2.v16f32", log2_type);
    }

    LLVMValueRef log2_val = LLVMBuildCall2(ctx->builder, log2_type, log2_func, &max_len_sq, 1, "log2_val");

    LLVMValueRef half_vec_elems[SIMT_WIDTH];
    for (int i = 0; i < SIMT_WIDTH; i++) 
    {
        half_vec_elems[i] = LLVMConstReal(ctx->float_type, 0.5f);
    }
    LLVMValueRef half_vec = LLVMConstVector(half_vec_elems, SIMT_WIDTH);
    (void)LLVMBuildFMul(ctx->builder, log2_val, half_vec, "lod_base");

    emit_sample_call(ctx, res_id, image_val, u_coords, v_coords);
}

void handle_op_image_sample_explicit_lod(JitContext *ctx, uint32_t res_id, uint32_t *operands)
{
    uint32_t image_id = operands[0];
    uint32_t coords_id = operands[1];

    LLVMValueRef coords_val = get_val(ctx, coords_id);
    LLVMValueRef image_val = get_val(ctx, image_id);

    LLVMValueRef u_coords = LLVMBuildExtractValue(ctx->builder, coords_val, 0, "u_coords");
    LLVMValueRef v_coords = LLVMBuildExtractValue(ctx->builder, coords_val, 1, "v_coords");

    emit_sample_call(ctx, res_id, image_val, u_coords, v_coords);
}

void handle_op_image_fetch(JitContext *ctx, uint32_t res_id, uint32_t *operands)
{
    uint32_t image_id = operands[0];
    uint32_t coords_id = operands[1];

    LLVMValueRef coords_val = get_val(ctx, coords_id);
    LLVMValueRef image_val = get_val(ctx, image_id);
    if (!image_val)
    {
        image_val = LLVMConstNull(ctx->ptr_type);
    }

    LLVMValueRef x_coords = LLVMBuildExtractValue(ctx->builder, coords_val, 0, "x_coords");
    LLVMValueRef y_coords = LLVMBuildExtractValue(ctx->builder, coords_val, 1, "y_coords");

    LLVMTypeRef ptr_type = ctx->ptr_type;
    LLVMTypeRef vec_float_type = ctx->vec_float_type;
    LLVMTypeRef vec_int_type = LLVMVectorType(ctx->int_type, SIMT_WIDTH);

    LLVMValueRef x_alloca = LLVMBuildAlloca(ctx->builder, vec_int_type, "x_alloca");
    LLVMValueRef y_alloca = LLVMBuildAlloca(ctx->builder, vec_int_type, "y_alloca");
    LLVMValueRef r_alloca = LLVMBuildAlloca(ctx->builder, vec_float_type, "r_alloca");
    LLVMValueRef g_alloca = LLVMBuildAlloca(ctx->builder, vec_float_type, "g_alloca");
    LLVMValueRef b_alloca = LLVMBuildAlloca(ctx->builder, vec_float_type, "b_alloca");
    LLVMValueRef a_alloca = LLVMBuildAlloca(ctx->builder, vec_float_type, "a_alloca");

    LLVMSetAlignment(x_alloca, 64);
    LLVMSetAlignment(y_alloca, 64);
    LLVMSetAlignment(r_alloca, 64);
    LLVMSetAlignment(g_alloca, 64);
    LLVMSetAlignment(b_alloca, 64);
    LLVMSetAlignment(a_alloca, 64);

    LLVMBuildStore(ctx->builder, x_coords, x_alloca);
    LLVMBuildStore(ctx->builder, y_coords, y_alloca);

    LLVMValueRef fetch_func = LLVMGetNamedFunction(ctx->module, "fetch_texture_2d_simt");
    LLVMTypeRef param_types[7] = {
        ptr_type, ptr_type, ptr_type, ptr_type, ptr_type, ptr_type, ptr_type
    };
    LLVMTypeRef func_type = LLVMFunctionType(LLVMVoidTypeInContext(ctx->context), param_types, 7, 0);

    if (!fetch_func)
    {
        fetch_func = LLVMAddFunction(ctx->module, "fetch_texture_2d_simt", func_type);
        if (ctx->engine)
        {
            LLVMAddGlobalMapping(ctx->engine, fetch_func, (void*)&fetch_texture_2d_simt);
        }
    }

    LLVMValueRef args[7] = {
        image_val,
        x_alloca,
        y_alloca,
        r_alloca,
        g_alloca,
        b_alloca,
        a_alloca
    };

    LLVMBuildCall2(ctx->builder, func_type, fetch_func, args, 7, "");

    LLVMValueRef r_res = LLVMBuildLoad2(ctx->builder, vec_float_type, r_alloca, "r_res");
    LLVMValueRef g_res = LLVMBuildLoad2(ctx->builder, vec_float_type, g_alloca, "g_res");
    LLVMValueRef b_res = LLVMBuildLoad2(ctx->builder, vec_float_type, b_alloca, "b_res");
    LLVMValueRef a_res = LLVMBuildLoad2(ctx->builder, vec_float_type, a_alloca, "a_res");

    LLVMSetAlignment(r_res, 64);
    LLVMSetAlignment(g_res, 64);
    LLVMSetAlignment(b_res, 64);
    LLVMSetAlignment(a_res, 64);

    LLVMValueRef result_val = LLVMGetUndef(LLVMArrayType(vec_float_type, 4));
    result_val = LLVMBuildInsertValue(ctx->builder, result_val, r_res, 0, "res_r");
    result_val = LLVMBuildInsertValue(ctx->builder, result_val, g_res, 1, "res_g");
    result_val = LLVMBuildInsertValue(ctx->builder, result_val, b_res, 2, "res_b");
    result_val = LLVMBuildInsertValue(ctx->builder, result_val, a_res, 3, "res_a");

    set_val(ctx, res_id, result_val);
}

void handle_op_image_query_size(JitContext *ctx, uint32_t res_id, uint32_t *operands)
{
    uint32_t image_id = operands[0];
    LLVMValueRef image_val = get_val(ctx, image_id);

    LLVMTypeRef i32_type = ctx->int_type;
    LLVMTypeRef vec_i32_type = LLVMVectorType(i32_type, SIMT_WIDTH);

    LLVMValueRef width_vec;
    LLVMValueRef height_vec;

    if (image_val)
    {
        LLVMValueRef width_idx = LLVMConstInt(i32_type, 8, 0);
        LLVMValueRef width_ptr = LLVMBuildInBoundsGEP2(ctx->builder, ctx->int8_type, image_val, &width_idx, 1, "width_ptr");
        LLVMValueRef width_scalar = LLVMBuildLoad2(ctx->builder, i32_type, width_ptr, "width_scalar");
        
        LLVMValueRef height_idx = LLVMConstInt(i32_type, 12, 0);
        LLVMValueRef height_ptr = LLVMBuildInBoundsGEP2(ctx->builder, ctx->int8_type, image_val, &height_idx, 1, "height_ptr");
        LLVMValueRef height_scalar = LLVMBuildLoad2(ctx->builder, i32_type, height_ptr, "height_scalar");

        LLVMValueRef width_undef = LLVMGetUndef(vec_i32_type);
        LLVMValueRef height_undef = LLVMGetUndef(vec_i32_type);
        LLVMValueRef zero_idx = LLVMConstInt(i32_type, 0, 0);
        width_vec = LLVMBuildInsertElement(ctx->builder, width_undef, width_scalar, zero_idx, "w_vec_0");
        height_vec = LLVMBuildInsertElement(ctx->builder, height_undef, height_scalar, zero_idx, "h_vec_0");

        LLVMValueRef zero_mask_elems[SIMT_WIDTH];
        for (int i = 0; i < SIMT_WIDTH; i++) zero_mask_elems[i] = LLVMConstInt(i32_type, 0, 0);
        LLVMValueRef zero_mask = LLVMConstVector(zero_mask_elems, SIMT_WIDTH);
        width_vec = LLVMBuildShuffleVector(ctx->builder, width_vec, width_vec, zero_mask, "w_splat");
        height_vec = LLVMBuildShuffleVector(ctx->builder, height_vec, height_vec, zero_mask, "h_splat");
    }
    else
    {
        LLVMValueRef zero_elems[SIMT_WIDTH];
        for (int i = 0; i < SIMT_WIDTH; i++) zero_elems[i] = LLVMConstInt(i32_type, 0, 0);
        width_vec = LLVMConstVector(zero_elems, SIMT_WIDTH);
        height_vec = LLVMConstVector(zero_elems, SIMT_WIDTH);
    }

    LLVMValueRef res = LLVMGetUndef(LLVMArrayType(vec_i32_type, 2));
    res = LLVMBuildInsertValue(ctx->builder, res, width_vec, 0, "size_w");
    res = LLVMBuildInsertValue(ctx->builder, res, height_vec, 1, "size_h");
    set_val(ctx, res_id, res);
}

void handle_op_image_query_size_lod(JitContext *ctx, uint32_t res_id, uint32_t *operands)
{
    handle_op_image_query_size(ctx, res_id, operands);
}

static inline uint8_t color_to_u8(float c)
{
    if (c <= 0.0f) return 0;
    float scaled = c * 255.0f;
    if (scaled >= 255.0f) return 255;
    return (uint8_t)(scaled + 0.5f);
}

void image_write_2d_simt(
    const TextureSamplerDescriptor *desc,
    const int32_t *x_coords,
    const int32_t *y_coords,
    const float *in_r,
    const float *in_g,
    const float *in_b,
    const float *in_a,
    const int32_t *mask
)
{
    if (!desc || !desc->data || desc->width == 0 || desc->height == 0)
        return;

    uint32_t channels = desc->channels ? desc->channels : 4;
    uint8_t *pixels = (uint8_t *)desc->data;

    for (int lane = 0; lane < SIMT_WIDTH; lane++)
    {
        if (!mask[lane])
            continue;

        int x = x_coords[lane];
        int y = y_coords[lane];

        if (x < 0 || x >= (int)desc->width || y < 0 || y >= (int)desc->height)
            continue;

        uint32_t idx = ((uint32_t)y * desc->width + (uint32_t)x) * channels;

        switch (channels)
        {
            case 1:
                pixels[idx] = color_to_u8(in_r[lane]);
                break;
            case 2:
                pixels[idx + 0] = color_to_u8(in_r[lane]);
                pixels[idx + 1] = color_to_u8(in_g[lane]);
                break;
            case 3:
                pixels[idx + 0] = color_to_u8(in_r[lane]);
                pixels[idx + 1] = color_to_u8(in_g[lane]);
                pixels[idx + 2] = color_to_u8(in_b[lane]);
                break;
            case 4:
            default:
                pixels[idx + 0] = color_to_u8(in_r[lane]);
                pixels[idx + 1] = color_to_u8(in_g[lane]);
                pixels[idx + 2] = color_to_u8(in_b[lane]);
                pixels[idx + 3] = color_to_u8(in_a[lane]);
                break;
        }
    }
}

void handle_op_image_read(JitContext *ctx, uint32_t res_id, uint32_t type_id, uint32_t *operands)
{
    (void)type_id;
    handle_op_image_fetch(ctx, res_id, operands);
}

void handle_op_image_write(JitContext *ctx, uint32_t *operands)
{
    uint32_t image_id = operands[0];
    uint32_t coords_id = operands[1];
    uint32_t texel_id = operands[2];

    LLVMValueRef image_val = get_val(ctx, image_id);
    if (!image_val)
    {
        image_val = LLVMConstNull(ctx->ptr_type);
    }

    LLVMValueRef coords_val = get_val(ctx, coords_id);
    LLVMValueRef texel_val = get_val(ctx, texel_id);

    LLVMValueRef x_coords = LLVMBuildExtractValue(ctx->builder, coords_val, 0, "write_x");
    LLVMValueRef y_coords = LLVMBuildExtractValue(ctx->builder, coords_val, 1, "write_y");

    LLVMTypeRef x_type = LLVMTypeOf(x_coords);
    if (LLVMGetTypeKind(x_type) == LLVMVectorTypeKind &&
        (LLVMGetTypeKind(LLVMGetElementType(x_type)) == LLVMFloatTypeKind ||
         LLVMGetTypeKind(LLVMGetElementType(x_type)) == LLVMDoubleTypeKind))
    {
        x_coords = LLVMBuildFPToSI(ctx->builder, x_coords, LLVMVectorType(ctx->int_type, SIMT_WIDTH), "x_f2i");
        y_coords = LLVMBuildFPToSI(ctx->builder, y_coords, LLVMVectorType(ctx->int_type, SIMT_WIDTH), "y_f2i");
    }

    LLVMValueRef in_r = NULL, in_g = NULL, in_b = NULL, in_a = NULL;
    LLVMTypeRef texel_type = LLVMTypeOf(texel_val);
    if (LLVMGetTypeKind(texel_type) == LLVMArrayTypeKind)
    {
        uint32_t num_comps = LLVMGetArrayLength(texel_type);
        in_r = LLVMBuildExtractValue(ctx->builder, texel_val, 0, "in_r");
        in_g = num_comps > 1 ? LLVMBuildExtractValue(ctx->builder, texel_val, 1, "in_g") : in_r;
        in_b = num_comps > 2 ? LLVMBuildExtractValue(ctx->builder, texel_val, 2, "in_b") : in_r;
        in_a = num_comps > 3 ? LLVMBuildExtractValue(ctx->builder, texel_val, 3, "in_a") : in_r;
    }
    else
    {
        in_r = in_g = in_b = in_a = texel_val;
    }

    LLVMTypeRef ptr_type = ctx->ptr_type;
    LLVMTypeRef vec_float_type = ctx->vec_float_type;
    LLVMTypeRef vec_int_type = LLVMVectorType(ctx->int_type, SIMT_WIDTH);

    LLVMValueRef x_alloca = LLVMBuildAlloca(ctx->builder, vec_int_type, "wx_alloca");
    LLVMValueRef y_alloca = LLVMBuildAlloca(ctx->builder, vec_int_type, "wy_alloca");
    LLVMValueRef r_alloca = LLVMBuildAlloca(ctx->builder, vec_float_type, "wr_alloca");
    LLVMValueRef g_alloca = LLVMBuildAlloca(ctx->builder, vec_float_type, "wg_alloca");
    LLVMValueRef b_alloca = LLVMBuildAlloca(ctx->builder, vec_float_type, "wb_alloca");
    LLVMValueRef a_alloca = LLVMBuildAlloca(ctx->builder, vec_float_type, "wa_alloca");
    LLVMValueRef mask_alloca = LLVMBuildAlloca(ctx->builder, vec_int_type, "mask_alloca");

    LLVMSetAlignment(x_alloca, 64);
    LLVMSetAlignment(y_alloca, 64);
    LLVMSetAlignment(r_alloca, 64);
    LLVMSetAlignment(g_alloca, 64);
    LLVMSetAlignment(b_alloca, 64);
    LLVMSetAlignment(a_alloca, 64);
    LLVMSetAlignment(mask_alloca, 64);

    LLVMBuildStore(ctx->builder, x_coords, x_alloca);
    LLVMBuildStore(ctx->builder, y_coords, y_alloca);
    LLVMBuildStore(ctx->builder, in_r, r_alloca);
    LLVMBuildStore(ctx->builder, in_g, g_alloca);
    LLVMBuildStore(ctx->builder, in_b, b_alloca);
    LLVMBuildStore(ctx->builder, in_a, a_alloca);

    LLVMValueRef mask_i32 = LLVMBuildZExt(ctx->builder, ctx->emask, vec_int_type, "mask_i32");
    LLVMBuildStore(ctx->builder, mask_i32, mask_alloca);

    LLVMValueRef write_func = LLVMGetNamedFunction(ctx->module, "image_write_2d_simt");
    LLVMTypeRef param_types[8] = {
        ptr_type, ptr_type, ptr_type, ptr_type, ptr_type, ptr_type, ptr_type, ptr_type
    };
    LLVMTypeRef func_type = LLVMFunctionType(LLVMVoidTypeInContext(ctx->context), param_types, 8, 0);

    if (!write_func)
    {
        write_func = LLVMAddFunction(ctx->module, "image_write_2d_simt", func_type);
        if (ctx->engine)
        {
            LLVMAddGlobalMapping(ctx->engine, write_func, (void*)&image_write_2d_simt);
        }
    }

    LLVMValueRef args[8] = {
        image_val,
        x_alloca,
        y_alloca,
        r_alloca,
        g_alloca,
        b_alloca,
        a_alloca,
        mask_alloca
    };

    LLVMBuildCall2(ctx->builder, func_type, write_func, args, 8, "");
}