#include "jit_mem.h"
#include "debug_gpu.h"


//handle_op_variable: Maps SPIR-V variables to physical resources or local memory.
//operands[0]: Storage Class (Uniform, Input, Output, Function, etc.)
void handle_op_variable(JitContext* ctx, uint32_t res_id, uint32_t type_id, uint32_t opCount, uint32_t* operands) 
{
    uint32_t storage_class = operands[0];
    SpvDecoInfo* deco = &ctx->decorations[res_id];
    
    // Always map the type ID so we know what this variable represents later
    ctx->type_kind_map[res_id] = (uint8_t)type_id;

    // --- CASE A: GLOBAL SCOPE (Module Level) ---
    // We cannot emit LLVM instructions (GEP/Load/Alloca) here because there is no function.
    if (ctx->func == NULL) 
    {
        // Add to our "Pending Globals" list to be resolved inside the main function prologue
        if (ctx->global_count < MAX_GLOBALS) 
        {
            uint32_t idx = ctx->global_count++;
            ctx->globals[idx].res_id = res_id;
            ctx->globals[idx].storage_class = storage_class;
            
            // Store binding for Uniforms/Buffers, Location for Inputs/Outputs
            if (storage_class == SpvStorageClassUniform || storage_class == SpvStorageClassStorageBuffer || storage_class == SpvStorageClassUniformConstant)
                ctx->globals[idx].binding_or_loc = deco->binding;
            else
                ctx->globals[idx].binding_or_loc = deco->location;
        }
        else 
        {
            DEBUG_PRINT("Error: Exceeded MAX_PENDING_GLOBALS\n");
        }
        return;
    }

    // --- CASE B: FUNCTION SCOPE (Inside a Function) ---
    // If we are already inside a function, we can emit instructions immediately.
    if (storage_class == SpvStorageClassFunction || storage_class == SpvStorageClassPrivate) 
    {
        LLVMTypeRef type = map_spv_to_llvm_type(ctx, type_id);
        LLVMValueRef alloca_inst = LLVMBuildAlloca(ctx->builder, type, "local_var");
        LLVMSetAlignment(alloca_inst, 64);
        
        // Handle optional Initializer (OpVariable %type %storage [%initializer])
        if (opCount > 1) 
        {
            uint32_t init_id = operands[1];
            LLVMValueRef init_val = ctx->id_val_map[init_id];
            if (init_val != NULL) 
            {
                LLVMBuildStore(ctx->builder, init_val, alloca_inst);
            }
        }
        
        set_val(ctx, res_id, alloca_inst);
    }
    else if (storage_class == SpvStorageClassUniform || storage_class == SpvStorageClassStorageBuffer ||
             storage_class == SpvStorageClassUniformConstant ||
             storage_class == SpvStorageClassInput || storage_class == SpvStorageClassOutput)
    {
        // If a variable with these classes is defined inside a function (rare but possible in some SPIR-V),
        // we resolve it immediately from the context argument.
        
        uint32_t field_idx = 0;
        int32_t offset = 0;

        if (storage_class == SpvStorageClassUniform || storage_class == SpvStorageClassStorageBuffer || storage_class == SpvStorageClassUniformConstant) { field_idx = 0; offset = deco->binding; }
        else if (storage_class == SpvStorageClassInput) { field_idx = 1; offset = deco->location; }
        else if (storage_class == SpvStorageClassOutput) { field_idx = 2; offset = deco->location; }

        if (offset >= 0) 
        {
            LLVMValueRef indices[] = {
                LLVMConstInt(ctx->int_type, 0, 0),
                LLVMConstInt(ctx->int_type, field_idx, 0),
                LLVMConstInt(ctx->int_type, offset, 0)
            };

            LLVMValueRef slot_ptr = LLVMBuildInBoundsGEP2(ctx->builder, ctx->exec_ctx_type, ctx->env_arg_param, indices, 3, "res_slot");
            LLVMValueRef actual_ptr = LLVMBuildLoad2(ctx->builder, ctx->ptr_type, slot_ptr, "res_ptr");
            set_val(ctx, res_id, actual_ptr);
        }
    }
    else 
    {
        DEBUG_PRINT("Warning: Unhandled Storage Class %u in function scope\n", storage_class);
    }
}

static LLVMValueRef build_recursive_load(JitContext *ctx, LLVMTypeRef type, LLVMValueRef ptr) 
{
    LLVMTypeKind kind = LLVMGetTypeKind(type);

    if (kind == LLVMVectorTypeKind) 
    {
        LLVMValueRef load = LLVMBuildLoad2(ctx->builder, type, ptr, "load_simt");
        LLVMSetAlignment(load, 64);
        return load;
    } 
    else if (kind == LLVMArrayTypeKind) 
    {
        uint32_t count = LLVMGetArrayLength(type);
        LLVMTypeRef elem_type = LLVMGetElementType(type);
        
        LLVMValueRef aggregate = LLVMGetUndef(type);

        for (uint32_t i = 0; i < count; i++) {
            LLVMValueRef index = LLVMConstInt(LLVMInt32TypeInContext(ctx->context), i, 0);
            
            LLVMValueRef element_ptr = LLVMBuildInBoundsGEP2(ctx->builder, elem_type, ptr, &index, 1, "load_gep");
            
            LLVMValueRef element_val = build_recursive_load(ctx, elem_type, element_ptr);
            
            aggregate = LLVMBuildInsertValue(ctx->builder, aggregate, element_val, i, "insert_elem");
        }
        return aggregate;
    }

    LLVMValueRef load = LLVMBuildLoad2(ctx->builder, type, ptr, "load_scalar");
    LLVMSetAlignment(load, 64);
    return load;
}

void handle_op_load(JitContext* ctx, uint32_t res_id, uint32_t res_type_id, uint32_t* operands) 
{
    uint32_t ptr_id = operands[0];
    LLVMValueRef ptr = get_val(ctx, ptr_id);
    
    SpvTypeInfo* type_info = &ctx->type_info[res_type_id];
    if (type_info->opcode == SpvOpTypeSampledImage || 
        type_info->opcode == SpvOpTypeImage || 
        type_info->opcode == SpvOpTypeSampler)
    {
        set_val(ctx, res_id, ptr);
        return;
    }

    LLVMTypeRef result_llvm_type = map_spv_to_llvm_type(ctx, res_type_id);

    if (LLVMIsConstant(ptr) && LLVMIsNull(ptr)) 
    {
        set_val(ctx, res_id, LLVMConstNull(result_llvm_type));
        return;
    }

    LLVMValueRef final_val = build_recursive_load(ctx, result_llvm_type, ptr);
    
    set_val(ctx, res_id, final_val);
}


static void build_recursive_store(JitContext *ctx, LLVMValueRef val_to_store, LLVMValueRef ptr, LLVMValueRef mask) {
    LLVMTypeRef type = LLVMTypeOf(val_to_store);
    LLVMTypeKind kind = LLVMGetTypeKind(type);

    if (kind == LLVMVectorTypeKind) 
    {
        LLVMValueRef current_mem_val = LLVMBuildLoad2(ctx->builder, type, ptr, "current_mem");
        
        LLVMValueRef masked_val = LLVMBuildSelect(ctx->builder, mask, val_to_store, current_mem_val, "masked_val");
        
        LLVMValueRef store_inst = LLVMBuildStore(ctx->builder, masked_val, ptr);
        LLVMSetAlignment(store_inst, 64);
    } 
    else if (kind == LLVMArrayTypeKind) 
    {
        uint32_t count = LLVMGetArrayLength(type);
        LLVMTypeRef elem_type = LLVMGetElementType(type);

        for (uint32_t i = 0; i < count; i++) 
        {
            LLVMValueRef index = LLVMConstInt(LLVMInt32TypeInContext(ctx->context), i, 0);
            
            LLVMValueRef element_ptr = LLVMBuildInBoundsGEP2(ctx->builder, elem_type, ptr, &index, 1, "store_gep");
            
            LLVMValueRef element_val = LLVMBuildExtractValue(ctx->builder, val_to_store, i, "extract_elem");

            build_recursive_store(ctx, element_val, element_ptr, mask);
        }
    }
    else
    {
        LLVMValueRef store_inst = LLVMBuildStore(ctx->builder, val_to_store, ptr);
        LLVMSetAlignment(store_inst, 64);
    }
}

// handle_op_store: Writes data from a register (ID) into memory.
void handle_op_store(JitContext* ctx, uint32_t* operands) {
    uint32_t ptr_id = operands[0];
    uint32_t val_id = operands[1];

    LLVMValueRef ptr = get_val(ctx, ptr_id);
    LLVMValueRef val = get_val(ctx, val_id);

    build_recursive_store(ctx, val, ptr, ctx->emask);
}

/**
 * handle_op_access_chain: Calculates pointer offsets using raw integer arithmetic.
 */
void handle_op_access_chain(JitContext *ctx, uint32_t res_id, uint32_t type_id, uint32_t* operands, int operand_count) 
{
    uint32_t base_id = operands[0];
    LLVMValueRef base_ptr = get_val(ctx, base_id);
    
    DEBUG_PRINT("\n=== AccessChain Start ===\n");
    DEBUG_PRINT("Result ID: %u, Base ID: %u, Type ID: %u, Operand Count: %d\n",
                res_id, base_id, type_id, operand_count);

    if (!base_ptr) {
        base_ptr = LLVMConstNull(ctx->ptr_type);
        DEBUG_PRINT("Base pointer is NULL, using null constant\n");
    }

    LLVMTypeRef i64_type = LLVMInt64TypeInContext(ctx->context);
    LLVMValueRef total_offset = LLVMConstInt(i64_type, 0, 0);
    uint32_t current_type_id = ctx->type_info[type_id].base_type_id;

    DEBUG_PRINT("Dereferenced pointer: current_type_id=%u\n", current_type_id);

    // Process each index operand to accumulate the total byte offset
    for (int i = 1; i < operand_count; i++) {
        uint32_t index_id = operands[i];
        LLVMValueRef idx_val = get_val(ctx, index_id);

        DEBUG_PRINT("Index operand %d: ID=%u, Value=%p\n", i, index_id, (void*)idx_val);

        if (!idx_val) {
            DEBUG_PRINT("  WARNING: Index ID %u not in value map, creating constant 0\n", index_id);
            idx_val = LLVMConstInt(ctx->int_type, 0, 0);
        }

        // Extract first element if the index is a vector
        if (LLVMGetTypeKind(LLVMTypeOf(idx_val)) == LLVMVectorTypeKind) {
            idx_val = LLVMBuildExtractElement(ctx->builder, idx_val, LLVMConstInt(ctx->int_type, 0, 0), "idx_s");
        }

        SpvTypeInfo* info = &ctx->type_info[current_type_id];
        DEBUG_PRINT("  Current type ID: %u, opcode=%d (Struct=%d, Array=%d)\n", 
                    current_type_id, info->opcode, SpvOpTypeStruct, SpvOpTypeArray);

        if (info->opcode == SpvOpTypeStruct) 
        {
            // ==========================================
            // Struct Handling
            // ==========================================
            uint64_t member_idx = 0;
            if (LLVMIsConstant(idx_val)) {
                member_idx = LLVMConstIntGetZExtValue(idx_val);
                DEBUG_PRINT("  Struct member index: %lu\n", member_idx);
            } else {
                DEBUG_PRINT("  WARNING: Index is not a constant, using 0\n");
            }

            int32_t offset_bytes = 0;
            for (MemberDecoNode* m = ctx->member_decorations[current_type_id]; m != NULL; m = m->next) {
                if (m->member_index == (uint32_t)member_idx) {
                    offset_bytes = m->offset;
                    DEBUG_PRINT("  Found member %lu decoration: offset=%d bytes\n", member_idx, offset_bytes);
                    break;
                }
            }

            total_offset = LLVMBuildAdd(ctx->builder, total_offset, LLVMConstInt(i64_type, offset_bytes, 0), "struct_off");
            
            if (info->member_types && member_idx < info->member_count) {
                current_type_id = info->member_types[member_idx];
                DEBUG_PRINT("  Next type ID: %u\n", current_type_id);
            }
        } 
        else if (info->opcode == SpvOpTypeArray) 
        {
            // ==========================================
            // Array Handling
            // ==========================================
            idx_val = LLVMBuildZExt(ctx->builder, idx_val, i64_type, "idx64");
            
            int32_t stride = ctx->decorations[current_type_id].array_stride;
            stride = (stride <= 0) ? 4 : stride;
            DEBUG_PRINT("  Array stride: %d\n", stride);
            
            LLVMValueRef array_off = LLVMBuildMul(ctx->builder, idx_val, LLVMConstInt(i64_type, stride, 0), "arr_step");
            total_offset = LLVMBuildAdd(ctx->builder, total_offset, array_off, "arr_off");
            current_type_id = info->base_type_id;
        }
        else
        {
            // ==========================================
            // SIMT / Built-in / Fallback Handling
            // ==========================================
            uint32_t type_info_idx = ctx->type_kind_map[base_id];
            SpvTypeInfo *ptr_type_info = &ctx->type_info[type_info_idx];
            uint32_t struct_type_info = ptr_type_info->base_type_id;
            
            MemberDecoNode* m = ctx->member_decorations[struct_type_info];
            LLVMValueRef offset;
            
            idx_val = LLVMBuildZExt(ctx->builder, idx_val, i64_type, "idx64");

            if (m == NULL) {
                uint32_t size = get_spv_type_size(info->opcode);
                LLVMValueRef simt_size = LLVMConstInt(i64_type, SIMT_WIDTH * size, 0);
                offset = LLVMBuildMul(ctx->builder, simt_size, idx_val, "index");
            } 
            else {
                LLVMValueRef offset_vec_vals[16]; // Max struct members
                uint32_t j = 0;
                
                while (m && j < 16) {
                    if (m->buildin == SpvBuiltInPosition) {
                        uint32_t final_offset = 0; // Base offset to reach 'vertexOut'
                        offset_vec_vals[j] = LLVMConstInt(i64_type, final_offset, 0);
                        
                       
                        DEBUG_PRINT("  BuiltIn Position mapped to offset: %u bytes\n", final_offset);
                    } else {
                        offset_vec_vals[j] = LLVMConstInt(i64_type, m->offset * SIMT_WIDTH, 0);
                    }
                    j++;
                    m = m->next;
                }
                
                LLVMValueRef offset_vec = LLVMConstVector(offset_vec_vals, j);
                
                
                
                offset = LLVMBuildExtractElement(ctx->builder, offset_vec, idx_val, "struct_off");
            }

       
            total_offset = LLVMBuildAdd(ctx->builder, total_offset, offset, "final_elem_off");
        }
    }

    // ==========================================
    // Final Offset Application
    // ==========================================
    // Step by exact bytes using an i8 pointee type GEP.
    LLVMTypeRef i8_type = LLVMInt8TypeInContext(ctx->context);
    LLVMValueRef gep_indices[] = { total_offset };

    LLVMValueRef final_ptr = LLVMBuildGEP2(ctx->builder, i8_type, base_ptr, gep_indices, 1, "access_chain_ptr");

    DEBUG_PRINT("Final AccessChain result: ID %u, total_offset added via i8 GEP\n", res_id);
    DEBUG_PRINT("=== AccessChain End ===\n\n");

    set_val(ctx, res_id, final_ptr);
    ctx->type_kind_map[res_id] = ctx->type_kind_map[base_id];
}

uint32_t get_spv_type_size(uint32_t spv_type_id)
{
    switch (spv_type_id)
    {
    case SpvOpTypeFloat:
    case SpvOpTypeInt:
        return 4;
    default:
        DEBUG_PRINT("Error: Size query for unhandled SPIR-V type ID %u\n", spv_type_id);
        exit(1);
        break;
    }
    return 0;
}