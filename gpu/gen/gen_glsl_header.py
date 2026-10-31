import json
from pathlib import Path

def generate_glsl_header(json_path, output_path):
    if not Path(json_path).exists():
        print(f"Error: Could not find {json_path}")
        return

    with open(json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    instructions = data['instructions']

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("// Auto-generated GLSL.std.450 Metadata for JIT\n")
        f.write("// Based on: extinst.glsl.std.450.grammar.json\n")
        f.write("#ifndef GLSL_STD_450_META_H\n#define GLSL_STD_450_META_H\n\n")
        f.write("#include <stdint.h>\n\n")

        f.write("typedef enum {\n")
        for inst in instructions:
            f.write(f"    GLSLstd450{inst['opname']} = {inst['opcode']},\n")
        f.write("    GLSLstd450OpMax = 0x7FFFFFFF\n")
        f.write("} GLSLstd450Op;\n\n")


        f.write("typedef struct {\n")
        f.write("    const char* name;\n")
        f.write("    int arg_count;\n")
        f.write("} GLSLstd450Meta;\n\n")

        max_op = max(inst['opcode'] for inst in instructions)
        f.write(f"static const GLSLstd450Meta GLSL_STD_450_META[{max_op + 1}] = {{\n")
        
        op_map = {inst['opcode']: inst for inst in instructions}

        for i in range(max_op + 1):
            if i in op_map:
                inst = op_map[i]
                arg_count = len(inst.get('operands', []))
                f.write(f"    [{i}] = {{ \"{inst['opname']}\", {arg_count} }},\n")
            else:
                f.write(f"    [{i}] = {{ \"Unknown\", 0 }},\n")
        
        f.write("};\n\n")
        
        f.write("static inline const char* glsl_std_450_op_name(uint32_t opcode) {\n")
        f.write(f"    if (opcode <= {max_op}) return GLSL_STD_450_META[opcode].name;\n")
        f.write("    return \"Unknown\";\n")
        f.write("}\n\n")

        f.write("#endif // GLSL_STD_450_META_H\n")

if __name__ == "__main__":
    generate_glsl_header('extinst.glsl.std.450.grammar.json', 'glsl_std_450.h')
    print("Successfully generated glsl_std_450.h")