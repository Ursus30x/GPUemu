#include "jit_smpl.h"
#include <math.h>
#include <stddef.h>

static inline float clamp_float(float v, float min_v, float max_v)
{
    if (v < min_v) return min_v;
    if (v > max_v) return max_v;
    return v;
}

static inline float wrap_coordinate(float coord, WrapMode wrap)
{
    if (wrap == WRAP_REPEAT)
    {
        float c = coord - floorf(coord);
        if (c < 0.0f) c += 1.0f;
        return c;
    }
    else if (wrap == WRAP_MIRROR)
    {
        float c = coord - floorf(coord);
        int itemp = (int)floorf(coord);
        if (itemp % 2 != 0) c = 1.0f - c;
        if (c < 0.0f) c += 1.0f;
        return c;
    }
    else // WRAP_CLAMP or default (0)
    {
        return clamp_float(coord, 0.0f, 1.0f);
    }
}
static inline void read_texel_color_data(
    const void *data, uint32_t width, uint32_t height, uint32_t depth,
    uint32_t channels, int x, int y, int z,
    float *r, float *g, float *b, float *a
)
{
    if (!data || width == 0 || height == 0 || depth == 0)
    {
        *r = 0.0f; *g = 0.0f; *b = 0.0f; *a = 1.0f;
        return;
    }

    if (x < 0) x = 0;
    if (x >= (int)width) x = (int)width - 1;
    if (y < 0) y = 0;
    if (y >= (int)height) y = (int)height - 1;
    if (z < 0) z = 0;
    if (z >= (int)depth) z = (int)depth - 1;

    if (channels == 0) channels = 4;
    const uint8_t *pixels = (const uint8_t *)data;
    uint32_t idx = ((uint32_t)z * height * width + (uint32_t)y * width + (uint32_t)x) * channels;

    switch (channels)
    {
        case 1:
        {
            float v = pixels[idx] / 255.0f;
            *r = v; *g = v; *b = v; *a = 1.0f;
            break;
        }
        case 2:
        {
            *r = pixels[idx + 0] / 255.0f;
            *g = pixels[idx + 1] / 255.0f;
            *b = 0.0f; *a = 1.0f;
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

static inline void sample_texel_3d_single(
    const void *data, uint32_t width, uint32_t height, uint32_t depth,
    uint32_t channels, WrapMode wrap_u, WrapMode wrap_v, WrapMode wrap_w,
    bool is_linear, float u, float v, float w,
    float *r, float *g, float *b, float *a
)
{
    float u_norm = wrap_coordinate(u, wrap_u);
    float v_norm = wrap_coordinate(v, wrap_v);
    float w_norm = wrap_coordinate(w, wrap_w);

    if (is_linear)
    {
        float u_tex = u_norm * (float)width - 0.5f;
        float v_tex = v_norm * (float)height - 0.5f;
        float w_tex = w_norm * (float)depth - 0.5f;

        int x0 = (int)floorf(u_tex);
        int y0 = (int)floorf(v_tex);
        int z0 = (int)floorf(w_tex);

        int x1 = x0 + 1;
        int y1 = y0 + 1;
        int z1 = z0 + 1;

        float fx = u_tex - (float)x0;
        float fy = v_tex - (float)y0;
        float fz = w_tex - (float)z0;

        if (wrap_u == WRAP_REPEAT) {
            x0 = ((x0 % (int)width) + (int)width) % (int)width;
            x1 = ((x1 % (int)width) + (int)width) % (int)width;
        } else {
            if (x0 < 0) { x0 = 0; }
            if (x0 >= (int)width) { x0 = (int)width - 1; }
            if (x1 < 0) { x1 = 0; }
            if (x1 >= (int)width) { x1 = (int)width - 1; }
        }

        if (wrap_v == WRAP_REPEAT) {
            y0 = ((y0 % (int)height) + (int)height) % (int)height;
            y1 = ((y1 % (int)height) + (int)height) % (int)height;
        } else {
            if (y0 < 0) { y0 = 0; }
            if (y0 >= (int)height) { y0 = (int)height - 1; }
            if (y1 < 0) { y1 = 0; }
            if (y1 >= (int)height) { y1 = (int)height - 1; }
        }

        if (wrap_w == WRAP_REPEAT) {
            z0 = ((z0 % (int)depth) + (int)depth) % (int)depth;
            z1 = ((z1 % (int)depth) + (int)depth) % (int)depth;
        } else {
            if (z0 < 0) { z0 = 0; }
            if (z0 >= (int)depth) { z0 = (int)depth - 1; }
            if (z1 < 0) { z1 = 0; }
            if (z1 >= (int)depth) { z1 = (int)depth - 1; }
        }

        float c000[4], c100[4], c010[4], c110[4];
        float c001[4], c101[4], c011[4], c111[4];

        read_texel_color_data(data, width, height, depth, channels, x0, y0, z0, &c000[0], &c000[1], &c000[2], &c000[3]);
        read_texel_color_data(data, width, height, depth, channels, x1, y0, z0, &c100[0], &c100[1], &c100[2], &c100[3]);
        read_texel_color_data(data, width, height, depth, channels, x0, y1, z0, &c010[0], &c010[1], &c010[2], &c010[3]);
        read_texel_color_data(data, width, height, depth, channels, x1, y1, z0, &c110[0], &c110[1], &c110[2], &c110[3]);

        read_texel_color_data(data, width, height, depth, channels, x0, y0, z1, &c001[0], &c001[1], &c001[2], &c001[3]);
        read_texel_color_data(data, width, height, depth, channels, x1, y0, z1, &c101[0], &c101[1], &c101[2], &c101[3]);
        read_texel_color_data(data, width, height, depth, channels, x0, y1, z1, &c011[0], &c011[1], &c011[2], &c011[3]);
        read_texel_color_data(data, width, height, depth, channels, x1, y1, z1, &c111[0], &c111[1], &c111[2], &c111[3]);

        float c00 = c000[0] * (1.0f - fx) + c100[0] * fx;
        float c10 = c010[0] * (1.0f - fx) + c110[0] * fx;
        float c01 = c001[0] * (1.0f - fx) + c101[0] * fx;
        float c11 = c011[0] * (1.0f - fx) + c111[0] * fx;

        float c0 = c00 * (1.0f - fy) + c10 * fy;
        float c1 = c01 * (1.0f - fy) + c11 * fy;
        *r = c0 * (1.0f - fz) + c1 * fz;

        c00 = c000[1] * (1.0f - fx) + c100[1] * fx;
        c10 = c010[1] * (1.0f - fx) + c110[1] * fx;
        c01 = c001[1] * (1.0f - fx) + c101[1] * fx;
        c11 = c011[1] * (1.0f - fx) + c111[1] * fx;

        c0 = c00 * (1.0f - fy) + c10 * fy;
        c1 = c01 * (1.0f - fy) + c11 * fy;
        *g = c0 * (1.0f - fz) + c1 * fz;

        c00 = c000[2] * (1.0f - fx) + c100[2] * fx;
        c10 = c010[2] * (1.0f - fx) + c110[2] * fx;
        c01 = c001[2] * (1.0f - fx) + c101[2] * fx;
        c11 = c011[2] * (1.0f - fx) + c111[2] * fx;

        c0 = c00 * (1.0f - fy) + c10 * fy;
        c1 = c01 * (1.0f - fy) + c11 * fy;
        *b = c0 * (1.0f - fz) + c1 * fz;

        c00 = c000[3] * (1.0f - fx) + c100[3] * fx;
        c10 = c010[3] * (1.0f - fx) + c110[3] * fx;
        c01 = c001[3] * (1.0f - fx) + c101[3] * fx;
        c11 = c011[3] * (1.0f - fx) + c111[3] * fx;

        c0 = c00 * (1.0f - fy) + c10 * fy;
        c1 = c01 * (1.0f - fy) + c11 * fy;
        *a = c0 * (1.0f - fz) + c1 * fz;
    }
    else // Nearest
    {
        int x = (int)floorf(u_norm * (float)width);
        int y = (int)floorf(v_norm * (float)height);
        int z = (int)floorf(w_norm * (float)depth);

        if (wrap_u == WRAP_REPEAT) {
            x = ((x % (int)width) + (int)width) % (int)width;
        } else {
            if (x < 0) { x = 0; }
            if (x >= (int)width) { x = (int)width - 1; }
        }

        if (wrap_v == WRAP_REPEAT) {
            y = ((y % (int)height) + (int)height) % (int)height;
        } else {
            if (y < 0) { y = 0; }
            if (y >= (int)height) { y = (int)height - 1; }
        }

        if (wrap_w == WRAP_REPEAT) {
            z = ((z % (int)depth) + (int)depth) % (int)depth;
        } else {
            if (z < 0) { z = 0; }
            if (z >= (int)depth) { z = (int)depth - 1; }
        }

        read_texel_color_data(data, width, height, depth, channels, x, y, z, r, g, b, a);
    }
}
static inline void sample_texel_2d_single_data(
    const void *data, uint32_t width, uint32_t height,
    uint32_t channels, WrapMode wrap_u, WrapMode wrap_v,
    bool is_linear, float u, float v,
    float *r, float *g, float *b, float *a
)
{
    float u_norm = wrap_coordinate(u, wrap_u);
    float v_norm = wrap_coordinate(v, wrap_v);

    if (is_linear)
    {
        float u_tex = u_norm * (float)width - 0.5f;
        float v_tex = v_norm * (float)height - 0.5f;

        int x0 = (int)floorf(u_tex);
        int y0 = (int)floorf(v_tex);
        int x1 = x0 + 1;
        int y1 = y0 + 1;

        float fx = u_tex - (float)x0;
        float fy = v_tex - (float)y0;

        if (wrap_u == WRAP_REPEAT) {
            x0 = ((x0 % (int)width) + (int)width) % (int)width;
            x1 = ((x1 % (int)width) + (int)width) % (int)width;
        } else {
            if (x0 < 0) { x0 = 0; }
            if (x0 >= (int)width) { x0 = (int)width - 1; }
            if (x1 < 0) { x1 = 0; }
            if (x1 >= (int)width) { x1 = (int)width - 1; }
        }

        if (wrap_v == WRAP_REPEAT) {
            y0 = ((y0 % (int)height) + (int)height) % (int)height;
            y1 = ((y1 % (int)height) + (int)height) % (int)height;
        } else {
            if (y0 < 0) { y0 = 0; }
            if (y0 >= (int)height) { y0 = (int)height - 1; }
            if (y1 < 0) { y1 = 0; }
            if (y1 >= (int)height) { y1 = (int)height - 1; }
        }

        float r00, g00, b00, a00;
        float r10, g10, b10, a10;
        float r01, g01, b01, a01;
        float r11, g11, b11, a11;

        read_texel_color_data(data, width, height, 1, channels, x0, y0, 0, &r00, &g00, &b00, &a00);
        read_texel_color_data(data, width, height, 1, channels, x1, y0, 0, &r10, &g10, &b10, &a10);
        read_texel_color_data(data, width, height, 1, channels, x0, y1, 0, &r01, &g01, &b01, &a01);
        read_texel_color_data(data, width, height, 1, channels, x1, y1, 0, &r11, &g11, &b11, &a11);

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
    else // Nearest
    {
        int x = (int)floorf(u_norm * (float)width);
        int y = (int)floorf(v_norm * (float)height);

        if (wrap_u == WRAP_REPEAT) {
            x = ((x % (int)width) + (int)width) % (int)width;
        } else {
            if (x < 0) { x = 0; }
            if (x >= (int)width) { x = (int)width - 1; }
        }

        if (wrap_v == WRAP_REPEAT) {
            y = ((y % (int)height) + (int)height) % (int)height;
        } else {
            if (y < 0) { y = 0; }
            if (y >= (int)height) { y = (int)height - 1; }
        }

        read_texel_color_data(data, width, height, 1, channels, x, y, 0, r, g, b, a);
    }
}
static inline void sample_single_level(
    const TextureSamplerDescriptor *desc,
    int level,
    float u, float v, float w,
    float *r, float *g, float *b, float *a
)
{
    const void *data = NULL;
    if (level < MAX_MIP_LEVELS && desc->mip_addr[level] != NULL) {
        data = desc->mip_addr[level];
    } else {
        data = desc->data;
        level = 0;
    }

    uint32_t w0 = desc->width;
    uint32_t h0 = desc->height;
    uint32_t d0 = desc->depth > 0 ? desc->depth : 1;

    uint32_t width  = w0 >> level; if (width == 0) width = 1;
    uint32_t height = h0 >> level; if (height == 0) height = 1;
    uint32_t depth  = d0 >> level; if (depth == 0) depth = 1;

    if (!data) {
        *r = 0.0f; *g = 0.0f; *b = 0.0f; *a = 1.0f;
        return;
    }

    bool is_3d = (desc->dimension == TEXTURE_DIM_3D || desc->depth > 1);

    WrapMode wrap_u = desc->wrap_u ? desc->wrap_u : (desc->wrap ? desc->wrap : WRAP_CLAMP);
    WrapMode wrap_v = desc->wrap_v ? desc->wrap_v : (desc->wrap ? desc->wrap : WRAP_CLAMP);
    WrapMode wrap_w = desc->wrap_w ? desc->wrap_w : (desc->wrap ? desc->wrap : WRAP_CLAMP);

    FilterMode filter = desc->filter;
    bool is_linear = (filter == FILTER_LINEAR ||
                      filter == FILTER_LINEAR_MIPMAP_NEAREST ||
                      filter == FILTER_LINEAR_MIPMAP_LINEAR);

    if (is_3d)
    {
        if (!is_linear)
        {
            // Fast path: nearest 3D — inline coordinate wrapping + single texel read
            float u_norm = wrap_coordinate(u, wrap_u);
            float v_norm = wrap_coordinate(v, wrap_v);
            float w_norm = wrap_coordinate(w, wrap_w);

            int ix = (int)floorf(u_norm * (float)width);
            int iy = (int)floorf(v_norm * (float)height);
            int iz = (int)floorf(w_norm * (float)depth);

            if (ix < 0) ix = 0; else if (ix >= (int)width) ix = (int)width - 1;
            if (iy < 0) iy = 0; else if (iy >= (int)height) iy = (int)height - 1;
            if (iz < 0) iz = 0; else if (iz >= (int)depth) iz = (int)depth - 1;

            read_texel_color_data(data, width, height, depth, desc->channels, ix, iy, iz, r, g, b, a);
        }
        else
        {
            sample_texel_3d_single(data, width, height, depth, desc->channels, wrap_u, wrap_v, wrap_w, is_linear, u, v, w, r, g, b, a);
        }
    }
    else
    {
        sample_texel_2d_single_data(data, width, height, desc->channels, wrap_u, wrap_v, is_linear, u, v, r, g, b, a);
    }
}
static inline void sample_mipmap_level(
    const TextureSamplerDescriptor *desc,
    float u, float v, float w,
    float lod,
    float *r, float *g, float *b, float *a
)
{
    if (!desc || (!desc->data && !desc->mip_addr[0]) || desc->width == 0 || desc->height == 0)
    {
        *r = 0.0f; *g = 0.0f; *b = 0.0f; *a = 1.0f;
        return;
    }

    uint32_t num_levels = desc->num_mip_levels > 0 ? desc->num_mip_levels : 1;
    FilterMode filter = desc->filter;

    bool is_mipmap_filter = (filter == FILTER_NEAREST_MIPMAP_NEAREST ||
                             filter == FILTER_LINEAR_MIPMAP_NEAREST  ||
                             filter == FILTER_NEAREST_MIPMAP_LINEAR  ||
                             filter == FILTER_LINEAR_MIPMAP_LINEAR);

    if (!is_mipmap_filter || num_levels <= 1 || lod <= 0.0f)
    {
        sample_single_level(desc, 0, u, v, w, r, g, b, a);
        return;
    }

    if (filter == FILTER_NEAREST_MIPMAP_NEAREST || filter == FILTER_LINEAR_MIPMAP_NEAREST)
    {
        int level = (int)roundf(lod);
        if (level < 0) level = 0;
        if (level >= (int)num_levels) level = (int)num_levels - 1;
        sample_single_level(desc, level, u, v, w, r, g, b, a);
    }
    else // FILTER_NEAREST_MIPMAP_LINEAR or FILTER_LINEAR_MIPMAP_LINEAR
    {
        int level0 = (int)floorf(lod);
        if (level0 < 0) level0 = 0;
        if (level0 >= (int)num_levels) level0 = (int)num_levels - 1;

        int level1 = level0 + 1;
        if (level1 >= (int)num_levels) level1 = (int)num_levels - 1;

        float f = lod - (float)level0;
        if (f < 0.0f) f = 0.0f;
        if (f > 1.0f) f = 1.0f;

        float r0, g0, b0, a0;
        float r1, g1, b1, a1;
        sample_single_level(desc, level0, u, v, w, &r0, &g0, &b0, &a0);

        if (level0 == level1 || f == 0.0f)
        {
            *r = r0; *g = g0; *b = b0; *a = a0;
        }
        else
        {
            sample_single_level(desc, level1, u, v, w, &r1, &g1, &b1, &a1);
            *r = r0 * (1.0f - f) + r1 * f;
            *g = g0 * (1.0f - f) + g1 * f;
            *b = b0 * (1.0f - f) + b1 * f;
            *a = a0 * (1.0f - f) + a1 * f;
        }
    }
}
void sample_texture_generic_simt(
    const TextureSamplerDescriptor *desc,
    const float *u_coords,
    const float *v_coords,
    const float *w_coords,
    const float *du_dx, const float *dv_dx, const float *dw_dx,
    const float *du_dy, const float *dv_dy, const float *dw_dy,
    const float *explicit_lod,
    float *out_r, float *out_g, float *out_b, float *out_a
)
{
    if (!desc || (!desc->data && !desc->mip_addr[0]) || desc->width == 0 || desc->height == 0)
    {
        for (int lane = 0; lane < SIMT_WIDTH; lane++)
        {
            out_r[lane] = 0.0f;
            out_g[lane] = 0.0f;
            out_b[lane] = 0.0f;
            out_a[lane] = 1.0f;
        }
        return;
    }

    uint32_t num_levels = desc->num_mip_levels > 0 ? desc->num_mip_levels : 1;
    float max_aniso = desc->max_anisotropy > 1.0f ? desc->max_anisotropy : 1.0f;
    bool has_mipmaps = num_levels > 1;
    bool has_aniso = max_aniso > 1.0f;

    // Fast path: no mipmaps and no anisotropy — skip all derivative/LOD math
    if (!has_mipmaps && !has_aniso && explicit_lod == NULL)
    {
        for (int lane = 0; lane < SIMT_WIDTH; lane++)
        {
            float u = u_coords ? u_coords[lane] : 0.0f;
            float v = v_coords ? v_coords[lane] : 0.0f;
            float w = w_coords ? w_coords[lane] : 0.0f;
            sample_single_level(desc, 0, u, v, w,
                &out_r[lane], &out_g[lane], &out_b[lane], &out_a[lane]);
        }
        return;
    }

    float W0 = (float)desc->width;
    float H0 = (float)desc->height;
    float D0 = (float)(desc->depth > 0 ? desc->depth : 1);

    for (int lane = 0; lane < SIMT_WIDTH; lane++)
    {
        float u = u_coords ? u_coords[lane] : 0.0f;
        float v = v_coords ? v_coords[lane] : 0.0f;
        float w = w_coords ? w_coords[lane] : 0.0f;

        float lod;
        int N = 1;
        float dmaj_u = 0.0f, dmaj_v = 0.0f, dmaj_w = 0.0f;

        if (explicit_lod != NULL)
        {
            float lod_raw = explicit_lod[lane] + desc->lod_bias;
            float max_level_avail = (float)(num_levels - 1);
            float clamped_max_lod = fminf(desc->max_lod, max_level_avail);
            lod = clamp_float(lod_raw, desc->min_lod, clamped_max_lod);
        }
        else
        {
            float du_x = du_dx ? du_dx[lane] : 0.0f;
            float dv_x = dv_dx ? dv_dx[lane] : 0.0f;
            float dw_x = dw_dx ? dw_dx[lane] : 0.0f;

            float du_y = du_dy ? du_dy[lane] : 0.0f;
            float dv_y = dv_dy ? dv_dy[lane] : 0.0f;
            float dw_y = dw_dy ? dw_dy[lane] : 0.0f;

            // Spatial derivative vectors in texel space
            float Jx_x = du_x * W0;
            float Jx_y = dv_x * H0;
            float Jx_z = dw_x * D0;

            float Jy_x = du_y * W0;
            float Jy_y = dv_y * H0;
            float Jy_z = dw_y * D0;

            float Px2 = Jx_x * Jx_x + Jx_y * Jx_y + Jx_z * Jx_z;
            float Py2 = Jy_x * Jy_x + Jy_y * Jy_y + Jy_z * Jy_z;

            float lod_raw;

            if (has_aniso)
            {
                // Full anisotropic: eigenvalue decomposition
                float Pxy = Jx_x * Jy_x + Jx_y * Jy_y + Jx_z * Jy_z;

                float A = Px2 + Py2;
                float B = Px2 - Py2;
                float C = 2.0f * Pxy;

                float sq = sqrtf(fmaxf(0.0f, B * B + C * C));
                float lambda_max = sqrtf(fmaxf(0.0f, 0.5f * (A + sq)));
                float lambda_min = sqrtf(fmaxf(0.0f, 0.5f * (A - sq)));

                float aniso_ratio = lambda_max / fmaxf(lambda_min, 1e-5f);
                N = (int)ceilf(aniso_ratio);
                if (N < 1) N = 1;
                if (N > (int)max_aniso) N = (int)max_aniso;

                lod_raw = (lambda_max > 0.0f)
                    ? log2f(lambda_max / (float)N) + desc->lod_bias
                    : desc->lod_bias;

                // Anisotropic Major Axis Direction
                if (Px2 >= Py2)
                {
                    dmaj_u = du_x;
                    dmaj_v = dv_x;
                    dmaj_w = dw_x;
                }
                else
                {
                    dmaj_u = du_y;
                    dmaj_v = dv_y;
                    dmaj_w = dw_y;
                }
            }
            else
            {
                // Isotropic: use max footprint, skip eigenvalue decomposition
                float max_p2 = fmaxf(Px2, Py2);
                lod_raw = (max_p2 > 0.0f)
                    ? 0.5f * log2f(max_p2) + desc->lod_bias
                    : desc->lod_bias;
            }

            float max_level_avail = (float)(num_levels - 1);
            float clamped_max_lod = fminf(desc->max_lod, max_level_avail);
            lod = clamp_float(lod_raw, desc->min_lod, clamped_max_lod);
        }

        if (N <= 1)
        {
            // No anisotropic multi-sampling needed
            sample_mipmap_level(desc, u, v, w, lod,
                &out_r[lane], &out_g[lane], &out_b[lane], &out_a[lane]);
        }
        else
        {
            float r_acc = 0.0f, g_acc = 0.0f, b_acc = 0.0f, a_acc = 0.0f;
            float inv_N = 1.0f / (float)N;
            for (int k = 0; k < N; k++)
            {
                float tk = ((float)k + 0.5f) * inv_N - 0.5f;
                float uk = u + tk * dmaj_u;
                float vk = v + tk * dmaj_v;
                float wk = w + tk * dmaj_w;

                float r_s, g_s, b_s, a_s;
                sample_mipmap_level(desc, uk, vk, wk, lod, &r_s, &g_s, &b_s, &a_s);

                r_acc += r_s;
                g_acc += g_s;
                b_acc += b_s;
                a_acc += a_s;
            }

            out_r[lane] = r_acc * inv_N;
            out_g[lane] = g_acc * inv_N;
            out_b[lane] = b_acc * inv_N;
            out_a[lane] = a_acc * inv_N;
        }
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
    sample_texture_generic_simt(
        desc, u_coords, v_coords, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, out_r, out_g, out_b, out_a
    );
}

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
)
{
    for (int lane = 0; lane < SIMT_WIDTH; lane++)
    {
        if (!desc || (!desc->data && !desc->mip_addr[0]) || desc->width == 0 || desc->height == 0)
        {
            out_r[lane] = 0.0f;
            out_g[lane] = 0.0f;
            out_b[lane] = 0.0f;
            out_a[lane] = 1.0f;
            continue;
        }

        int lod = lod_coords ? lod_coords[lane] : 0;
        uint32_t num_levels = desc->num_mip_levels > 0 ? desc->num_mip_levels : 1;
        if (lod < 0) lod = 0;
        if (lod >= (int)num_levels) lod = (int)num_levels - 1;

        uint32_t width  = desc->width >> lod;  if (width == 0) width = 1;
        uint32_t height = desc->height >> lod; if (height == 0) height = 1;
        uint32_t depth  = (desc->depth > 0 ? desc->depth : 1) >> lod; if (depth == 0) depth = 1;

        const void *data = (lod < MAX_MIP_LEVELS && desc->mip_addr[lod]) ? desc->mip_addr[lod] : desc->data;

        int x = x_coords[lane];
        int y = y_coords[lane];
        int z = z_coords ? z_coords[lane] : 0;

        read_texel_color_data(data, width, height, depth, desc->channels, x, y, z, &out_r[lane], &out_g[lane], &out_b[lane], &out_a[lane]);
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
    fetch_texture_generic_simt(desc, x_coords, y_coords, NULL, NULL, out_r, out_g, out_b, out_a);
}

void handle_op_type_image(JitContext *ctx, uint32_t res_id, uint32_t *operands)
{
    ctx->type_info[res_id].opcode = SpvOpTypeImage;
    ctx->type_info[res_id].base_type_id = operands[0];

    ctx->type_info[res_id].image_type.sampled_type = operands[0];
    ctx->type_info[res_id].image_type.dim         = operands[1];
    ctx->type_info[res_id].image_type.depth       = operands[2];
    ctx->type_info[res_id].image_type.arrayed     = operands[3];
    ctx->type_info[res_id].image_type.ms          = operands[4];
    ctx->type_info[res_id].image_type.sampled     = operands[5];
    ctx->type_info[res_id].image_type.format      = operands[6];
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

static void emit_sample_call_generic(
    JitContext *ctx,
    uint32_t res_id,
    LLVMValueRef image_val,
    LLVMValueRef u_coords,
    LLVMValueRef v_coords,
    LLVMValueRef w_coords,
    LLVMValueRef du_dx, LLVMValueRef dv_dx, LLVMValueRef dw_dx,
    LLVMValueRef du_dy, LLVMValueRef dv_dy, LLVMValueRef dw_dy,
    LLVMValueRef explicit_lod
)
{
    LLVMTypeRef ptr_type = ctx->ptr_type;
    LLVMTypeRef vec_type = ctx->vec_float_type;

    if (!image_val)
    {
        image_val = LLVMConstNull(ptr_type);
    }

    LLVMValueRef u_alloca = LLVMBuildAlloca(ctx->builder, vec_type, "u_alloca");
    LLVMValueRef v_alloca = LLVMBuildAlloca(ctx->builder, vec_type, "v_alloca");
    LLVMValueRef w_alloca = LLVMBuildAlloca(ctx->builder, vec_type, "w_alloca");

    LLVMValueRef du_dx_alloca = LLVMBuildAlloca(ctx->builder, vec_type, "du_dx_alloca");
    LLVMValueRef dv_dx_alloca = LLVMBuildAlloca(ctx->builder, vec_type, "dv_dx_alloca");
    LLVMValueRef dw_dx_alloca = LLVMBuildAlloca(ctx->builder, vec_type, "dw_dx_alloca");

    LLVMValueRef du_dy_alloca = LLVMBuildAlloca(ctx->builder, vec_type, "du_dy_alloca");
    LLVMValueRef dv_dy_alloca = LLVMBuildAlloca(ctx->builder, vec_type, "dv_dy_alloca");
    LLVMValueRef dw_dy_alloca = LLVMBuildAlloca(ctx->builder, vec_type, "dw_dy_alloca");

    LLVMValueRef lod_alloca = explicit_lod ? LLVMBuildAlloca(ctx->builder, vec_type, "lod_alloca") : NULL;

    LLVMValueRef r_alloca = LLVMBuildAlloca(ctx->builder, vec_type, "r_alloca");
    LLVMValueRef g_alloca = LLVMBuildAlloca(ctx->builder, vec_type, "g_alloca");
    LLVMValueRef b_alloca = LLVMBuildAlloca(ctx->builder, vec_type, "b_alloca");
    LLVMValueRef a_alloca = LLVMBuildAlloca(ctx->builder, vec_type, "a_alloca");

    LLVMSetAlignment(u_alloca, 64);
    LLVMSetAlignment(v_alloca, 64);
    LLVMSetAlignment(w_alloca, 64);

    LLVMSetAlignment(du_dx_alloca, 64);
    LLVMSetAlignment(dv_dx_alloca, 64);
    LLVMSetAlignment(dw_dx_alloca, 64);

    LLVMSetAlignment(du_dy_alloca, 64);
    LLVMSetAlignment(dv_dy_alloca, 64);
    LLVMSetAlignment(dw_dy_alloca, 64);

    if (lod_alloca) LLVMSetAlignment(lod_alloca, 64);

    LLVMSetAlignment(r_alloca, 64);
    LLVMSetAlignment(g_alloca, 64);
    LLVMSetAlignment(b_alloca, 64);
    LLVMSetAlignment(a_alloca, 64);

    LLVMBuildStore(ctx->builder, u_coords, u_alloca);
    LLVMBuildStore(ctx->builder, v_coords, v_alloca);
    LLVMBuildStore(ctx->builder, w_coords, w_alloca);

    LLVMBuildStore(ctx->builder, du_dx, du_dx_alloca);
    LLVMBuildStore(ctx->builder, dv_dx, dv_dx_alloca);
    LLVMBuildStore(ctx->builder, dw_dx, dw_dx_alloca);

    LLVMBuildStore(ctx->builder, du_dy, du_dy_alloca);
    LLVMBuildStore(ctx->builder, dv_dy, dv_dy_alloca);
    LLVMBuildStore(ctx->builder, dw_dy, dw_dy_alloca);

    if (lod_alloca) LLVMBuildStore(ctx->builder, explicit_lod, lod_alloca);

    LLVMValueRef sample_func = LLVMGetNamedFunction(ctx->module, "sample_texture_generic_simt");
    LLVMTypeRef param_types[15] = {
        ptr_type, ptr_type, ptr_type, ptr_type,
        ptr_type, ptr_type, ptr_type,
        ptr_type, ptr_type, ptr_type,
        ptr_type,
        ptr_type, ptr_type, ptr_type, ptr_type
    };
    LLVMTypeRef func_type = LLVMFunctionType(LLVMVoidTypeInContext(ctx->context), param_types, 15, 0);

    if (!sample_func)
    {
        sample_func = LLVMAddFunction(ctx->module, "sample_texture_generic_simt", func_type);
        if (ctx->engine)
        {
            LLVMAddGlobalMapping(ctx->engine, sample_func, (void*)&sample_texture_generic_simt);
        }
    }

    LLVMValueRef args[15] = {
        image_val,
        u_alloca,
        v_alloca,
        w_alloca,
        du_dx_alloca,
        dv_dx_alloca,
        dw_dx_alloca,
        du_dy_alloca,
        dv_dy_alloca,
        dw_dy_alloca,
        lod_alloca ? lod_alloca : LLVMConstNull(ptr_type),
        r_alloca,
        g_alloca,
        b_alloca,
        a_alloca
    };

    LLVMBuildCall2(ctx->builder, func_type, sample_func, args, 15, "");

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

    LLVMValueRef zero_vec_elems[SIMT_WIDTH];
    for (int i = 0; i < SIMT_WIDTH; i++) {
        zero_vec_elems[i] = LLVMConstReal(ctx->float_type, 0.0f);
    }
    LLVMValueRef zero_vec = LLVMConstVector(zero_vec_elems, SIMT_WIDTH);

    LLVMValueRef u_coords = zero_vec;
    LLVMValueRef v_coords = zero_vec;
    LLVMValueRef w_coords = zero_vec;

    if (coords_val)
    {
        LLVMTypeRef coords_type = LLVMTypeOf(coords_val);
        if (LLVMGetTypeKind(coords_type) == LLVMArrayTypeKind)
        {
            unsigned num_elems = LLVMGetArrayLength(coords_type);
            if (num_elems >= 1) u_coords = LLVMBuildExtractValue(ctx->builder, coords_val, 0, "u_coords");
            if (num_elems >= 2) v_coords = LLVMBuildExtractValue(ctx->builder, coords_val, 1, "v_coords");
            if (num_elems >= 3) w_coords = LLVMBuildExtractValue(ctx->builder, coords_val, 2, "w_coords");
        }
        else if (LLVMGetTypeKind(coords_type) == LLVMVectorTypeKind)
        {
            u_coords = coords_val;
        }
    }

    // Extract U, V, W coordinates for spatial derivatives across SIMT 2x2 quads
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
    LLVMValueRef w_neighbor_x = LLVMBuildShuffleVector(ctx->builder, w_coords, w_coords, ddx_mask, "w_neighbor_x");

    LLVMValueRef u_neighbor_y = LLVMBuildShuffleVector(ctx->builder, u_coords, u_coords, ddy_mask, "u_neighbor_y");
    LLVMValueRef v_neighbor_y = LLVMBuildShuffleVector(ctx->builder, v_coords, v_coords, ddy_mask, "v_neighbor_y");
    LLVMValueRef w_neighbor_y = LLVMBuildShuffleVector(ctx->builder, w_coords, w_coords, ddy_mask, "w_neighbor_y");

    LLVMValueRef du_dx = LLVMBuildFSub(ctx->builder, u_coords, u_neighbor_x, "du_dx");
    LLVMValueRef dv_dx = LLVMBuildFSub(ctx->builder, v_coords, v_neighbor_x, "dv_dx");
    LLVMValueRef dw_dx = LLVMBuildFSub(ctx->builder, w_coords, w_neighbor_x, "dw_dx");

    LLVMValueRef du_dy = LLVMBuildFSub(ctx->builder, u_coords, u_neighbor_y, "du_dy");
    LLVMValueRef dv_dy = LLVMBuildFSub(ctx->builder, v_coords, v_neighbor_y, "dv_dy");
    LLVMValueRef dw_dy = LLVMBuildFSub(ctx->builder, w_coords, w_neighbor_y, "dw_dy");

    emit_sample_call_generic(
        ctx, res_id, image_val,
        u_coords, v_coords, w_coords,
        du_dx, dv_dx, dw_dx,
        du_dy, dv_dy, dw_dy,
        NULL
    );
}

void handle_op_image_sample_explicit_lod(JitContext *ctx, uint32_t res_id, uint32_t *operands)
{
    uint32_t image_id = operands[0];
    uint32_t coords_id = operands[1];

    LLVMValueRef coords_val = get_val(ctx, coords_id);
    LLVMValueRef image_val = get_val(ctx, image_id);

    LLVMValueRef zero_vec_elems[SIMT_WIDTH];
    for (int i = 0; i < SIMT_WIDTH; i++) {
        zero_vec_elems[i] = LLVMConstReal(ctx->float_type, 0.0f);
    }
    LLVMValueRef zero_vec = LLVMConstVector(zero_vec_elems, SIMT_WIDTH);

    LLVMValueRef u_coords = zero_vec;
    LLVMValueRef v_coords = zero_vec;
    LLVMValueRef w_coords = zero_vec;

    if (coords_val)
    {
        LLVMTypeRef coords_type = LLVMTypeOf(coords_val);
        if (LLVMGetTypeKind(coords_type) == LLVMArrayTypeKind)
        {
            unsigned num_elems = LLVMGetArrayLength(coords_type);
            if (num_elems >= 1) u_coords = LLVMBuildExtractValue(ctx->builder, coords_val, 0, "u_coords");
            if (num_elems >= 2) v_coords = LLVMBuildExtractValue(ctx->builder, coords_val, 1, "v_coords");
            if (num_elems >= 3) w_coords = LLVMBuildExtractValue(ctx->builder, coords_val, 2, "w_coords");
        }
        else if (LLVMGetTypeKind(coords_type) == LLVMVectorTypeKind)
        {
            u_coords = coords_val;
        }
    }

    LLVMValueRef explicit_lod = NULL;
    uint32_t image_operands = operands[2];
    if (image_operands & 0x2) // Lod operand present
    {
        uint32_t lod_id = operands[3];
        LLVMValueRef lod_val = get_val(ctx, lod_id);
        if (lod_val)
        {
            if (LLVMGetTypeKind(LLVMTypeOf(lod_val)) == LLVMArrayTypeKind)
            {
                explicit_lod = LLVMBuildExtractValue(ctx->builder, lod_val, 0, "lod_comp");
            }
            else
            {
                explicit_lod = lod_val;
            }
        }
    }

    LLVMValueRef du_dx = zero_vec, dv_dx = zero_vec, dw_dx = zero_vec;
    LLVMValueRef du_dy = zero_vec, dv_dy = zero_vec, dw_dy = zero_vec;

    emit_sample_call_generic(
        ctx, res_id, image_val,
        u_coords, v_coords, w_coords,
        du_dx, dv_dx, dw_dx,
        du_dy, dv_dy, dw_dy,
        explicit_lod
    );
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

    LLVMTypeRef vec_int_type = LLVMVectorType(ctx->int_type, SIMT_WIDTH);
    LLVMValueRef zero_int_elems[SIMT_WIDTH];
    for (int i = 0; i < SIMT_WIDTH; i++) {
        zero_int_elems[i] = LLVMConstInt(ctx->int_type, 0, 0);
    }
    LLVMValueRef zero_int_vec = LLVMConstVector(zero_int_elems, SIMT_WIDTH);

    LLVMValueRef x_coords = zero_int_vec;
    LLVMValueRef y_coords = zero_int_vec;
    LLVMValueRef z_coords = zero_int_vec;

    if (coords_val)
    {
        LLVMTypeRef coords_type = LLVMTypeOf(coords_val);
        if (LLVMGetTypeKind(coords_type) == LLVMArrayTypeKind)
        {
            unsigned num_elems = LLVMGetArrayLength(coords_type);
            if (num_elems >= 1) x_coords = LLVMBuildExtractValue(ctx->builder, coords_val, 0, "x_coords");
            if (num_elems >= 2) y_coords = LLVMBuildExtractValue(ctx->builder, coords_val, 1, "y_coords");
            if (num_elems >= 3) z_coords = LLVMBuildExtractValue(ctx->builder, coords_val, 2, "z_coords");
        }
        else if (LLVMGetTypeKind(coords_type) == LLVMVectorTypeKind)
        {
            x_coords = coords_val;
        }
    }

    LLVMValueRef lod_coords = NULL;
    uint32_t image_operands = operands[2];
    if (image_operands & 0x2) // Lod operand present
    {
        uint32_t lod_id = operands[3];
        LLVMValueRef lod_val = get_val(ctx, lod_id);
        if (lod_val)
        {
            if (LLVMGetTypeKind(LLVMTypeOf(lod_val)) == LLVMArrayTypeKind)
            {
                lod_coords = LLVMBuildExtractValue(ctx->builder, lod_val, 0, "lod_comp");
            }
            else
            {
                lod_coords = lod_val;
            }
        }
    }

    LLVMTypeRef ptr_type = ctx->ptr_type;
    LLVMTypeRef vec_float_type = ctx->vec_float_type;

    LLVMValueRef x_alloca = LLVMBuildAlloca(ctx->builder, vec_int_type, "x_alloca");
    LLVMValueRef y_alloca = LLVMBuildAlloca(ctx->builder, vec_int_type, "y_alloca");
    LLVMValueRef z_alloca = LLVMBuildAlloca(ctx->builder, vec_int_type, "z_alloca");
    LLVMValueRef lod_alloca = lod_coords ? LLVMBuildAlloca(ctx->builder, vec_int_type, "lod_alloca") : NULL;

    LLVMValueRef r_alloca = LLVMBuildAlloca(ctx->builder, vec_float_type, "r_alloca");
    LLVMValueRef g_alloca = LLVMBuildAlloca(ctx->builder, vec_float_type, "g_alloca");
    LLVMValueRef b_alloca = LLVMBuildAlloca(ctx->builder, vec_float_type, "b_alloca");
    LLVMValueRef a_alloca = LLVMBuildAlloca(ctx->builder, vec_float_type, "a_alloca");

    LLVMSetAlignment(x_alloca, 64);
    LLVMSetAlignment(y_alloca, 64);
    LLVMSetAlignment(z_alloca, 64);
    if (lod_alloca) LLVMSetAlignment(lod_alloca, 64);
    LLVMSetAlignment(r_alloca, 64);
    LLVMSetAlignment(g_alloca, 64);
    LLVMSetAlignment(b_alloca, 64);
    LLVMSetAlignment(a_alloca, 64);

    LLVMBuildStore(ctx->builder, x_coords, x_alloca);
    LLVMBuildStore(ctx->builder, y_coords, y_alloca);
    LLVMBuildStore(ctx->builder, z_coords, z_alloca);
    if (lod_alloca) LLVMBuildStore(ctx->builder, lod_coords, lod_alloca);

    LLVMValueRef fetch_func = LLVMGetNamedFunction(ctx->module, "fetch_texture_generic_simt");
    LLVMTypeRef param_types[9] = {
        ptr_type, ptr_type, ptr_type, ptr_type, ptr_type,
        ptr_type, ptr_type, ptr_type, ptr_type
    };
    LLVMTypeRef func_type = LLVMFunctionType(LLVMVoidTypeInContext(ctx->context), param_types, 9, 0);

    if (!fetch_func)
    {
        fetch_func = LLVMAddFunction(ctx->module, "fetch_texture_generic_simt", func_type);
        if (ctx->engine)
        {
            LLVMAddGlobalMapping(ctx->engine, fetch_func, (void*)&fetch_texture_generic_simt);
        }
    }

    LLVMValueRef args[9] = {
        image_val,
        x_alloca,
        y_alloca,
        z_alloca,
        lod_alloca ? lod_alloca : LLVMConstNull(ptr_type),
        r_alloca,
        g_alloca,
        b_alloca,
        a_alloca
    };

    LLVMBuildCall2(ctx->builder, func_type, fetch_func, args, 9, "");

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
    LLVMValueRef depth_vec;

    if (image_val)
    {
        size_t off_w = offsetof(TextureSamplerDescriptor, width);
        size_t off_h = offsetof(TextureSamplerDescriptor, height);
        size_t off_d = offsetof(TextureSamplerDescriptor, depth);

        LLVMValueRef w_idx = LLVMConstInt(i32_type, off_w, 0);
        LLVMValueRef w_ptr = LLVMBuildInBoundsGEP2(ctx->builder, ctx->int8_type, image_val, &w_idx, 1, "w_ptr");
        LLVMValueRef w_scalar = LLVMBuildLoad2(ctx->builder, i32_type, w_ptr, "w_scalar");

        LLVMValueRef h_idx = LLVMConstInt(i32_type, off_h, 0);
        LLVMValueRef h_ptr = LLVMBuildInBoundsGEP2(ctx->builder, ctx->int8_type, image_val, &h_idx, 1, "h_ptr");
        LLVMValueRef h_scalar = LLVMBuildLoad2(ctx->builder, i32_type, h_ptr, "h_scalar");

        LLVMValueRef d_idx = LLVMConstInt(i32_type, off_d, 0);
        LLVMValueRef d_ptr = LLVMBuildInBoundsGEP2(ctx->builder, ctx->int8_type, image_val, &d_idx, 1, "d_ptr");
        LLVMValueRef d_scalar = LLVMBuildLoad2(ctx->builder, i32_type, d_ptr, "d_scalar");

        LLVMValueRef zero_idx = LLVMConstInt(i32_type, 0, 0);

        width_vec = LLVMBuildInsertElement(ctx->builder, LLVMGetUndef(vec_i32_type), w_scalar, zero_idx, "w_vec_0");
        height_vec = LLVMBuildInsertElement(ctx->builder, LLVMGetUndef(vec_i32_type), h_scalar, zero_idx, "h_vec_0");
        depth_vec = LLVMBuildInsertElement(ctx->builder, LLVMGetUndef(vec_i32_type), d_scalar, zero_idx, "d_vec_0");

        LLVMValueRef zero_mask_elems[SIMT_WIDTH];
        for (int i = 0; i < SIMT_WIDTH; i++) zero_mask_elems[i] = LLVMConstInt(i32_type, 0, 0);
        LLVMValueRef zero_mask = LLVMConstVector(zero_mask_elems, SIMT_WIDTH);

        width_vec = LLVMBuildShuffleVector(ctx->builder, width_vec, width_vec, zero_mask, "w_splat");
        height_vec = LLVMBuildShuffleVector(ctx->builder, height_vec, height_vec, zero_mask, "h_splat");
        depth_vec = LLVMBuildShuffleVector(ctx->builder, depth_vec, depth_vec, zero_mask, "d_splat");
    }
    else
    {
        LLVMValueRef zero_elems[SIMT_WIDTH];
        for (int i = 0; i < SIMT_WIDTH; i++) zero_elems[i] = LLVMConstInt(i32_type, 0, 0);
        width_vec = LLVMConstVector(zero_elems, SIMT_WIDTH);
        height_vec = LLVMConstVector(zero_elems, SIMT_WIDTH);
        depth_vec = LLVMConstVector(zero_elems, SIMT_WIDTH);
    }

    LLVMValueRef res = LLVMGetUndef(LLVMArrayType(vec_i32_type, 3));
    res = LLVMBuildInsertValue(ctx->builder, res, width_vec, 0, "size_w");
    res = LLVMBuildInsertValue(ctx->builder, res, height_vec, 1, "size_h");
    res = LLVMBuildInsertValue(ctx->builder, res, depth_vec, 2, "size_d");
    set_val(ctx, res_id, res);
}

void handle_op_image_query_size_lod(JitContext *ctx, uint32_t res_id, uint32_t *operands)
{
    handle_op_image_query_size(ctx, res_id, operands);
}