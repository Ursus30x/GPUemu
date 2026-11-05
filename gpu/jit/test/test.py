import subprocess
from pathlib import Path
import sys

# Create output directory for compiled SPIR-V
subprocess.run(["mkdir", "-p", "out"], capture_output=True, text=True)

# Directory containing GLSL shaders
test_directory = Path("glsl")

# Find all GLSL files
glsl_files = list(test_directory.glob("*.vert")) + list(test_directory.glob("*.frag")) + list(test_directory.glob("*.comp"))

if not glsl_files:
    print("No GLSL files found in glsl/ directory")
    sys.exit(1)

passed = 0
failed = 0

for glsl_file in glsl_files:
    name = glsl_file.stem

    
    result = subprocess.run(
        ["glslc", "--target-env=vulkan1.1", str(glsl_file), "-o", f"out/{name}.spv"],
        capture_output=True,
        text=True
    )
    
    if result.returncode != 0:
        print(f"{'='*50}")
        print(f"Failed to compile {name}")
        print(f"Error: {result.stderr.strip()}")
        failed += 1
        print(f"{'='*50}")
    else:
        passed += 1

print(f"{'='*60}")
print(f"Compilation Results: {passed} succeeded, {failed} failed out of {len(glsl_files)} total")
print(f"{'='*60}")

sys.exit(0 if failed == 0 else 1)