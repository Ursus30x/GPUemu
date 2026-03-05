import subprocess
from pathlib import Path

subprocess.run(["mkdir", "out"], capture_output=True, text=True)

test_directory = Path("spirv")

files = list(test_directory.glob("*.spvasm"))
passed = 0
for file in files:
    name = file.stem
    print(f"Compiling {name}")
    result = subprocess.run(["spirv-as", str(file), "-o", f"out/{name}.spv"], capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Failed to compile {name}: {result.stderr}")
        continue
    print(f"Testing {name}")
    result = subprocess.run(["./test_jit", f"out/{name}.spv", f"spirv/{name}.out"], capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Test failed for {name}: {result.stderr}")
        print(result.stdout)
        print(result.stderr)
    else:
        print(f"Test passed for {name}: {result.stdout}")
        passed += 1
print(f"\n\n\n\n")
print("=============================")
print(f"Passed {passed}/{len(files)} tests")
print("=============================")
#subprocess.run(["rm", "-rf", "out"], capture_output=True, text=True)
