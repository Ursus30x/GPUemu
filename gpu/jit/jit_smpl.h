#ifndef JIT_SMPL_H
#define JIT_SMPL_H

#include "jit.h"
#include "jit_flow.h"

#include <stdio.h>
#include <string.h>

typedef enum {
    FILTER_NEAREST = 0,
    FILTER_LINEAR  = 1
} FilterMode;

typedef enum {
    WRAP_REPEAT = 0,
    WRAP_CLAMP  = 1
} WrapMode;

typedef struct {
    void*      data;       
    uint32_t   width;
    uint32_t   height;
    uint32_t   channels; 
    FilterMode filter;
    WrapMode   wrap;
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
void handle_op_image_read(JitContext *ctx, uint32_t res_id, uint32_t type_id, uint32_t *operands);
void handle_op_image_write(JitContext *ctx, uint32_t *operands);

void sample_texture_2d_simt(
    const TextureSamplerDescriptor *desc,
    const float *u_coords,
    const float *v_coords,
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

void image_write_2d_simt(
    const TextureSamplerDescriptor *desc,
    const int32_t *x_coords,
    const int32_t *y_coords,
    const float *in_r,
    const float *in_g,
    const float *in_b,
    const float *in_a,
    const int32_t *mask
);

#endif