# Jitter JIT

Jitter is GPUemu's LLVM-backed SPIR-V JIT. It translates a shader into one
LLVM function that evaluates **16 lanes at once**. The implementation targets
the shader subset used by GPUemu's UEFI demos and JIT tests; it is not intended
to be a complete SPIR-V runtime.

## Current status

The JIT test suite covers vertex, fragment, and compute shaders. The latest
run passes **20/20 tests**:

| Area | Validated coverage |
| --- | --- |
| Vertex | Attributes, `gl_Position`, matrices, loops, and selections |
| Fragment | Varyings, `gl_FragCoord`, samplers, filtering, and wrapping |
| Compute | Buffers, integer arithmetic, bitwise operations, atomics, barriers, and image stores |
| Subgroups | Add reduction, elect, ballot, and shuffle |
| GLSL.std.450 | Trigonometric, geometric, interpolation, and scalar math helpers |

Run the suite from the repository root:

```sh
make -C gpu/jit
cd gpu/jit/test
./test_jit
```

The test runner recompiles the GLSL fixtures before executing the JIT tests.

## SPIR-V support

The dispatcher in `gpu/jit/jit.c` currently has handlers for these families:

- Types and constants: void, bool, integer, float, vectors, matrices, arrays,
  structs, pointers, functions, constants, and composite constants.
- Shader structure: entry points, execution modes, labels, branches, selection
  and loop control, phi values, returns, variables, loads, stores, and access
  chains.
- Arithmetic: integer and floating-point add, subtract, multiply, divide,
  modulo/remainder, negate, comparisons, select, logical operations,
  conversions, and bitcasts.
- Composites and matrices: construction, extraction, vector shuffle, dot,
  vector-times-scalar, and matrix-times-vector operations.
- Resources: sampled images, image fetch/read/write, image size queries, and
  2D texture sampling.
- Parallel execution: control/memory barriers, atomics, subgroup reductions,
  elect, ballot, broadcast, and shuffle operations.
- GLSL.std.450: trigonometric, exponential, interpolation, vector geometry,
  reflection, and refraction helpers.

### Implemented core opcodes

The following SPIR-V opcodes are present in the dispatcher. Names are grouped
by the implementation area rather than listed in numeric opcode order.

**Module, type, and structure**

`OpName`, `OpMemberDecorate`, `OpDecorate`, `OpEntryPoint`,
`OpExecutionMode`, `OpTypeVoid`, `OpTypeBool`, `OpTypeInt`, `OpTypeFloat`,
`OpTypeVector`, `OpTypeMatrix`, `OpTypeArray`, `OpTypeStruct`,
`OpTypePointer`, `OpTypeFunction`, `OpTypeImage`, `OpTypeSampler`,
`OpTypeSampledImage`, `OpConstant`, `OpConstantComposite`, `OpVariable`,
`OpFunction`, `OpFunctionEnd`, `OpLabel`, `OpReturn`, `OpReturnValue`,
`OpKill`, `OpUnreachable`, and `OpExtInst`.

**Memory and composites**

`OpLoad`, `OpStore`, `OpAccessChain`, `OpCompositeConstruct`,
`OpCompositeExtract`, `OpVectorShuffle`, `OpSampledImage`, `OpImage`, and
`OpPhi`.

**Control flow and synchronization**

`OpBranch`, `OpBranchConditional`, `OpSelectionMerge`, `OpLoopMerge`,
`OpControlBarrier`, and `OpMemoryBarrier`.

**Arithmetic and conversion**

`OpIAdd`, `OpISub`, `OpIMul`, `OpSDiv`, `OpUDiv`, `OpUMod`, `OpSRem`,
`OpSMod`, `OpSNegate`, `OpFAdd`, `OpFSub`, `OpFMul`, `OpFDiv`, `OpFMod`,
`OpFNegate`, `OpVectorTimesScalar`, `OpDot`, `OpMatrixTimesVector`,
`OpConvertSToF`, `OpConvertFToS`, `OpConvertFToU`, `OpConvertUToF`,
`OpBitcast`, `OpBitwiseAnd`, `OpBitwiseOr`, `OpBitwiseXor`, `OpNot`,
`OpShiftLeftLogical`, `OpShiftRightLogical`, and `OpShiftRightArithmetic`.

**Comparisons and logical operations**

`OpIEqual`, `OpINotEqual`, `OpSLessThan`, `OpULessThan`,
`OpSLessThanEqual`, `OpULessThanEqual`, `OpSGreaterThan`,
`OpUGreaterThan`, `OpSGreaterThanEqual`, `OpUGreaterThanEqual`,
`OpFOrdLessThan`, `OpFOrdGreaterThan`, `OpSelect`, `OpLogicalAnd`,
`OpLogicalOr`, `OpLogicalNot`, `OpLogicalEqual`, `OpLogicalNotEqual`,
`OpAny`, `OpAll`, `OpIsNan`, and `OpIsInf`.

**Images and sampling**

`OpImageSampleImplicitLod`, `OpImageSampleExplicitLod`, `OpImageFetch`,
`OpImageQuerySize`, `OpImageQuerySizeLod`, `OpImageRead`, and
`OpImageWrite`.

**Atomics**

`OpAtomicLoad`, `OpAtomicStore`, `OpAtomicExchange`,
`OpAtomicCompareExchange`, `OpAtomicCompareExchangeWeak`,
`OpAtomicIIncrement`, `OpAtomicIDecrement`, `OpAtomicIAdd`, `OpAtomicISub`,
`OpAtomicSMin`, `OpAtomicUMin`, `OpAtomicSMax`, `OpAtomicUMax`,
`OpAtomicAnd`, `OpAtomicOr`, and `OpAtomicXor`.

**Subgroups**

`OpGroupNonUniformElect`, `OpGroupNonUniformAll`, `OpGroupNonUniformAny`,
`OpGroupNonUniformAllEqual`, `OpGroupNonUniformBroadcast`,
`OpGroupNonUniformBroadcastFirst`, `OpGroupNonUniformBallot`,
`OpGroupNonUniformInverseBallot`, `OpGroupNonUniformBallotBitExtract`,
`OpGroupNonUniformBallotBitCount`, `OpGroupNonUniformBallotFindLSB`,
`OpGroupNonUniformBallotFindMSB`, `OpGroupNonUniformShuffle`,
`OpGroupNonUniformShuffleXor`, `OpGroupNonUniformShuffleUp`,
`OpGroupNonUniformShuffleDown`, `OpGroupNonUniformIAdd`,
`OpGroupNonUniformFAdd`, `OpGroupNonUniformIMul`, `OpGroupNonUniformFMul`,
`OpGroupNonUniformSMin`, `OpGroupNonUniformUMin`, `OpGroupNonUniformFMin`,
`OpGroupNonUniformSMax`, `OpGroupNonUniformUMax`, `OpGroupNonUniformFMax`,
`OpGroupNonUniformBitwiseAnd`, `OpGroupNonUniformBitwiseOr`,
`OpGroupNonUniformBitwiseXor`, `OpGroupNonUniformLogicalAnd`,
`OpGroupNonUniformLogicalOr`, `OpGroupNonUniformLogicalXor`,
`OpGroupNonUniformQuadBroadcast`, `OpGroupNonUniformQuadSwap`,
`OpSubgroupBallotKHR`, `OpSubgroupFirstInvocationKHR`, `OpSubgroupAllKHR`,
`OpSubgroupAnyKHR`, `OpSubgroupAllEqualKHR`, and
`OpSubgroupReadInvocationKHR`.

This is an implementation inventory, not a conformance claim. An opcode may
only support the type shapes and storage classes exercised by current tests.

## Known limitations

- Only a single shader function is currently supported.
- The backend is specialized for `SIMT_WIDTH == 16`.
- Many integer registers use `<16 x float>` storage. Bitwise operations must
  preserve bit patterns, while indices, shift counts, modulo, and division use
  numeric integer conversion.
- Resource descriptors are GPUemu host-side descriptors, not Vulkan descriptors.
- Image support is currently centered on 2D host buffers and sampler helpers.
- Function parameters/calls, matrix multiplication, vector-times-matrix, wider
  image formats/dimensions, and broader multi-function control flow remain
  follow-up work where they are not covered by tests.
- Unsupported instructions are reported by the dispatcher and should receive a
  focused test before being marked complete.

## Implementation map

| Concern | Primary files |
| --- | --- |
| Opcode dispatch and JIT lifecycle | `gpu/jit/jit.c`, `gpu/jit/jit.h` |
| Arithmetic, conversions, comparisons, subgroups | `gpu/jit/jit_alu.c` |
| Control flow and execution masks | `gpu/jit/jit_flow.c` |
| Loads, stores, access chains, memory layout | `gpu/jit/jit_mem.c` |
| Sampling and image operations | `gpu/jit/jit_smpl.c` |
| GLSL.std.450 mapping | `gpu/jit/glsl_std_450.h` and `gpu/jit/jit_alu.c` |
| End-to-end tests | `gpu/jit/jit_test.c`, `gpu/jit/test/glsl/` |

Generated SPIR-V binaries live under `gpu/jit/test/out/` and should be
regenerated through the test runner rather than edited by hand.

## Design rules

When adding an opcode:

1. Confirm its operand and result types in the SPIR-V specification.
2. Decide whether each operand is a bit pattern or a numeric value. This is
   important because the register backend stores many integers as float bits.
3. Preserve the execution mask for stores, atomics, image writes, and subgroup
   operations.
4. Add a small shader test that exercises the path across multiple lanes.
5. Run the complete JIT suite before updating this document.

## TODO priorities

- Add explicit support tracking and tests for function parameters and calls.
- Complete matrix multiplication and vector-times-matrix operations.
- Expand image formats, dimensions, and access qualifiers.
- Improve diagnostics for unsupported opcodes and invalid type combinations.
- Generate coverage data from the dispatcher and test suite instead of keeping
  a manually counted opcode table.

## SIMT memory layout

The JIT uses a 16-lane SIMT vector backend. A scalar is represented as one
LLVM vector, and a composite is an LLVM array of those vectors.

### Scalar

```text
LLVM type: <16 x float>
Size:      64 bytes
Alignment: 64 bytes
Layout:    [lane 0][lane 1] ... [lane 15]
```

```c
typedef struct {
    float lane[16];
} SimtFloat;
```

Scalar constants are broadcast to every lane. Values loaded from buffers may
instead differ per lane.

### Vectors

`vecN` is stored as `[N x <16 x float>]`: each component has its own 16-lane
vector. Components are not interleaved.

| Type | LLVM type | Size |
| --- | --- | ---: |
| `vec2` | `[2 x <16 x float>]` | 128 bytes |
| `vec3` | `[3 x <16 x float>]` | 192 bytes |
| `vec4` | `[4 x <16 x float>]` | 256 bytes |

For a `vec3`, `vector[0]` contains all X values, `vector[1]` all Y values,
and `vector[2]` all Z values.

### Matrices

Matrices are column-major arrays of column vectors. A `matN` is represented as
`[N x [N x <16 x float>]]`; each matrix element is a complete 16-lane vector.

| Type | LLVM type | Size |
| --- | --- | ---: |
| `mat2` | `[2 x [2 x <16 x float>]]` | 256 bytes |
| `mat3` | `[3 x [3 x <16 x float>]]` | 576 bytes |
| `mat4` | `[4 x [4 x <16 x float>]]` | 1024 bytes |

For a matrix element at column `c`, row `r`, and lane `l`, the byte offset is:

```text
c * (N * 64) + r * 64 + l * 4
```

### Masked memory operations

Control flow maintains an execution mask with one boolean per lane. Stores,
atomics, and image writes update only active lanes. This keeps divergent
branches from modifying inactive lanes or their associated memory.

## Performance notes

- One JIT function processes 16 work items in parallel.
- 64-byte SIMT vectors are aligned for efficient host loads and stores.
- LLVM may lower the vector operations to host SIMD instructions.
- Array-of-vectors storage favors component-wise operations and predictable
  lane access over interleaved scalar layout.
