

# GPU Matrix Shader ISA Specification v1.0


### 1.1 Registers

The core contains **8 General Purpose Registers**, each capable of storing a standard 4*4  floating-point matrix.

| Register Name | Index | Description |
| :--- | :--- | :--- |
| **M0 - M7** | `0x0` - `0x7` | General Purpose Matrix Registers |

### 1.2 Memory Model

The Shader segment is divided into two distinct segments within VRAM:

  * **Code Segment:** Read-only executable instructions.
  * **Data Segment:** Read-only matrix data (constants, uniforms).

**Program Control:**
The program entry point is defined by writing the offset from the shader segment base address to the **BAR0** register at `SHADER_ADDR`.

-----

## 2\. Instruction Format

Instructions are fixed-width data structures. The opcode defines the operation, and operand types are determined via bit-flags.

### 2.1 C-Struct Representation

```c
typedef struct Instr {
    uint8_t   opcode;    // Operation identifier
    uint8_t   dst;       // Destination Register Index (M0-M7)
    uint16_t  opt;       // Options / Padding
    uint32_t  arg0;      // Source 0
    uint32_t  arg1;      // Source 1
    uint32_t  arg2;      // Source 2
} Instr;
```

### 2.2 Argument Encoding

Arguments (`arg0`, `arg1`, `arg2`) are 32-bit values interpreted based on the instruction context and specific bit-flags.

  * **Matrix Register:**
    If the Most Significant Bit (MSB) is set (1 << 31), the lower bits represent the register index.
      * *Mask:* `0x80000000 | reg_index`
  * **Memory Address (Data Segment):**
    If the MSB is **0** and the instruction expects a matrix, the value is treated as an offset (in bytes) from the start of the Data Segment.
  * **Immediate Float:**
    For instructions expecting scalar values (e.g., `ROTX`, `TRANS`), the 32-bit value is interpreted directly as an IEEE 754 floating-point number.

-----

## 3\. Instruction Set Reference

### Summary Table

| Opcode | Mnemonic | Operands | Description |
| :--- | :--- | :--- | :--- |
| **0** | `MOV` | `dst, src` | Move matrix data |
| **1** | `MUL` | `dst, src0, src1` | Matrix multiplication |
| **2** | `ROTX` | `dst, rads` | Create X-axis rotation matrix |
| **3** | `ROTY` | `dst, rads` | Create Y-axis rotation matrix |
| **4** | `IDENT` | `dst` | Load Identity matrix |
| **5** | `TRANS` | `dst, x, y, z` | Create Translation matrix |
| **6** | `MVP` | `dst` | Flush to hardware MVP register |
| **7** | `EXIT` | - | Terminate shader execution |

-----

### Detailed Operations

#### **MOV** (Move)

  * **Opcode:** `0`
  * **Syntax:** `MOV dst, src`
  * **Description:** Copies the contents of the source matrix into the destination register.
  * **Operands:**
      * `dst`: Register Index (M0-M7)
      * `src`: Register Index OR Data Segment Offset

#### **MUL** (Multiply)

  * **Opcode:** `1`
  * **Syntax:** `MUL dst, src0, src1`
  * **Description:** Performs matrix multiplication.
    Dst = Src0 * Src1
  * **Operands:**
      * `dst`: Register Index
      * `src0`: Register Index OR Data Segment Offset
      * `src1`: Register Index OR Data Segment Offset

#### **ROTX** (Rotate X)

  * **Opcode:** `2`
  * **Syntax:** `ROTX dst, angle`
  * **Description:** Generates a rotation matrix around the X-axis based on the input angle (in radians) and stores it in `dst`.
  * **Operands:**
      * `dst`: Register Index
      * `angle`: 32-bit Float (Immediate)

#### **ROTY** (Rotate Y)

  * **Opcode:** `3`
  * **Syntax:** `ROTY dst, angle`
  * **Description:** Generates a rotation matrix around the Y-axis based on the input angle (in radians) and stores it in `dst`.
  * **Operands:**
      * `dst`: Register Index
      * `angle`: 32-bit Float (Immediate)

#### **IDENT** (Identity)

  * **Opcode:** `4`
  * **Syntax:** `IDENT dst`
  * **Description:** Loads the 4x4 Identity matrix into the destination register.
  * **Operands:**
      * `dst`: Register Index

#### **TRANS** (Translate)

  * **Opcode:** `5`
  * **Syntax:** `TRANS dst, x, y, z`
  * **Description:** Generates a translation matrix using the immediate float coordinates provided.
  * **Operands:**
      * `dst`: Register Index
      * `x`: 32-bit Float (Immediate)
      * `y`: 32-bit Float (Immediate)
      * `z`: 32-bit Float (Immediate)

#### **MVP** (Set Output)

  * **Opcode:** `6`
  * **Syntax:** `MVP dst`
  * **Description:** Commits the matrix in `dst` to the special hardware "Model View Projection" state register, to be used by the rasterization pipeline.
  * **Operands:**
      * `dst`: Register Index

#### **EXIT** (Terminate)

  * **Opcode:** `7`
  * **Syntax:** `EXIT`
  * **Description:** Signals the end of the shader program. The GPU will cease fetching instructions.
