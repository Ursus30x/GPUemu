import argparse
import os
import struct

def convert_bin_to_uint32_array(input_filepath, output_header_path):
    if not os.path.exists(input_filepath):
        print(f"Error: File '{input_filepath}' not found.")
        return

    with open(input_filepath, 'rb') as f:
        raw_data = f.read()

    total_bytes = len(raw_data)
    print(f"Original File Size: {total_bytes} bytes")

    uint32_array = [total_bytes]

    padding_needed = (4 - (total_bytes % 4)) % 4
    if padding_needed > 0:
        raw_data += b'\x00' * padding_needed
        print(f"Padded data with {padding_needed} zero-bytes to align to 32-bit boundaries.")

    for i in range(0, len(raw_data), 4):
        chunk = raw_data[i:i+4]
        val = struct.unpack('<I', chunk)[0]
        uint32_array.append(val)


    with open(output_header_path, 'w') as f:
        f.write(f"// Generated from {os.path.basename(input_filepath)}\n")
        f.write(f"// First element [0] is the byte-size ({total_bytes})\n")
        f.write(f"static const uint32_t shader[{len(uint32_array)}] = {{\n")
        
        for i, val in enumerate(uint32_array):
            if i == 0:
                f.write(f"    0x{val:08X}, // <-- Size in bytes\n    ")
            else:
                f.write(f"0x{val:08X}, ")
                if i % 6 == 0:
                    f.write("\n    ")
                    
        f.write("\n};\n")
    print(f"Saved C header array to: {output_header_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert a raw binary file into a uint32 array format.")
    
    parser.add_argument("-i", "--input", default="my_gpu_code.bin", help="Path to the input binary file")
    parser.add_argument("-o", "--header-out", default="shader.h", help="Path for the output C header file")
    
    args = parser.parse_args()
    
    convert_bin_to_uint32_array(args.input, args.header_out)