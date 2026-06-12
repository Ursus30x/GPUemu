import json
import sys

def generate_header(json_path, output_path):
    with open(json_path, 'r') as f:
        data = json.load(f)

    with open(output_path, 'w') as f:
        f.write("// Auto-generated SPIR-V Metadata for JIT\n")
        f.write("#ifndef SPIRV_JIT_META_H\n#define SPIRV_JIT_META_H\n\n")
        f.write("#include <stdint.h>\n#include <stdbool.h>\n\n")

        # ================================
        # SpvOp enum
        # ================================
        f.write("typedef enum {\n")
        for inst in data['instructions']:
            f.write(f"    Spv{inst['opname']} = {inst['opcode']},\n")
        f.write("    SpvOpMax = 0x7FFFFFFF\n")
        f.write("} SpvOp;\n\n")

        # ================================
        # Helper to extract and write enums from operand_kinds
        # ================================
        def write_enum_from_kind(kind_name, prefix):
            target_kind = next((k for k in data.get("operand_kinds", []) if k.get("kind") == kind_name), None)
            if target_kind:
                f.write(f"typedef enum {{\n")
                for enum in target_kind.get("enumerants", []):
                    name = enum["enumerant"]
                    value = enum["value"]
                    f.write(f"    {prefix}{name} = {value},\n")
                f.write(f"    {prefix}Max = 0x7FFFFFFF\n")
                
                # Cleanup: if prefix is "SpvBuiltIn", typedef should be "SpvBuiltIn"
                # If prefix is "SpvDecoration", typedef should be "SpvDecoration"
                f.write(f"}} {prefix};\n\n")

        # Generate Enums from SPIR-V Grammar
        write_enum_from_kind("Decoration", "SpvDecoration")
        write_enum_from_kind("StorageClass", "SpvStorageClass")
        write_enum_from_kind("BuiltIn", "SpvBuiltIn") 

        # ================================
        # SpvOpMeta struct
        # ================================
        f.write("typedef struct {\n")
        f.write("    const char* name;\n")
        f.write("    bool has_result_type;\n")
        f.write("    bool has_result_id;\n")
        f.write("    int fixed_operand_count;\n")
        f.write("} SpvOpMeta;\n\n")

        max_op = max(inst['opcode'] for inst in data['instructions'])
        f.write(f"static const SpvOpMeta SPV_META[{max_op + 1}] = {{\n")
        
        op_map = {inst['opcode']: inst for inst in data['instructions']}

        for i in range(max_op + 1):
            if i in op_map:
                inst = op_map[i]
                ops = inst.get('operands', [])
                
                has_type = any(o['kind'] == 'IdResultType' for o in ops)
                has_id = any(o['kind'] == 'IdResult' for o in ops)
                
                fixed_count = len([
                    o for o in ops
                    if o['kind'] not in ('IdResultType', 'IdResult')
                ])
                
                f.write(
                    f"    [{i}] = {{ \"{inst['opname']}\", "
                    f"{str(has_type).lower()}, "
                    f"{str(has_id).lower()}, "
                    f"{fixed_count} }},\n"
                )
            else:
                f.write(f"    [{i}] = {{ \"OpUnknown\", false, false, 0 }},\n")
        
        f.write("};\n\n")
        f.write("#endif\n")

if __name__ == "__main__":
    generate_header('spirv.core.grammar.json', 'spirv_jit_meta.h')
    print("Generated spirv_jit_meta.h with Op, Decoration, StorageClass, and BuiltIn.")