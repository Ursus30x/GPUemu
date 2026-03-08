# Jitter TO-DO list

## Spir-V operations

| Opcode | Name                     | Count | Completed | Difficulty | Must Have |
|--------|--------------------------|-------|------------|------------|------------|
| 3      | OpSource                 | 6     | ✅ | 🟢 Easy   | ❌ |
| 5      | OpName                   | 147   | ✅ | 🟢 Easy   | ❌ |
| 6      | OpMemberName             | 23    | ✅ | 🟢 Easy   | ❌ |
| 11     | OpExtInstImport          | 6     | ✅ | 🟡 Medium | ❌ |
| 12     | OpExtInst                | 90    | ❌ | 🔴 Hard   | ⭐ |
| 14     | OpMemoryModel            | 6     | ❌ | 🟢 Easy   | ⭐ |
| 15     | OpEntryPoint             | 6     | ❌ | 🟡 Medium | ⭐ |
| 16     | OpExecutionMode          | 3     | ❌ | 🟡 Medium | ⭐ |
| 17     | OpCapability             | 6     | ❌ | 🟢 Easy   | ⭐ |
| 19     | OpTypeVoid               | 6     | ✅ | 🟢 Easy   | ⭐ |
| 20     | OpTypeBool               | 3     | ✅ | 🟢 Easy   | ⭐ |
| 21     | OpTypeInt                | 11    | ✅ | 🟢 Easy   | ⭐ |
| 22     | OpTypeFloat              | 6     | ✅ | 🟢 Easy   | ⭐ |
| 23     | OpTypeVector             | 15    | ✅ | 🟡 Medium | ⭐ |
| 24     | OpTypeMatrix             | 6     | ❌ | 🟡 Medium | ⭐ |
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
| 145    | OpMatrixTimesVector      | 6     | ❌ | 🔴 Hard   | ⭐ |
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
| 253    | OpReturn                 | 6     | ❌ | 🟢 Easy   | ⭐ |
| 254    | OpReturnValue            | 7     | ✅ | 🟢 Easy   | ⭐ |

Status: 41 / 62 (4 unused)

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