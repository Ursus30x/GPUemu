#ifndef RASTERIZER_SIMT_H
#define RASTERIZER_SIMT_H

#include "renderer.h"

void draw_triangle_simt_band(const TransformedVertex *vertices[3], GpuState *gpu, int band_min_y, int band_max_y, RenderThreadArgs *args);

void worker_transform_vertices_simt_impl(RenderThreadArgs *args);
void worker_rasterize_bands_simt_impl(RenderThreadArgs *args);
void worker_rasterize_points_simt_impl(RenderThreadArgs *args);
void worker_rasterize_lines_simt_impl(RenderThreadArgs *args);
void worker_compute_simt_impl(RenderThreadArgs *args);
void worker_task_shader_simt_impl(RenderThreadArgs *args);
void worker_mesh_shader_simt_impl(RenderThreadArgs *args);

#endif
