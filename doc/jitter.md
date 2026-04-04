# Jitter TO-DO list

## Spir-V operations

| Opcode | Name                     | Count | Completed | Difficulty | Must Have |
|--------|--------------------------|-------|------------|------------|------------|
| 3      | OpSource                 | 6     | ✅ | 🟢 Easy   | ❌ |
| 5      | OpName                   | 147   | ✅ | 🟢 Easy   | ❌ |
| 6      | OpMemberName             | 23    | ✅ | 🟢 Easy   | ❌ |
| 11     | OpExtInstImport          | 6     | ✅ | 🟡 Medium | ❌ |
| 12     | OpExtInst                | 90    | ✅ | 🔴 Hard   | ⭐ |
| 14     | OpMemoryModel            | 6     | ❌ | 🟢 Easy   | ⭐ |
| 15     | OpEntryPoint             | 6     | ❌ | 🟡 Medium | ⭐ |
| 16     | OpExecutionMode          | 3     | ❌ | 🟡 Medium | ⭐ |
| 17     | OpCapability             | 6     | ❌ | 🟢 Easy   | ⭐ |
| 19     | OpTypeVoid               | 6     | ✅ | 🟢 Easy   | ⭐ |
| 20     | OpTypeBool               | 3     | ✅ | 🟢 Easy   | ⭐ |
| 21     | OpTypeInt                | 11    | ✅ | 🟢 Easy   | ⭐ |
| 22     | OpTypeFloat              | 6     | ✅ | 🟢 Easy   | ⭐ |
| 23     | OpTypeVector             | 15    | ✅ | 🟡 Medium | ⭐ |
| 24     | OpTypeMatrix             | 6     | ✅ | 🟡 Medium | ⭐ |
| 25     | OpTypeImage              | 2     | ❌ | 🔴 Hard   | ⭐ |
| 27     | OpTypeSampledImage       | 2     | ❌ | 🔴 Hard   | ⭐ |
| 28     | OpTypeArray              | 3     | ✅ | 🟡 Medium | ⭐ |
| 30     | OpTypeStruct             | 7     | ✅ | 🟡 Medium | ⭐ |
| 32     | OpTypePointer            | 51    | ✅ | 🟡 Medium | ⭐ |
| 33     | OpTypeFunction           | 12    | ✅ | 🟢 Easy   | ⭐ |
| 43     | OpConstant               | 117   | ✅ | 🟢 Easy   | ⭐ |
| 44     | OpConstantComposite      | 26    | ❌ | 🟡 Medium | ⭐ |
| 54     | OpFunction               | 12    | ✅ | 🟡 Medium | ⭐ |
| 55     | OpFunctionParameter      | 14    | ❌ | 🟢 Easy   | ⭐ |
| 56     | OpFunctionEnd            | 12    | ✅ | 🟢 Easy   | ⭐ |
| 57     | OpFunctionCall           | 12    | ❌ | 🔴 Hard   | ⭐ |
| 59     | OpVariable               | 116   | ✅ | 🟡 Medium | ⭐ |
| 61     | OpLoad                   | 262   | ✅ | 🟡 Medium | ⭐ |
| 62     | OpStore                  | 138   | ✅ | 🟡 Medium | ⭐ |
| 65     | OpAccessChain            | 63    | ✅ | 🔴 Hard   | ⭐ |
| 71     | OpDecorate               | 37    | ✅ | 🟢 Easy   | ⭐ |
| 72     | OpMemberDecorate         | 31    | ✅ | 🟢 Easy   | ⭐ |
| 79     | OpVectorShuffle          | 21    | ❌ | 🟡 Medium | ⭐ |
| 80     | OpCompositeConstruct     | 62    | ✅ | 🟡 Medium | ⭐ |
| 81     | OpCompositeExtract       | 45    | ✅ | 🟡 Medium | ⭐ |
| 87     | OpImageSampleImplicitLod | 6     | ❌ | 🔴 Hard   | ⭐ |
| 111    | OpConvertSToF            | 5     | ✅ | 🟢 Easy   | ⭐ |
| 127    | OpFNegate                | 9     | ✅ | 🟢 Easy   | ⭐ |
| 128    | OpIAdd                   | 5     | ✅ | 🟢 Easy   | ⭐ |
| 129    | OpFAdd                   | 31    | ✅ | 🟢 Easy   | ⭐ |
| 130    | OpISub                   | 1     | ✅ | 🟢 Easy   | ⭐ |
| 131    | OpFSub                   | 30    | ✅ | 🟢 Easy   | ⭐ |
| 133    | OpFMul                   | 40    | ✅ | 🟢 Easy   | ⭐ |
| 136    | OpFDiv                   | 24    | ✅ | 🟢 Easy   | ⭐ |
| 141    | OpFMod                   | 5     | ✅ | 🟡 Medium | ⭐ |
| 142    | OpVectorTimesScalar      | 30    | ✅ | 🟡 Medium | ⭐ |
| 144    | OpVectorTimesMatrix      | 3     | ❌ | 🔴 Hard   | ⭐ |
| 145    | OpMatrixTimesVector      | 6     | ✅ | 🔴 Hard   | ⭐ |
| 146    | OpMatrixTimesMatrix      | 1     | ❌ | 🔴 Hard   | ⭐ |
| 148    | OpDot                    | 4     | ✅ | 🟡 Medium | ⭐ |
| 169    | OpSelect                 | 1     | ✅ | 🟢 Easy   | ⭐ |
| 177    | OpSLessThan              | 5     | ✅ | 🟢 Easy   | ⭐ |
| 184    | OpFOrdLessThan           | 6     | ✅ | 🟢 Easy   | ⭐ |
| 186    | OpFOrdGreaterThan        | 1     | ✅ | 🟢 Easy   | ⭐ |
| 246    | OpLoopMerge              | 5     | ❌ | 🔴 Hard   | ⭐ |
| 247    | OpSelectionMerge         | 6     | ❌ | 🔴 Hard   | ⭐ |
| 248    | OpLabel                  | 52    | ✅ | 🟡 Medium | ⭐ |
| 249    | OpBranch                 | 28    | ❌ | 🔴 Hard   | ⭐ |
| 250    | OpBranchConditional      | 11    | ❌ | 🔴 Hard   | ⭐ |
| 253    | OpReturn                 | 6     | ✅ | 🟢 Easy   | ⭐ |
| 254    | OpReturnValue            | 7     | ✅ | 🟢 Easy   | ⭐ |

Status: 45 / 62 (4 unused)

- We only support single function shaders

## Extended GLSL 450 operations

| Ext Set      | Name       | Count | Completed | Difficulty |
| ------------ | ---------- | ----- | --------- | ---------- |
| GLSL.std.450 | Sin        | 13    | ✅         | 🟢 Easy  |
| GLSL.std.450 | Cos        | 11    | ✅         | 🟢 Easy  |
| GLSL.std.450 | FAbs       | 9     | ✅         | 🟢 Easy    |
| GLSL.std.450 | Length     | 9     | ✅         | 🟡 Medium  |
| GLSL.std.450 | Normalize  | 8     | ✅         | 🟡 Medium  |
| GLSL.std.450 | FMax       | 7     | ✅         | 🟢 Easy    |
| GLSL.std.450 | FMin       | 6     | ✅         | 🟢 Easy    |
| GLSL.std.450 | FClamp     | 4     | ✅         | 🟢 Easy    |
| GLSL.std.450 | SmoothStep | 4     | ✅         | 🟢 Easy  |
| GLSL.std.450 | Sqrt       | 3     | ✅         | 🟢 Easy    |
| GLSL.std.450 | FMix       | 3     | ✅         | 🟡 Medium  |
| GLSL.std.450 | Reflect    | 2     | ✅         | 🔴 Hard    |
| GLSL.std.450 | Distance   | 2     | ✅         | 🟡 Medium  |
| GLSL.std.450 | Cross      | 2     | ✅         | 🟡 Medium  |
| GLSL.std.450 | Pow        | 2     | ✅         | 🟢 Easy  |
| GLSL.std.450 | Atan2      | 1     | ✅         | 🟢 Easy  |
| GLSL.std.450 | Refract    | 1     | ✅         | 🔴 Hard    |
| GLSL.std.450 | FSign      | 1     | ✅         | 🟢 Easy    |
| GLSL.std.450 | Step       | 1     | ✅         | 🟢 Easy    |
| GLSL.std.450 | Log        | 1     | ✅         | 🟢 Easy  |

Status: 20/20


## DOC

### Memory layout

The JIT compiler uses an SIMT (Single Instruction Multiple Threads) vector backend with a width of **16 lanes** (`SIMT_WIDTH = 16`).

#### Float (Scalar)
A single float value is broadcast to all 16 lanes:

```
Type: <16 x float>
Memory layout:
  [float0][float0][float0]...[float0]  (16 copies of the same value)
  └─ 16 lanes, each with identical float value
  
Size: 64 bytes (16 floats × 4 bytes/float)
Alignment: 64 bytes
```

**C equivalent:**
```c
typedef struct {
    float lane[16];  // All lanes contain same value
} SimtFloat;
```

---

#### Vector (e.g., vec3, vec4)
A vector with N components, where each component is a separate SIMT vector replicated across all 16 lanes.

```
Type: [N x <16 x float>]  (Array of N vectors, each with 16 lanes)

Memory layout for vec3:
  Component 0 (X):  [x0_l0][x0_l1][x0_l2]...[x0_l15]  (64 bytes)
                     ↑ lane 0, lane 1, lane 2... lane 15 of component X
  
  Component 1 (Y):  [y0_l0][y0_l1][y0_l2]...[y0_l15]  (64 bytes)
                     ↑ lane 0, lane 1, lane 2... lane 15 of component Y
  
  Component 2 (Z):  [z0_l0][z0_l1][z0_l2]...[z0_l15]  (64 bytes)
                     ↑ lane 0, lane 1, lane 2... lane 15 of component Z
  
Total: 3 × 64 = 192 bytes

Key insight: Each component value is duplicated across all 16 lanes for parallel execution
```

**C equivalent:**
```c
typedef struct {
    float lane[16];  // One component across 16 lanes
} SimtFloat;

typedef struct {
    SimtFloat x;     // Component 0 (64 bytes)
    SimtFloat y;     // Component 1 (64 bytes)
    SimtFloat z;     // Component 2 (64 bytes)
} vec3;
// Total: 3 * 64 = 192 bytes

// For vec4:
typedef struct {
    SimtFloat x, y, z, w;  // 4 components × 64 bytes = 256 bytes
} vec4;
```

---

#### Matrix (e.g., mat3x3 = 3 columns × 3 rows)
A matrix is stored as an array of **column vectors**. Each column is an array of `<16 x float>` SIMT vectors.

```
Type: [NumCols x [NumRows x <16 x float>]]

Memory layout for mat3 (3 columns, 3 rows):
  Column 0 (bytes 0-191):
    Row 0:  [m[0][0]_l0] [m[0][0]_l1] ... [m[0][0]_l15]  (64 bytes)
            ↑ Matrix element [row=0, col=0] across 16 lanes
    
    Row 1:  [m[1][0]_l0] [m[1][0]_l1] ... [m[1][0]_l15]  (64 bytes)
            ↑ Matrix element [row=1, col=0] across 16 lanes
    
    Row 2:  [m[2][0]_l0] [m[2][0]_l1] ... [m[2][0]_l15]  (64 bytes)
            ↑ Matrix element [row=2, col=0] across 16 lanes
  
  Column 1 (bytes 192-383):  (same structure, 192 bytes)
  Column 2 (bytes 384-575):  (same structure, 192 bytes)
  
Total: 3 columns * 3 rows * 64 = 576 bytes

Memory address layout:
  Base address:  [ Col0_Row0 | Col0_Row1 | Col0_Row2 | Col1_Row0 | Col1_Row1 | Col1_Row2 | Col2_Row0 | Col2_Row1 | Col2_Row2 ]
                 [  64 bytes | 64 bytes  | 64 bytes  | 64 bytes  | 64 bytes  | 64 bytes  | 64 bytes  | 64 bytes  | 64 bytes  ]
```

**C equivalent:**
```c
typedef struct {
    float lane[16];
} SimtFloat;

typedef struct {
    SimtFloat row[3];  // 3 rows per column, each 64 bytes = 192 bytes
} mat3_column;

typedef struct {
    mat3_column col[3];  // 3 columns, each 192 bytes = 576 bytes total
} mat3;

// For mat4 (4x4):
typedef struct {
    SimtFloat row[4];  // 4 rows = 256 bytes per column
} mat4_column;

typedef struct {
    mat4_column col[4];  // 4 columns = 1024 bytes total
} mat4;
```

**Column-major storage order:**
- Matrices follow **GLSL convention**: columns are primary, rows secondary
- `mat[col][row]` in GLSL code corresponds to `col[col].row[row].lane[...]` in memory
- This is ideal for matrix-vector multiplication where we iterate over columns
- Cache-friendly when accessing column vectors sequentially


---

#### Memory Layout Summary Table

| Type | LLVM Type | C Equivalent | Size (bytes) | Layout Details |
|------|-----------|--------------|--------------|----------------|
| float | `<16 x float>` | `float[16]` | 64 | 16 identical scalar copies across lanes |
| vec2 | `[2 x <16 x float>]` | `struct { SimtFloat x, y; }` | 128 | 2 components, each replicated 16× |
| vec3 | `[3 x <16 x float>]` | `struct { SimtFloat x, y, z; }` | 192 | 3 components, each replicated 16× |
| vec4 | `[4 x <16 x float>]` | `struct { SimtFloat x, y, z, w; }` | 256 | 4 components, each replicated 16× |
| mat2 | `[2 x [2 x <16 x float>]]` | `mat2_column[2]` | 256 | 2 cols × 2 rows × 64 = 256 bytes |
| mat3 | `[3 x [3 x <16 x float>]]` | `mat3_column[3]` | 576 | 3 cols × 3 rows × 64 = 576 bytes |
| mat4 | `[4 x [4 x <16 x float>]]` | `mat4_column[4]` | 1024 | 4 cols × 4 rows × 64 = 1024 bytes |

---

#### Key Characteristics

- **Lane-major layout**: Each scalar value exists in 16 copies across the lanes for SIMT execution
- **Component-wise storage**: Vector components are stored as separate SIMT arrays, not interleaved
- **Column-major matrices**: Matrices use column-primary ordering (standard GLSL convention)
- **Array-of-vectors representation**: Composites (vectors/matrices) are LLVM arrays containing SIMT vectors
- **64-byte alignment**: All SIMT vectors are 64-byte aligned for cache efficiency
- **Broadcast operations**: Scalar constants are automatically broadcast to all 16 lanes via `LLVMConstVector()`
- **Memory efficiency**: A single operation processes 16 work items simultaneously across all lanes
- **Cache locality**: Column-major storage aligns with typical GPU memory access patterns

#### Data Access Patterns

**Scalar access:**
```c
// Access x-component, lane 3
result = vector.x.lane[3];  // Get component X from lane 3
```

**Vector component extraction:**
```c
// Extract all lanes for one component
SimtFloat x_component = vector.x;  // All 16 lanes of X component
```

**Matrix element access:**
```c
// Access matrix[row][col], lane 5
float value = matrix.col[col].row[row].lane[5];
// Byte offset: col*192 + row*64 + 5*4
```

**Component broadcast:**
```c
// When a scalar is used in a vector operation
float scalar = 2.0f;
SimtFloat broadcasted;
for (int i = 0; i < 16; i++)
    broadcasted.lane[i] = scalar;  // All lanes get same value
```

**Masked operations** (from `jit_mem.c: build_masked_store`):
```c
// Store with lane mask for conditional writes
for (int lane = 0; lane < 16; lane++) {
    if (emask.lane[lane]) {  // Check execution mask
        *ptr = new_value.lane[lane];  // Conditional write
    }
}
```

#### Performance Implications

- **Vectorization**: LLVM can generate SIMD instructions (SSE, AVX) from vector operations
- **Lane independence**: Each lane executes independently with its own data
- **Parallel execution**: 16 work items compute in parallel within a single function invocation
- **Memory bandwidth**: 64-byte aligned vectors enable optimal cache line utilization