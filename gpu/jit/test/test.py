import subprocess
from pathlib import Path
import os

# Create output directory for compiled SPIR-V
subprocess.run(["mkdir", "-p", "out"], capture_output=True, text=True)

# Test directory containing GLSL shaders
test_directory = Path("glsl")

# Find all GLSL files
glsl_files = list(test_directory.glob("*.vert"))

if not glsl_files:
    print("No GLSL files found in glsl/ directory")
    exit(1)

passed = 0
failed = 0

for glsl_file in glsl_files:
    name = glsl_file.stem
    print(f"\n{'='*50}")
    print(f"Processing: {name}")
    print(f"{'='*50}")
    
    # Compile GLSL to SPIR-V using glslc
    print(f"  Compiling {name}.glsl to SPIR-V...")
    result = subprocess.run(
        ["glslc", str(glsl_file), "-o", f"out/{name}.spv"],
        capture_output=True,
        text=True
    )
    
    if result.returncode != 0:
        print(f"  ✗ Failed to compile {name}")
        print(f"    Error: {result.stderr}")
        failed += 1
        continue
    
    print(f"  ✓ Compiled successfully")
    
    # Check for corresponding binary context file
    bin_file = test_directory / f"{name}.bin"
    if not bin_file.exists():
        print(f"  ✗ Missing context file: {name}.bin")
        failed += 1
        continue
    
    print(f"  Running test with {name}.bin...")
    
    # Execute JIT test with SPIR-V and binary context
    result = subprocess.run(
        ["./test_jit", f"out/{name}.spv", f"glsl/{name}.bin"],
        capture_output=True,
        text=True
    )
    
    if result.returncode != 0:
        print(f"  ✗ Test failed for {name}")
        print(f"    Output: {result.stdout}")
        print(f"    Error: {result.stderr}")
        failed += 1
    else:
        print(f"  ✓ Test passed")
        if result.stdout:
            print(f"    {result.stdout.strip()}")
        passed += 1

print(f"\n\n{'='*50}")
print(f"Test Results: {passed} passed, {failed} failed out of {len(glsl_files)} total")
print(f"{'='*50}\n")

# Exit with error code if any tests failed
exit(0 if failed == 0 else 1)
