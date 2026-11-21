import argparse
import os
import struct


def convert_bmp_to_array(input_filepath, output_header_path):
    if not os.path.exists(input_filepath):
        print(f"Error: File '{input_filepath}' not found.")
        return

    with open(input_filepath, 'rb') as f:
        data = f.read()

    # ------------------------------------------------------------
    # BMP FILE HEADER
    # ------------------------------------------------------------

    if len(data) < 54:
        print("Error: File is too small to be a valid BMP.")
        return

    if data[0:2] != b'BM':
        print("Error: Not a BMP file.")
        return

    pixel_offset = struct.unpack_from('<I', data, 10)[0]

    # ------------------------------------------------------------
    # DIB HEADER
    # ------------------------------------------------------------

    dib_size = struct.unpack_from('<I', data, 14)[0]

    width = struct.unpack_from('<i', data, 18)[0]
    height = struct.unpack_from('<i', data, 22)[0]

    planes = struct.unpack_from('<H', data, 26)[0]
    bits_per_pixel = struct.unpack_from('<H', data, 28)[0]
    compression = struct.unpack_from('<I', data, 30)[0]

    # ------------------------------------------------------------
    # VALIDATE BMP
    # ------------------------------------------------------------

    if planes != 1:
        print("Error: Unsupported BMP planes.")
        return

    if bits_per_pixel != 24:
        print(
            f"Error: Only 24-bit BMP is supported. "
            f"Found {bits_per_pixel}-bit BMP."
        )
        return

    if compression != 0:
        print("Error: Compressed BMP is not supported.")
        return

    # Negative height means BMP is already top-down.
    bmp_top_down = height < 0
    height = abs(height)

    print(f"BMP Width       : {width}")
    print(f"BMP Height      : {height}")
    print(f"Bits Per Pixel  : {bits_per_pixel}")
    print(f"Compression     : {compression}")
    print(f"Pixel Data      : {pixel_offset}")
    print("Output Rotation : 180 degrees")

    # ------------------------------------------------------------
    # BMP ROW SIZE
    #
    # Every BMP row is padded to a multiple of 4 bytes.
    # ------------------------------------------------------------

    row_size = ((width * 3 + 3) // 4) * 4

    print(f"BMP Row Size    : {row_size} bytes")

    # ------------------------------------------------------------
    # READ PIXELS
    #
    # 180° ROTATION
    #
    # Original:
    #
    #   A B C
    #   D E F
    #   G H I
    #
    # Output:
    #
    #   I H G
    #   F E D
    #   C B A
    #
    # The BMP itself stores:
    #
    #   B G R
    #
    # We output:
    #
    #   R G B
    # ------------------------------------------------------------

    pixels = []

    for y in range(height):

        # --------------------------------------------------------
        # BMP normally stores rows bottom-to-top.
        #
        # For 180° rotation, we want the visual bottom row first.
        #
        # For a normal BMP:
        #   bmp_y = y
        #
        # For a top-down BMP:
        #   bmp_y = height - 1 - y
        # --------------------------------------------------------

        if bmp_top_down:
            bmp_y = height - 1 - y
        else:
            bmp_y = y

        row_start = pixel_offset + bmp_y * row_size

        # --------------------------------------------------------
        # Read pixels RIGHT -> LEFT
        # --------------------------------------------------------

        for x in range(width - 1, -1, -1):

            pixel_position = row_start + x * 3

            # BMP = BGR
            b = data[pixel_position]
            g = data[pixel_position + 1]
            r = data[pixel_position + 2]

            # Output = RGB
            pixels.append(r)
            pixels.append(g)
            pixels.append(b)

    # ------------------------------------------------------------
    # WRITE C HEADER
    # ------------------------------------------------------------

    with open(output_header_path, 'w') as f:

        f.write(
            f"// Generated from {os.path.basename(input_filepath)}\n"
        )

        f.write(f"// Width  : {width}\n")
        f.write(f"// Height : {height}\n")
        f.write("// Format : RGB888\n")
        f.write("// Rotation: 180 degrees\n")
        f.write("// Pixel order: bottom-right -> top-left\n\n")

        f.write("#include <stdint.h>\n\n")

        f.write(f"#define IMAGE_WIDTH  {width}\n")
        f.write(f"#define IMAGE_HEIGHT {height}\n")
        f.write("#define IMAGE_CHANNELS 3\n\n")

        f.write(
            f"static const uint8_t image[{len(pixels)}] = {{\n"
        )

        # --------------------------------------------------------
        # Print 12 RGB pixels per line
        # 12 pixels * 3 channels = 36 numbers
        # --------------------------------------------------------

        for i, value in enumerate(pixels):

            f.write(f"{value}")

            if i != len(pixels) - 1:
                f.write(", ")

        f.write("};\n")

    # ------------------------------------------------------------
    # SUMMARY
    # ------------------------------------------------------------

    print()
    print(f"Saved C header : {output_header_path}")
    print(f"Image size     : {width} x {height}")
    print(f"Pixel count    : {width * height}")
    print(f"Array elements : {len(pixels)}")
    print(f"Array bytes    : {len(pixels)}")


if __name__ == "__main__":

    parser = argparse.ArgumentParser(
        description="Convert a 24-bit BMP into a 180-degree rotated RGB888 C array."
    )

    parser.add_argument(
        "-i",
        "--input",
        default="img.bmp",
        help="Path to the input BMP file"
    )

    parser.add_argument(
        "-o",
        "--header-out",
        default="img.h",
        help="Path to the output C header file"
    )

    args = parser.parse_args()

    convert_bmp_to_array(
        args.input,
        args.header_out
    )