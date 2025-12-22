

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
    uint8_t  dest;           // Destination register (M0–M7 or P0–P7 depending on op)
    uint8_t  cFlag;         // Condition flag handling (see below)
    uint8_t  arg0Type:2;    // 0=reg, 1=imm/data
    uint8_t  arg1Type:2;    // 0=reg, 1=imm/data
    uint8_t  arg2Type:2;    // 0=reg, 1=imm/data
    uint8_t  opType:2;      // Operand type: I32/F32/Matrix
    uint32_t arg0;          // Source operand 0
    uint32_t arg1;          // Source operand 1
    uint32_t arg2;          // Source operand 2
} Instr;
```

---

## **2.2 Argument Semantics**

Arguments (`arg0–arg2`) are 32-bit values interpreted according to:

### **Matrix (Data Segment)**

If MSB = `0` and a matrix is expected, the value is treated as a **byte offset into the Data Segment**.

### **Immediate Scalar**

For scalar instructions, values are **raw IEEE-754 floats** or **32-bit integers** depending on `opType`.

---

## **2.3 Conditional Execution (cFlag Field)**

If `cFlag = 1`, instruction execution is **gated** by the value of the runtime `cFlag` register:

| cFlag reg | Behavior               |
| --------- | ---------------------- |
| **1**     | Instruction executes   |
| **0**     | Instruction is skipped |

If `cFlag = 0`, the check is disabled.

**Exception:**
`CMP` always executes and overwrites `cFlag`.

---

## **2.4 Source Type Flags**

`argXType` determines interpretation of each source:

| Value | Meaning                                  |
| ----- | ---------------------------------------- |
| `0`   | Register operand                         |
| `1`   | Immediate scalar or Data Segment address |

`opType` determines operand datatype:

| Value | Meaning  |
| ----- | -------- |
| `0`   | `I32`    |
| `1`   | `F32`    |
| `2`   | `Matrix` |

Most instructions require a specific `opType`.
Scalar instructions use `i`/`f` suffixes, matrix instructions use `m`.


-----

## 3\. Instruction Set Reference

### Summary Table

| Opcode | Mnemonic | Operands       | Description                           |
| ------ | -------- | -------------- | ------------------------------------- |
| 0      | `MOV`    | dst, src       | Move matrix or scalar                 |
| 1      | `MUL`    | dst, s0, s1    | Multiply (mat or scalar)              |
| 2      | `ROTX`   | dst, angle     | Build X-axis rotation matrix          |
| 3      | `ROTY`   | dst, angle     | Build Y-axis rotation matrix          |
| 4      | `IDENT`  | dst            | Load identity matrix                  |
| 5      | `TRANS`  | dst, x, y, z   | Build translation matrix              |
| 6      | `MVP`    | dst            | Write matrix to hardware MVP register |
| 7      | `EXIT`   | –              | Terminate shader                      |
| 8      | `CMP`    | mode, s0, s1   | Compare → store result in `cFlag`     |
| 9      | `ADD`    | dst, s0, s1    | Add (scalar or matrix)                |
| 10     | `SUB`    | dst, s0, s1    | Subtract                              |
| 11     | `DIV`    | dst, s0, s1    | Divide (scalar only)                  |
| 12     | `MOD`    | dst, s0, s1    | Modulo (i32 only)                     |
| 13     | `COL`    | r, g, b        | Write color registers                 |
| 14     | `FSAN`   | dst, src       | Float sanitize (NaN/inf→0)            |
| 15     | `BLEND`  | dst, c0, c1, t | Blend (scalar RGB)                    |
| 16     | `LERP`   | dst, c0, c1, t | Linear interpolate                    |
| 17     | `SQRT`   | dst, src       | Float sqrt                            |
| 18     | `ABS`    | dst, src       | Absolute value                        |
| 19     | `SIN`    | dst, src       | Sine                                  |
| 20     | `COS`    | dst, src       | Cosine                                |
| 21     | `CAST`   | dst, src       | Convert between I32 ↔ F32             |


-----


# **3. Instruction Set Reference (Refactored Format)**

## **MOV** (Move)

* **Opcode:** `0`
* **Syntax:** `MOV[ |m] dst, src`
* **Description:** Copies the value from `src` into `dst`. Supports matrix and scalar transfers.
* **Type:** I32, F32, Matrix
* **Type Field:** Present
* **Operands:**

  * `dst` — Destination register
  * `src` — Register, immediate scalar, or Data Segment matrix address

---

## **MUL** (Multiply)

* **Opcode:** `1`
* **Syntax:** `MUL[i|f|m] dst, src0, src1`
* **Description:** Performs multiplication (`src0 * src1`). Supports matrix or scalar arithmetic.
* **Type:** I32, F32, Matrix
* **Type Field:** Present
* **Operands:**

  * `dst` — Destination register
  * `src0` — Register or Data Segment address
  * `src1` — Register or Data Segment address

---

## **ROTX** (Rotation X)

* **Opcode:** `2`
* **Syntax:** `ROTX dst, angle`
* **Description:** Creates a 4×4 rotation matrix about the X-axis.
* **Type:** Matrix only
* **Type Field:** Not present
* **Operands:**

  * `dst` — Matrix register
  * `angle` — Immediate F32

---

## **ROTY** (Rotation Y)

* **Opcode:** `3`
* **Syntax:** `ROTY dst, angle`
* **Description:** Creates a 4×4 rotation matrix about the Y-axis.
* **Type:** Matrix only
* **Type Field:** Not present
* **Operands:**

  * `dst` — Matrix register
  * `angle` — Immediate F32

---

## **IDENT** (Identity Matrix)

* **Opcode:** `4`
* **Syntax:** `IDENT dst`
* **Description:** Writes a 4×4 identity matrix into `dst`.
* **Type:** Matrix only
* **Type Field:** Not present
* **Operands:**

  * `dst` — Matrix register

---

## **TRANS** (Translation Matrix)

* **Opcode:** `5`
* **Syntax:** `TRANS dst, x, y, z`
* **Description:** Creates a translation matrix from `(x, y, z)`.
* **Type:** Matrix only
* **Type Field:** Not present
* **Operands:**

  * `dst` — Matrix register
  * `x` — Immediate F32
  * `y` — Immediate F32
  * `z` — Immediate F32

---

## **MVP** (Commit Matrix)

* **Opcode:** `6`
* **Syntax:** `MVP dst`
* **Description:** Writes the matrix in `dst` to the hardware MVP (Model-View-Projection) unit.
* **Type:** Matrix only
* **Type Field:** Not present
* **Operands:**

  * `dst` — Matrix register

---

## **EXIT** (Terminate Shader)

* **Opcode:** `7`
* **Syntax:** `EXIT`
* **Description:** Stops shader execution.
* **Type:** N/A
* **Type Field:** Not present
* **Operands:** None

---

## **CMP** (Compare)

* **Opcode:** `8`
* **Syntax:** `CMP[i|f] mode, src0, src1`
* **Description:** Compares `src0` and `src1` using `mode` and writes result (0 or 1) into the runtime `cFlag`.
* **Type:** I32 or F32
* **Type Field:** Present
* **Operands:**

  * `mode` — Comparison mode (stored in instruction `cFlag` byte)
  * `src0` — Register or immediate
  * `src1` — Register or immediate

---

## **ADD** (Addition)

* **Opcode:** `9`
* **Syntax:** `ADD[i|f|m] dst, src0, src1`
* **Description:** Computes `dst = src0 + src1`. Supports scalar or matrix addition.
* **Type:** I32, F32, Matrix
* **Type Field:** Present
* **Operands:**

  * `dst` — Destination register
  * `src0` — Register or immediate
  * `src1` — Register or immediate

---

## **SUB** (Subtraction)

* **Opcode:** `10`
* **Syntax:** `SUB[i|f|m] dst, src0, src1`
* **Description:** Computes `dst = src0 − src1`.
* **Type:** I32, F32, Matrix
* **Type Field:** Present
* **Operands:**

  * `dst` — Destination register
  * `src0` — Register or immediate
  * `src1` — Register or immediate

---

## **DIV** (Division)

* **Opcode:** `11`
* **Syntax:** `DIV[i|f] dst, src0, src1`
* **Description:** Computes scalar division `dst = src0 / src1`.
* **Type:** I32 or F32
* **Type Field:** Present
* **Operands:**

  * `dst` — Scalar register
  * `src0` — Register or immediate
  * `src1` — Register or immediate

---

## **MOD** (Modulo)

* **Opcode:** `12`
* **Syntax:** `MOD dst, src0, src1`
* **Description:** Computes integer modulo: `dst = src0 % src1`.
* **Type:** I32 only
* **Type Field:** Not present
* **Operands:**

  * `dst` — Scalar register
  * `src0` — Register or immediate
  * `src1` — Register or immediate

---

## **COL** (Write Color Registers)

* **Opcode:** `13`
* **Syntax:** `COL r, g, b`
* **Description:** Writes scalar values into the pixel color registers: PR, PG, PB.
* **Type:** I32 only
* **Type Field:** Not present
* **Operands:**

  * `r` — Red component
  * `g` — Green component
  * `b` — Blue component

---

## **FSAN** (Float Sanitize)

* **Opcode:** `14`
* **Syntax:** `FSAN dst, src`
* **Description:** Replaces NaN or Inf in `src` with `0.0`.
* **Type:** F32
* **Type Field:** Not present
* **Operands:**

  * `dst` — Scalar register
  * `src` — Register or immediate

---

## **BLEND** (Blend)

* **Opcode:** `15`
* **Syntax:** `BLEND dst, src0, src1, t`
* **Description:** Performs color blending: `dst = src0*(1 - t) + src1*t`.
* **Type:** I32 (colors), F32 (t)
* **Type Field:** Not present
* **Operands:**

  * `dst` — Scalar register
  * `src0` — Color component
  * `src1` — Color component
  * `t` — Float blend factor

---

## **LERP** (Linear Interpolation)

* **Opcode:** `16`
* **Syntax:** `LERP dst, src0, src1, t`
* **Description:** Computes linear interpolation: `dst = src0 + (src1 − src0) * t`.
* **Type:** I32 (colors), F32 (t)
* **Type Field:** Not present
* **Operands:**

  * `dst` — Scalar register
  * `src0` — Register or immediate
  * `src1` — Register or immediate
  * `t` — Float factor

---

## **SQRT** (Square Root)

* **Opcode:** `17`
* **Syntax:** `SQRT dst, src`
* **Description:** Computes square root of a float.
* **Type:** F32
* **Type Field:** Not present
* **Operands:**

  * `dst` — Scalar register
  * `src` — Register or immediate

---

## **ABS** (Absolute Value)

* **Opcode:** `18`
* **Syntax:** `ABS[i|f] dst, src`
* **Description:** Computes absolute value of integer or float.
* **Type:** I32 or F32
* **Type Field:** Present
* **Operands:**

  * `dst` — Scalar register
  * `src` — Register or immediate

---

## **SIN** (Sine)

* **Opcode:** `19`
* **Syntax:** `SIN dst, src`
* **Description:** Computes sine of a float value.
* **Type:** F32
* **Type Field:** Not present
* **Operands:**

  * `dst` — Scalar register
  * `src` — Register or immediate

---

## **COS** (Cosine)

* **Opcode:** `20`
* **Syntax:** `COS dst, src`
* **Description:** Computes cosine of a float value.
* **Type:** F32
* **Type Field:** Not present
* **Operands:**

  * `dst` — Scalar register
  * `src` — Register or immediate

---

## **CAST** (Scalar Type Conversion)

* **Opcode:** `21`
* **Syntax:** `CAST[i|f] dst, src`
* **Description:** Converts scalar between integer and float types.

```
CASTf (i32 → f32)  
CASTi (f32 → i32)
```

* **Type:** I32 or F32 depending on direction
* **Type Field:** Present
* **Operands:**

  * `dst` — Scalar register
  * `src` — Scalar register

---
