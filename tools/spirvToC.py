import argparse
import os
import struct
import subprocess
import tempfile


def convert_glsl_to_uint32_header(input_glsl_path, output_header_path, array_name):
    if not os.path.exists(input_glsl_path):
        print(f"Error: GLSL input file '{input_glsl_path}' not found.")
        return

    # 1. Read original GLSL source code
    try:
        with open(input_glsl_path, "r", encoding="utf-8") as f:
            glsl_source = f.read()
    except Exception as e:
        print(f"Error reading GLSL file: {e}")
        return

    # 2. Compile GLSL to SPIR-V binary using glslc via a temporary file
    with tempfile.NamedTemporaryFile(suffix=".spv", delete=False) as temp_spv:
        temp_spv_path = temp_spv.name

    try:
        cmd = ["glslc", input_glsl_path, "-o", temp_spv_path]
        print(f"Compiling with glslc: {' '.join(cmd)}")
        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode != 0:
            print(f"Compilation Error:\n{result.stderr}")
            return

        with open(temp_spv_path, "rb") as f:
            raw_data = f.read()
    finally:
        if os.path.exists(temp_spv_path):
            os.remove(temp_spv_path)

    total_bytes = len(raw_data)
    print(f"Compiled SPIR-V Size: {total_bytes} bytes")

    # 3. Align data to 32-bit boundaries and pack into integers
    padding_needed = (4 - (total_bytes % 4)) % 4
    if padding_needed > 0:
        raw_data += b"\x00" * padding_needed

    uint32_array = [total_bytes]  # First element is the size in bytes
    for i in range(0, len(raw_data), 4):
        chunk = raw_data[i : i + 4]
        val = struct.unpack("<I", chunk)[0]
        uint32_array.append(val)

    # 4. Generate output C header
    with open(output_header_path, "w", encoding="utf-8") as f:
        f.write(
            f"// Generated from '{os.path.basename(input_glsl_path)}' using glslc\n"
        )
        f.write(
            "// =============================================================================\n"
        )
        f.write("// ORIGINAL GLSL SOURCE\n")
        f.write(
            "// =============================================================================\n"
        )

        for line in glsl_source.splitlines():
            f.write(f"// {line}\n")

        f.write(
            "// =============================================================================\n\n"
        )

        f.write(f"// First element [0] is the byte-size ({total_bytes})\n")
        f.write(
            f"UINT32 {array_name}[{len(uint32_array)}] = {{\n    "
        )

        for i, val in enumerate(uint32_array):
            if i == 0:
                f.write(f"0x{val:08X}, // <-- Size in bytes\n    ")
            else:
                f.write(f"0x{val:08X}, ")
                if i % 6 == 0:
                    f.write("\n    ")

        f.write("\n};\n")

    print(f"Saved header to: {output_header_path} with array name '{array_name}'")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Compile GLSL to SPIR-V and embed into a C header."
    )

    parser.add_argument(
        "-i",
        "--input",
        required=True,
        help="Path to the input GLSL shader file",
    )
    parser.add_argument(
        "-o",
        "--header-out",
        default="shader.h",
        help="Path for the output C header file",
    )
    parser.add_argument(
        "-n",
        "--array-name",
        default="shader",
        help="Name of the generated uint32_t C array (default: 'shader')",
    )

    args = parser.parse_args()

    convert_glsl_to_uint32_header(args.input, args.header_out, args.array_name)