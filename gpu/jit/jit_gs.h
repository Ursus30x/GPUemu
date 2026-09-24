#ifndef JIT_GS_H
#define JIT_GS_H

#include "jit.h"


/**
 * Handle SPIR-V OpExecutionMode instructions for Geometry Shader stage.
 */
void handle_op_execution_mode_gs(JitContext *ctx, uint32_t *operands, uint32_t operand_count);

/**
 * Handle SPIR-V OpEmitVertex instruction (Opcode 218).
 */
void handle_op_emit_vertex(JitContext *ctx, uint32_t *operands, uint32_t operand_count);

/**
 * Handle SPIR-V OpEndPrimitive instruction (Opcode 219).
 */
void handle_op_end_primitive(JitContext *ctx, uint32_t *operands, uint32_t operand_count);


#endif // JIT_GS_H
