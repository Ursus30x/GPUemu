#ifndef JIT_EXT_H
#define JIT_EXT_H

#include "jit.h"

void create_glsl_std_450_map(JitContext* ctx);
void handle_op_ext_instr(JitContext* ctx, uint32_t res_id, uint32_t* operands);

void handle_ext_sin(JitContext* ctx, uint32_t res_id, uint32_t* operands);     
void handle_ext_cos(JitContext* ctx, uint32_t res_id, uint32_t* operands);      
void handle_ext_sqrt(JitContext* ctx, uint32_t res_id, uint32_t* operands);     
void handle_ext_pow(JitContext* ctx, uint32_t res_id, uint32_t* operands);     
void handle_ext_atan2(JitContext* ctx, uint32_t res_id, uint32_t* operands);    
void handle_ext_log(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_fabs(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_fmax(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_fmin(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_fclamp(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_smoothstep(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_fmix(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_fsign(JitContext* ctx, uint32_t res_id, uint32_t* operands); 
void handle_ext_step(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_length(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_normalize(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_reflect(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_distance(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_cross(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_refract(JitContext* ctx, uint32_t res_id, uint32_t* operands);   
void handle_ext_tan(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_exp(JitContext* ctx, uint32_t res_id, uint32_t* operands);

void handle_ext_round(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_round_even(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_trunc(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_floor(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_ceil(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_fract(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_fma(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_modf(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_modf_struct(JitContext* ctx, uint32_t res_id, uint32_t* operands);

void handle_ext_radians(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_degrees(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_asin(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_acos(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_atan(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_sinh(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_cosh(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_tanh(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_asinh(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_acosh(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_atanh(JitContext* ctx, uint32_t res_id, uint32_t* operands);

void handle_ext_exp2(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_log2(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_inverse_sqrt(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_frexp(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_frexp_struct(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_ldexp(JitContext* ctx, uint32_t res_id, uint32_t* operands);

void handle_ext_sabs(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_ssign(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_umin(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_smin(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_umax(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_smax(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_uclamp(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_sclamp(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_imix(JitContext* ctx, uint32_t res_id, uint32_t* operands);

void handle_ext_nmin(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_nmax(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_nclamp(JitContext* ctx, uint32_t res_id, uint32_t* operands);

void handle_ext_determinant(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_matrix_inverse(JitContext* ctx, uint32_t res_id, uint32_t* operands);

void handle_ext_face_forward(JitContext* ctx, uint32_t res_id, uint32_t* operands);

void handle_ext_pack_snorm4x8(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_pack_unorm4x8(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_pack_snorm2x16(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_pack_unorm2x16(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_pack_half2x16(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_pack_double2x32(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_unpack_snorm2x16(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_unpack_unorm2x16(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_unpack_half2x16(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_unpack_snorm4x8(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_unpack_unorm4x8(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_unpack_double2x32(JitContext* ctx, uint32_t res_id, uint32_t* operands);

void handle_ext_find_ilsb(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_find_smsb(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_find_umsb(JitContext* ctx, uint32_t res_id, uint32_t* operands);

void handle_ext_interpolate_at_centroid(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_interpolate_at_sample(JitContext* ctx, uint32_t res_id, uint32_t* operands);
void handle_ext_interpolate_at_offset(JitContext* ctx, uint32_t res_id, uint32_t* operands);

#endif
