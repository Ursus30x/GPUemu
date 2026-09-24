#include "jit_gs.h"
#include "debug_gpu.h"

void handle_op_execution_mode_gs(JitContext *ctx, uint32_t *operands, uint32_t operand_count)
{
    if (operand_count < 2) return;

    SpvExecutionMode mode = (SpvExecutionMode)operands[1];
    switch (mode) {
    case SpvExecutionModeLocalSize:
        if (operand_count >= 5) {
            ctx->shader_info.local_size_x = operands[2];
            ctx->shader_info.local_size_y = operands[3];
            ctx->shader_info.local_size_z = operands[4];
        }
        break;

    case SpvExecutionModeInvocations:
        if (operand_count >= 3) {
            ctx->shader_info.gs_invocations = operands[2];
        }
        break;

    case SpvExecutionModeOutputVertices:
        if (operand_count >= 3) {
            ctx->shader_info.gs_max_output_vertices = operands[2];
        }
        break;

    case SpvExecutionModeInputPoints:
        ctx->shader_info.gs_input_primitive = 0; // Points (1 vertex)
        break;

    case SpvExecutionModeInputLines:
        ctx->shader_info.gs_input_primitive = 1; // Lines (2 vertices)
        break;

    case SpvExecutionModeTriangles:
        ctx->shader_info.gs_input_primitive = 2; // Triangles (3 vertices)
        break;

    case SpvExecutionModeOutputPoints:
        ctx->shader_info.gs_output_primitive = 0; // Points
        break;

    case SpvExecutionModeOutputLineStrip:
        ctx->shader_info.gs_output_primitive = 1; // LineStrip
        break;

    case SpvExecutionModeOutputTriangleStrip:
        ctx->shader_info.gs_output_primitive = 2; // TriangleStrip
        break;

    default:
        break;
    }
}

void handle_op_emit_vertex(JitContext *ctx, G_GNUC_UNUSED uint32_t *operands, G_GNUC_UNUSED uint32_t operand_count)
{
    if (!ctx || !ctx->builder || !ctx->env_arg_param) return;

    /* Access location_out_buffers[0] (Position output V4) */
    LLVMValueRef pos_indices[] = {
        LLVMConstInt(ctx->int_type, 0, 0),
        LLVMConstInt(ctx->int_type, 2, 0), // location_out_buffers field in ExecutionContext
        LLVMConstInt(ctx->int_type, 0, 0)  // location 0
    };
    LLVMValueRef pos_slot_ptr = LLVMBuildInBoundsGEP2(
        ctx->builder, ctx->exec_ctx_type, ctx->env_arg_param, pos_indices, 3, "emit_pos_slot");
    LLVMValueRef pos_ptr = LLVMBuildLoad2(
        ctx->builder, ctx->ptr_type, pos_slot_ptr, "emit_pos_ptr");

    /* Access location_out_buffers[1] (Color / Varying 0 output V4) */
    LLVMValueRef var_indices[] = {
        LLVMConstInt(ctx->int_type, 0, 0),
        LLVMConstInt(ctx->int_type, 2, 0), // location_out_buffers field in ExecutionContext
        LLVMConstInt(ctx->int_type, 1, 0)  // location 1
    };
    LLVMValueRef var_slot_ptr = LLVMBuildInBoundsGEP2(
        ctx->builder, ctx->exec_ctx_type, ctx->env_arg_param, var_indices, 3, "emit_var_slot");
    LLVMValueRef var_ptr = LLVMBuildLoad2(
        ctx->builder, ctx->ptr_type, var_slot_ptr, "emit_var_ptr");

    (void)pos_ptr;
    (void)var_ptr;

    DEBUG_PRINT("[JIT GS] OpEmitVertex IR generated\n");
}

void handle_op_end_primitive(JitContext *ctx, G_GNUC_UNUSED uint32_t *operands, G_GNUC_UNUSED uint32_t operand_count)
{
    if (!ctx || !ctx->builder || !ctx->env_arg_param) return;

    /* OpEndPrimitive terminates current primitive strip boundary */
    LLVMBuildFence(ctx->builder, LLVMAtomicOrderingSequentiallyConsistent, 0, "");

    DEBUG_PRINT("[JIT GS] OpEndPrimitive IR generated\n");
}
