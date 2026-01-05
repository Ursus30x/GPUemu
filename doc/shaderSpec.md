# GPU Matrix Shader ISA Specification v1.0


### 1.1 Registers

The shader core exposes two register classes:

### **Matrix Registers (M0–M7)**

Eight general-purpose registers, each storing a **4×4 floating-point matrix**.

### **Scalar Registers (P0–P7)**

Eight general-purpose 32-bit scalar registers (int or float).

### **Special Registers**

| Register  | Description                   |
| --------- | ----------------------------- |
| **PX**    | Current pixel X coordinate    |
| **PY**    | Current pixel Y coordinate    |
| **PR**    | Current pixel red channel     |
| **PG**    | Current pixel green channel   |
| **PB**    | Current pixel blue channel    |
| **cFlag** | Condition flag (set by `CMP`) |
| **M_IN**  | Input vector (vertex position) |

### 1.2 Memory Model

The Shader segment is divided into two distinct segments within VRAM:

  * **Code Segment:** Read-only executable instructions.
  * **Data Segment:** Read-only matrix data (constants, uniforms).

**Program Control:**
The program entry point is defined by writing the offset from the shader segment base address to the **BAR0** register at `SHADER_ADDR`.



-----

# **2. Instruction Format**

Instructions are fixed-size structures. Operand interpretation depends on opcode and type flags.

## **2.1 C Structure**

```c
typedef struct Instr {
    uint8_t  opcode;        // Operation identifier
    uint8_t  cFlag;         // Condition flag handling (see below)
    uint8_t  dest;          // Destination register (M0–M7 or P0–P7 depending on op)
    uint8_t  arg0Type:2;    // 0=reg, 1=imm/data
    uint8_t  arg1Type:2;    // 0=reg, 1=imm/data
    uint8_t  arg2Type:2;    // 0=reg, 1=imm/data
    uint8_t  opType:2;      // Operand type: U32/F32/Matrix/Vec4/Vec3
    InstrArg arg0;          // Source operand 0
    InstrArg arg1;          // Source operand 1
    InstrArg arg2;          // Source operand 2
} Instr;
```

---

## **2.2 Argument Semantics**

Arguments (`arg0–arg2`) are 32-bit values interpreted according to:

### **Matrix (Data Segment)**

If a matrix is expected, the value is treated as a **byte offset into the Data Segment**.

### **Vector Operations**

For `OP_TYPE_VEC4` and `OP_TYPE_VEC3`:
  * Arguments are typically **M register indices** (matrix registers used as vector storage)
  * `M_IN` is a special register containing the input vector (e.g., vertex position)

### **Immediate Scalar**

For scalar instructions, values are **raw IEEE-754 floats** or **32-bit integers** depending on `opType`.

---

## **2.3 Conditional Execution (cFlag Field)**

If `cFlag != 0` (C_FLAG_ENABLE), instruction execution is **gated** by the value of the runtime `cFlag` register:

| cFlag reg | Behavior               |
| --------- | ---------------------- |
| **1**     | Instruction executes   |
| **0**     | Instruction is skipped |

If `cFlag == 0` (C_FLAG_DISABLE), the check is disabled.

**Exceptions:**
  - `CMP` always executes and sets `cFlag` to the comparison result.
  - `PCMP` always executes and writes the result to the destination register.

---

## **2.4 Source Type Flags**

`argXType` determines interpretation of each source:

| Value | Meaning                                  |
| ----- | ---------------------------------------- |
| `0`   | Register operand                         |
| `1`   | Immediate scalar or Data Segment address |

`opType` determines operand datatype:

| Value    | Meaning  |
| -------- | -------- |
| `0`      | `U32`    |
| `1`      | `F32`    |
| `2`      | `Matrix` |
| `3`      | `Vec4`   |
| `4`      | `Vec3`   |

Most instructions require a specific `opType`.
Scalar instructions use `i`/`f` suffixes, matrix instructions use `m`, vector instructions use `v3`/`v4`.


-----

## 3\. Instruction Set Reference

### Summary Table

| Opcode | Mnemonic | Operands       | Description                           |
| ------ | -------- | -------------- | ------------------------------------- |
| 0      | `MOV`    | dst, src       | Move matrix or scalar                 |
| 1      | `MUL`    | dst, s0, s1    | Multiply (mat, scalar, or vector)     |
| 2      | `ROTX`   | dst, angle     | Build X-axis rotation matrix          |
| 3      | `ROTY`   | dst, angle     | Build Y-axis rotation matrix          |
| 4      | `IDENT`  | dst            | Load identity matrix                  |
| 5      | `TRANS`  | dst, x, y, z   | Build translation matrix              |
| 6      | `MVP`    | dst            | Write matrix to hardware MVP register |
| 7      | `EXIT`   | –              | Terminate shader                      |
| 8      | `CMP`    | cond, s0, s1   | Compare → store result in `cFlag`     |
| 9      | `ADD`    | dst, s0, s1    | Add (scalar, matrix, or vector)       |
| 10     | `SUB`    | dst, s0, s1    | Subtract (scalar, matrix, or vector)  |
| 11     | `DIV`    | dst, s0, s1    | Divide (scalar or vector)             |
| 12     | `MOD`    | dst, s0, s1    | Modulo (u32 only)                     |
| 13     | `COL`    | dst            | Extract RGB from Vec3 → PR, PG, PB    |
| 14     | `FSAN`   | dst            | Float sanitize (NaN/inf→0)            |
| 15     | `BLEND`  | dst, c0, c1, t | Blend: c0*(1-t) + c1*t                |
| 16     | `LERP`   | dst, c0, c1, t | Linear interpolate: c0 + (c1-c0)*t    |
| 17     | `SQRT`   | dst, src       | Square root (f32)                     |
| 18     | `ABS`    | dst, src       | Absolute value (i32/f32)              |
| 19     | `SIN`    | dst, src       | Sine (f32)                            |
| 20     | `COS`    | dst, src       | Cosine (f32)                          |
| 21     | `CAST`   | dst, src       | Convert i32 ↔ f32                     |
| 22     | `LDU`    | dst, offset    | Load uniform (matrix/vec/scalar)      |
| 23     | `JMP`    | target         | Unconditional jump                    |
| 24     | `AND`    | dst, s0, s1    | Bitwise AND (u32)                     |
| 25     | `OR`     | dst, s0, s1    | Bitwise OR (u32)                      |
| 26     | `XOR`    | dst, s0, s1    | Bitwise XOR (u32)                     |
| 27     | `NOT`    | dst, src       | Bitwise NOT (u32)                     |
| 28     | `PCMP`   | cond, dst, s0, s1 | Compare → write result to dst      |
| 29     | `NORM`   | dst, src       | Normalize Vec3                        |
| 30     | `MIN`    | dst, s0, s1    | Minimum (i32/f32)                     |
| 31     | `MAX`    | dst, s0, s1    | Maximum (i32/f32)                     |
| 32     | `CLAMP`  | dst, v, min, max | Clamp to range (i32/f32)             |
| 33     | `NEG`    | dst, src       | Negate (i32/f32)                      |
| 34     | `RECIP`  | dst, src       | Reciprocal 1/x (i32/f32)              |
| 35     | `RSQRT`  | dst, src       | Reciprocal sqrt 1/sqrt(x) (f32)       |
| 36     | `DOT`    | dst, v0, v1    | Dot product Vec3 (f32 result)         |
| 37     | `CROSS`  | dst, v0, v1    | Cross product Vec3 (Vec3 result)      |
| 38     | `LEN`    | dst, src[, s2] | Length: Vec3 or 2D (f32)              |
| 39     | `FMA`    | dst, s0, s1, s2 | Fused multiply-add (i32/f32)         |
| 40     | `MAD`    | dst, s0, s1, s2 | Multiply-add (i32/f32)               |
| 41     | `SAT`    | dst, src       | Saturate [0,1] or [0,255] (i32/f32)   |
| 42     | `SIGN`   | dst, src       | Sign: -1, 0, +1 (i32/f32/vec3)        |
| 43     | `VEC3`   | dst, x, y, z   | Construct Vec3 from scalars           |
| 44     | `TAN`    | dst, src       | Tangent (f32)                         |
| 45     | `ATAN`   | dst, y, x      | Arc tangent atan2(y, x) (f32)         |
| 46     | `EXP`    | dst, src       | Exponential e^x (f32)                 |

---

### Vector and Matrix Operand Semantics

This section clarifies how vectors and matrices are stored, referenced, and manipulated in the shader ISA.

#### **Matrix Operations**

Matrices are 4×4 single-precision floating-point arrays stored in **M registers (M0–M7)**.

**Storage:**
```
typedef union {
    float m[4][4];        // Direct 4×4 access
    float elements[16];   // Linear 16-element array
    Vec4 rows[4];         // Four row vectors
    Vec3Raw vec3;         // Embedded 3D vector (position)
    struct {
        Vec4 right;       // Column 0
        Vec4 up;          // Column 1
        Vec4 forward;     // Column 2
        Vec4 position;    // Column 3
    };
} Mat4;
```

**Types:**
  - matrix (`m`) operates on `float m[4][4];`
  - vector 4x1 (`v4`) operates on `Vec4 right;`  
  - vector 3x1 (`v3`) operates on `Vec3Raw vec3;`  

**Typical Operations:**
- `ROTX`, `ROTY`, `TRANS`, `IDENT` — Create transformation matrices
- `MUL` (matrix × matrix) — Compose transformations
- `MUL` (matrix × Vec4) — Transform vertices
- `MVP` — Commit final transformation to hardware

---

#### **Vector4 Operations (Vec4)**

Vectors are stored in M registers alongside matrices. A Vec4 occupies one row/column of a matrix.

**Storage:**
```
typedef struct { float x, y, z, w; } Vec4;
```

**Special Register:**
- `M_IN` — Input vector register, typically the vertex position

**Typical Operations:**
- `MUL` (matrix × Vec4) — Matrix-vector multiplication
- `ADD`, `SUB`, `DIV` — Element-wise vector arithmetic
- Input via `M_IN` pseudo-register in vertex/fragment shaders

---

#### **Vector3 Operations (Vec3)**

Vec3 represents 3D vectors (e.g., colors, normals, positions) stored in M registers.

**Storage:**
```
typedef struct { float x, y, z; } Vec3Raw;
```

The Vec3 is embedded in the `.vec3` field of an M register's Mat4 union.

**Key Constraints:**
- **Destination:** Must use an **M register** for vec3 operations (MULv3, ADDv3, SUBv3, DIVv3)
- **First Argument:** Must be an **M register** containing a Vec3
- **Second Argument:** Can be a **scalar** (preg or immediate f32) for operations like `MULv3 M0, M1, 2.5`

**Example:**
```
; Scale a vector: M0 = M1 * 2.5
MULv3 M0 M1 2.5

; Add scalar to each component: M0 = M1 + p0
ADDv3 M0 M1 p0

; Normalize a vector
NORM M0  M1

; Construct a Vec3 from scalars
VEC3 M2 p0  p1 p2
```

**Typical Operations:**
- `VEC3` — Construct from three scalars
- `MULv3`, `ADDv3`, `SUBv3`, `DIVv3` — Element-wise scalar operations
- `DOT`, `CROSS` — Vector math (inputs are M registers, result is F32 or Vec3)
- `LEN` — Magnitude computation
- `NORM` — Normalization
- `SIGN` — Per-component sign
- `COL` — Extract RGB and convert to 8-bit color

---

#### **Register Assignment Summary**

| Operation Type  | Destination | Arg0 | Arg1 | Arg2 | Notes |
| --------------- | ----------- | ---- | ---- | ---- | ----- |
| **Scalar U32/F32** | Preg (P0–P7) | Preg/Imm | Preg/Imm | Preg/Imm | Standard arithmetic |
| **Matrix** | M register | M reg/Offset | M reg/Offset | – | Transformations, MVPs |
| **Vec4** | M register | M reg | M reg/Offset | – | Matrix × Vec4 transforms |
| **Vec3 (binary)** | M register | M register | Scalar/Imm | – | MULv3, ADDv3, etc. |
| **Vec3 (construction)** | M register | Scalar/Imm | Scalar/Imm | Scalar/Imm | VEC3 instruction |
| **Vec3 (dot)** | Preg | M register | M register | – | Result is F32 |
| **Vec3 (cross)** | M register | M register | M register | – | Result is Vec3 |

---

#### **Vector-Scalar Blending in Assembly**

When an instruction supports both vector and scalar operands (e.g., `MULv3`), the type suffix determines interpretation:

```
MULv3 M0 M1 p2       ; Vec3 in M1 * scalar in p2
MULv3 M0 M1 1.5      ; Vec3 in M1 * immediate float 1.5

ADDv3 M0 M1 p0       ; Vec3 in M1 + scalar in p0 (broadcasts)
```

Scalars are **broadcast** to all three components.

-----

# **3. Instruction Set Reference**

## **MOV** (Move)

* **Opcode:** `0`
* **Syntax:** `MOV[m|i|f] dst, src`
* **Description:** Copies the value from `src` into `dst`. Supports matrix and scalar transfers.
* **Types:** U32, F32, Matrix
* **Operands:**
  * `dst` — Destination register
  * `src` — Register, immediate scalar, or Data Segment matrix address

---

## **MUL** (Multiply)

* **Opcode:** `1`
* **Syntax:** `MUL[i|f|m|v3|v4] dst, src0, src1`
* **Description:** Performs multiplication. Supports matrix, scalar, and vector arithmetic.
* **Types:** U32, F32, Matrix, Vec3, Vec4
* **Operands:**
  * `dst` — Destination register (M register for vec ops)
  * `src0` — Register or Data Segment address
  * `src1` — Register or Data Segment address

**Vec3 Notes:** For `MULv3`, `src0` must be an M register; `src1` can be a scalar (preg or immediate f32).

---

## **ADD** (Addition)

* **Opcode:** `9`
* **Syntax:** `ADD[i|f|m|v3|v4] dst, src0, src1`
* **Description:** Computes addition. Supports scalar, matrix, and vector addition.
* **Types:** U32, F32, Matrix, Vec3, Vec4
* **Operands:**
  * `dst` — Destination register (M register for vec ops)
  * `src0` — Register or immediate
  * `src1` — Register or immediate

**Vec3 Notes:** For `ADDv3`, `src0` must be an M register; `src1` can be a scalar (preg or immediate f32).

---

## **SUB** (Subtraction)

* **Opcode:** `10`
* **Syntax:** `SUB[i|f|m|v3|v4] dst, src0, src1`
* **Description:** Computes subtraction.
* **Types:** U32, F32, Matrix, Vec3, Vec4
* **Operands:**
  * `dst` — Destination register (M register for vec ops)
  * `src0` — Register or immediate
  * `src1` — Register or immediate

**Vec3 Notes:** For `SUBv3`, `src0` must be an M register; `src1` can be a scalar (preg or immediate f32).

---

## **ROTX** (Rotation X)

* **Opcode:** `2`
* **Syntax:** `ROTX dst, angle`
* **Description:** Creates a 4×4 rotation matrix about the X-axis.
* **Type:** Matrix only
* **Operands:**
  * `dst` — Matrix register
  * `angle` — Immediate F32

---

## **ROTY** (Rotation Y)

* **Opcode:** `3`
* **Syntax:** `ROTY dst, angle`
* **Description:** Creates a 4×4 rotation matrix about the Y-axis.
* **Type:** Matrix only
* **Operands:**
  * `dst` — Matrix register
  * `angle` — Immediate F32

---

## **IDENT** (Identity Matrix)

* **Opcode:** `4`
* **Syntax:** `IDENT dst`
* **Description:** Writes a 4×4 identity matrix into `dst`.
* **Type:** Matrix only
* **Operands:**
  * `dst` — Matrix register

---

## **TRANS** (Translation Matrix)

* **Opcode:** `5`
* **Syntax:** `TRANS dst, x, y, z`
* **Description:** Creates a translation matrix from `(x, y, z)`.
* **Type:** Matrix only
* **Operands:**
  * `dst` — Matrix register
  * `x, y, z` — Immediate F32 or registers

---

## **MVP** (Commit Matrix)

* **Opcode:** `6`
* **Syntax:** `MVP dst`
* **Description:** Writes the matrix in `dst` to the hardware MVP (Model-View-Projection) unit.
* **Type:** Matrix only
* **Operands:**
  * `dst` — Matrix register

---

## **EXIT** (Terminate Shader)

* **Opcode:** `7`
* **Syntax:** `EXIT`
* **Description:** Stops shader execution.
* **Operands:** None

---

## **CMP** (Compare)

* **Opcode:** `8`
* **Syntax:** `CMP[i|f] cond, src0, src1`
* **Description:** Compares `src0` and `src1` using `cond` and writes result (0 or 1) into the runtime `cFlag`. Always executes.
* **Types:** U32, F32
* **Operands:**
  * `cond` — Comparison mode (EQ, NEQ, LT, GT, LTE, GTE)
  * `src0, src1` — Register or immediate

---

## **DIV** (Division)

* **Opcode:** `11`
* **Syntax:** `DIV[i|f|v3|v4] dst, src0, src1`
* **Description:** Computes scalar or vector division.
* **Types:** U32, F32, Vec3, Vec4
* **Operands:**
  * `dst` — Destination register
  * `src0, src1` — Register or immediate

---

## **MOD** (Modulo)

* **Opcode:** `12`
* **Syntax:** `MOD dst, src0, src1`
* **Description:** Computes integer modulo: `dst = src0 % src1`.
* **Type:** U32 only
* **Operands:**
  * `dst` — Scalar register
  * `src0, src1` — Register or immediate

---

## **COL** (Write Color Registers)

* **Opcode:** `13`
* **Syntax:** `COL dst` (where dst references a Vec3 in an M register)
* **Description:** Extracts RGB from a Vec3 stored in an M register and writes to PR, PG, PB as 8-bit values (clamped [0,1] → [0,255]).
* **Type:** Vec3
* **Operands:**
  * `dst` — M register containing Vec3 color

---

## **FSAN** (Float Sanitize)

* **Opcode:** `14`
* **Syntax:** `FSAN dst`
* **Description:** Replaces NaN or Inf in `dst` with `0.0`. Reads from and writes to the same register.
* **Type:** F32
* **Operands:**
  * `dst` — Scalar register

---

## **BLEND** (Blend)

* **Opcode:** `15`
* **Syntax:** `BLEND[i|f] dst, src0, src1, t`
* **Description:** Performs blending: `dst = src0*(1 - t) + src1*t`.
* **Types:** U32, F32
* **Operands:**
  * `dst` — Destination register
  * `src0, src1, t` — Register or immediate

---

## **LERP** (Linear Interpolation)

* **Opcode:** `16`
* **Syntax:** `LERP[i|f] dst, src0, src1, t`
* **Description:** Computes linear interpolation: `dst = src0 + (src1 − src0) * t`.
* **Types:** U32, F32
* **Operands:**
  * `dst` — Destination register
  * `src0, src1, t` — Register or immediate

---

## **SQRT** (Square Root)

* **Opcode:** `17`
* **Syntax:** `SQRT dst, src`
* **Description:** Computes square root of a float.
* **Type:** F32
* **Operands:**
  * `dst, src` — Scalar registers or immediates

---

## **ABS** (Absolute Value)

* **Opcode:** `18`
* **Syntax:** `ABS[i|f] dst, src`
* **Description:** Computes absolute value of integer or float.
* **Types:** U32, F32
* **Operands:**
  * `dst, src` — Scalar registers or immediates

---

## **SIN** (Sine)

* **Opcode:** `19`
* **Syntax:** `SIN dst, src`
* **Description:** Computes sine of a float value.
* **Type:** F32
* **Operands:**
  * `dst, src` — Scalar registers or immediates

---

## **COS** (Cosine)

* **Opcode:** `20`
* **Syntax:** `COS dst, src`
* **Description:** Computes cosine of a float value.
* **Type:** F32
* **Operands:**
  * `dst, src` — Scalar registers or immediates

---

## **CAST** (Scalar Type Conversion)

* **Opcode:** `21`
* **Syntax:** `CAST[i|f] dst, src`
* **Description:** Converts scalar between integer and float types.
* **Types:** U32 or F32
* **Operands:**
  * `dst, src` — Scalar registers

---

## **LDU** (Load Uniform)

* **Opcode:** `22`
* **Syntax:** `LDU[m|v|i|f] dst, offset`
* **Description:** Loads a value from the uniform buffer at `offset` bytes from the uniform base address.
* **Types:** Matrix, Vec4, U32, F32
* **Operands:**
  * `dst` — Destination register (M register for matrices/vectors)
  * `offset` — U32 immediate or register (always treated as u32)

---

## **JMP** (Jump)

* **Opcode:** `23`
* **Syntax:** `JMP target`
* **Description:** Unconditional jump to instruction at `target` (byte offset from program start).
* **Operands:**
  * `target` — Immediate address

---

## **AND** (Bitwise AND)

* **Opcode:** `24`
* **Syntax:** `AND dst, src0, src1`
* **Description:** Computes bitwise AND.
* **Type:** U32
* **Operands:**
  * `dst, src0, src1` — Scalar registers or immediates

---

## **OR** (Bitwise OR)

* **Opcode:** `25`
* **Syntax:** `OR dst, src0, src1`
* **Description:** Computes bitwise OR.
* **Type:** U32
* **Operands:**
  * `dst, src0, src1` — Scalar registers or immediates

---

## **XOR** (Bitwise XOR)

* **Opcode:** `26`
* **Syntax:** `XOR dst, src0, src1`
* **Description:** Computes bitwise XOR.
* **Type:** U32
* **Operands:**
  * `dst, src0, src1` — Scalar registers or immediates

---

## **NOT** (Bitwise NOT)

* **Opcode:** `27`
* **Syntax:** `NOT dst, src`
* **Description:** Computes bitwise NOT.
* **Type:** U32
* **Operands:**
  * `dst, src` — Scalar registers or immediates

---

## **PCMP** (Compare to Register)

* **Opcode:** `28`
* **Syntax:** `PCMP[i|f] cond, dst, src0, src1`
* **Description:** Compares `src0` and `src1` using `cond` and writes result (0 or 1) directly to `dst` register (does not affect `cFlag`). Always executes.
* **Types:** U32, F32
* **Operands:**
  * `cond` — Comparison mode (EQ, NEQ, LT, GT, LTE, GTE)
  * `dst` — Destination register (result written here)
  * `src0, src1` — Register or immediate

---

## **NORM** (Normalize Vector)

* **Opcode:** `29`
* **Syntax:** `NORM dst, src`
* **Description:** Normalizes a Vec3: divides by length. Stores normalized result in `dst` M register.
* **Type:** Vec3
* **Operands:**
  * `dst, src` — M registers

---

## **MIN** (Minimum)

* **Opcode:** `30`
* **Syntax:** `MIN[i|f] dst, src0, src1`
* **Description:** Returns minimum of two values.
* **Types:** U32, F32
* **Operands:**
  * `dst, src0, src1` — Scalar registers or immediates

---

## **MAX** (Maximum)

* **Opcode:** `31`
* **Syntax:** `MAX[i|f] dst, src0, src1`
* **Description:** Returns maximum of two values.
* **Types:** U32, F32
* **Operands:**
  * `dst, src0, src1` — Scalar registers or immediates

---

## **CLAMP** (Clamp Value)

* **Opcode:** `32`
* **Syntax:** `CLAMP[i|f] dst, val, min, max`
* **Description:** Clamps `val` to range [`min`, `max`].
* **Types:** U32, F32
* **Operands:**
  * `dst` — Destination register
  * `val, min, max` — Register or immediate

---

## **NEG** (Negate)

* **Opcode:** `33`
* **Syntax:** `NEG[i|f] dst, src`
* **Description:** Negates value: `dst = -src`.
* **Types:** U32, F32
* **Operands:**
  * `dst, src` — Scalar registers or immediates

---

## **RECIP** (Reciprocal)

* **Opcode:** `34`
* **Syntax:** `RECIP[i|f] dst, src`
* **Description:** Computes reciprocal: `dst = 1 / src`.
* **Types:** U32, F32
* **Operands:**
  * `dst, src` — Scalar registers or immediates

---

## **RSQRT** (Reciprocal Square Root)

* **Opcode:** `35`
* **Syntax:** `RSQRT dst, src`
* **Description:** Computes `dst = 1 / sqrt(src)`.
* **Type:** F32
* **Operands:**
  * `dst, src` — Scalar registers or immediates

---

## **DOT** (Dot Product)

* **Opcode:** `36`
* **Syntax:** `DOT dst, src0, src1`
* **Description:** Computes dot product of two Vec3 vectors: `dst = src0 · src1` (F32 result in scalar).
* **Type:** Vec3
* **Operands:**
  * `dst` — Scalar register (result)
  * `src0, src1` — M registers containing Vec3

---

## **CROSS** (Cross Product)

* **Opcode:** `37`
* **Syntax:** `CROSS dst, src0, src1`
* **Description:** Computes cross product of two Vec3 vectors: `dst = src0 × src1` (result in M register as Vec3).
* **Type:** Vec3
* **Operands:**
  * `dst` — M register (result)
  * `src0, src1` — M registers containing Vec3

---

## **LEN** (Length)

* **Opcode:** `38`
* **Syntax:** `LEN[v3|f] dst, src[, src2]`
* **Description:** Computes length (magnitude) of a vector. For F32, computes 2D length from two components.
* **Types:** Vec3, F32
* **Operands:**
  * **Vec3:** `dst` (scalar reg, result), `src` (M register containing Vec3)
  * **F32:** `dst` (scalar reg), `src` (x component), `src2` (y component)

---

## **FMA** (Fused Multiply-Add)

* **Opcode:** `39`
* **Syntax:** `FMA[i|f] dst, src0, src1, src2`
* **Description:** Computes fused multiply-add: `dst = src0 * src1 + src2`.
* **Types:** U32, F32
* **Operands:**
  * `dst, src0, src1, src2` — Scalar registers or immediates

---

## **MAD** (Multiply-Add)

* **Opcode:** `40`
* **Syntax:** `MAD[i|f] dst, src0, src1, src2`
* **Description:** Computes multiply-add: `dst = src0 * src1 + src2` (similar to FMA, may not be fused).
* **Types:** U32, F32
* **Operands:**
  * `dst, src0, src1, src2` — Scalar registers or immediates

---

## **SAT** (Saturate)

* **Opcode:** `41`
* **Syntax:** `SAT dst, src`
* **Description:** Saturates value to range [0, 1] for floats or [0, 255] for integers.
* **Types:** U32, F32
* **Operands:**
  * `dst, src` — Scalar registers or immediates

---

## **SIGN** (Sign)

* **Opcode:** `42`
* **Syntax:** `SIGN[i|f|v3] dst, src`
* **Description:** Returns sign of value: +1, 0, or -1.
* **Types:** U32, F32, Vec3
* **Operands:**
  * `dst, src` — Scalar registers (or M registers for Vec3)

---

## **VEC3** (Construct Vec3)

* **Opcode:** `43`
* **Syntax:** `VEC3 dst, x, y, z`
* **Description:** Constructs a Vec3 from three scalar components (pregs or immediate floats). Result stored in M register.
* **Type:** Vec3
* **Operands:**
  * `dst` — M register (result)
  * `x, y, z` — Scalar registers or immediate floats

---

## **TAN** (Tangent)

* **Opcode:** `44`
* **Syntax:** `TAN dst, src`
* **Description:** Computes tangent of a float value.
* **Type:** F32
* **Operands:**
  * `dst, src` — Scalar registers or immediates

---

## **ATAN** (Arc Tangent)

* **Opcode:** `45`
* **Syntax:** `ATAN dst, y, x`
* **Description:** Computes `atan2(y, x)` (two-argument arctangent).
* **Type:** F32
* **Operands:**
  * `dst` — Scalar register (result)
  * `y, x` — Scalar registers or immediates

---

## **EXP** (Exponential)

* **Opcode:** `46`
* **Syntax:** `EXP dst, src`
* **Description:** Computes `e^src`.
* **Type:** F32
* **Operands:**
  * `dst, src` — Scalar registers or immediates

---