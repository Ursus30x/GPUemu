#ifndef JIT_SMPL_H
#define JIT_SMPL_H

#include "jit.h"
#include "jit_flow.h"

#include <stdio.h>
#include <string.h>

#define MAX_MIP_LEVELS 14
typedef enum {
    FILTER_NEAREST                = 0,
    FILTER_LINEAR                 = 1,
    FILTER_NEAREST_MIPMAP_NEAREST = 2,
    FILTER_LINEAR_MIPMAP_NEAREST  = 3,
    FILTER_NEAREST_MIPMAP_LINEAR  = 4,
    FILTER_LINEAR_MIPMAP_LINEAR   = 5  // Trilinear filtering
} FilterMode;

typedef enum {
    TEXTURE_DIM_2D = 0,
    TEXTURE_DIM_3D = 1,
    TEXTURE_DIM_CUBE = 2
} GpuTextureDimension;

typedef enum {
    WRAP_REPEAT = 0,
    WRAP_CLAMP  = 1,
    WRAP_MIRROR = 2
} WrapMode;

typedef struct {
    void*               data;
    void*               mip_addr[MAX_MIP_LEVELS]; // VRAM relative offsets for Mip levels 0..13
    uint32_t            width;
    uint32_t            height;
    uint32_t            depth;
    uint32_t            channels;
    GpuTextureDimension dimension;  
    FilterMode          filter;
    WrapMode            wrap;
    WrapMode            wrap_u;
    WrapMode            wrap_v;
    WrapMode            wrap_w;
    uint32_t            num_mip_levels;          // Number of mipmap levels present (1 = base level only)
    float               max_anisotropy;          // Max anisotropic ratio (1.0f = disabled, up to 16.0f)
    float               min_lod;                 // Minimum clamp for LOD (e.g. 0.0f)
    float               max_lod;                 // Maximum clamp for LOD (e.g. 13.0f)
    float               lod_bias;                // User LOD bias (added to computed LOD)
} TextureSamplerDescriptor;

void handle_op_type_image(JitContext *ctx, uint32_t res_id, uint32_t *operands);
void handle_op_type_sampler(JitContext *ctx, uint32_t res_id);
void handle_op_type_sampled_image(JitContext *ctx, uint32_t res_id, uint32_t image_type_id);
void handle_op_sampled_image(JitContext *ctx, uint32_t res_id, uint32_t *operands);
void handle_op_image(JitContext *ctx, uint32_t res_id, uint32_t *operands);
void handle_op_image_sample_implicit_lod(JitContext *ctx, uint32_t res_id, uint32_t *operands);
void handle_op_image_sample_explicit_lod(JitContext *ctx, uint32_t res_id, uint32_t *operands);
void handle_op_image_fetch(JitContext *ctx, uint32_t res_id, uint32_t *operands);
void handle_op_image_query_size_lod(JitContext *ctx, uint32_t res_id, uint32_t *operands);
void handle_op_image_query_size(JitContext *ctx, uint32_t res_id, uint32_t *operands);

void sample_texture_generic_simt(
    const TextureSamplerDescriptor *desc,
    const float *u_coords,
    const float *v_coords,
    const float *w_coords,
    const float *du_dx, const float *dv_dx, const float *dw_dx,
    const float *du_dy, const float *dv_dy, const float *dw_dy,
    const float *explicit_lod,
    float *out_r,
    float *out_g,
    float *out_b,
    float *out_a
);

void sample_texture_2d_simt(
    const TextureSamplerDescriptor *desc,
    const float *u_coords,
    const float *v_coords,
    float *out_r,
    float *out_g,
    float *out_b,
    float *out_a
);

void fetch_texture_generic_simt(
    const TextureSamplerDescriptor *desc,
    const int32_t *x_coords,
    const int32_t *y_coords,
    const int32_t *z_coords,
    const int32_t *lod_coords,
    float *out_r,
    float *out_g,
    float *out_b,
    float *out_a
);

void fetch_texture_2d_simt(
    const TextureSamplerDescriptor *desc,
    const int32_t *x_coords,
    const int32_t *y_coords,
    float *out_r,
    float *out_g,
    float *out_b,
    float *out_a
);

#endif