# GPU Shader Compiler Specification v1.0

## Quick Start

1. Write shader assembly (`.asm` file)
2. Run compiler: `./compiler input.asm`
3. Get compiled output: `output.txt` (C code with binary shader data)
4. Include and upload with `GpuTransferBuffer()`

---

## Assembly Syntax

### Basic Format
```
[!]INSTRUCTION [arg0] [arg1] [arg2]
```

### Registers

**Matrix:** `m0–m7`, `mv` (M_IN)  
**Scalar:** `p0–p7`, `px`, `py`, `pr`, `pg`, `pb`

### Operands

- **Registers:** `m0`, `p0`, `px`, etc.
- **Immediates:** `123`, `3.14`, `-42`
- **Labels:** `loop:` (definition), `jmp loop` (reference)

### Comments
```asm
; Line comment
mov p0 100   ; Inline comment
```

---

## Type Suffixes

| Suffix | Type   | Example      |
|--------|--------|--------------|
| `i`    | Int    | `addi`, `casti` |
| `f`    | Float  | `addf`, `mulf` |
| `m`    | Matrix | `movm`, `mulm` |
| `v3`   | Vec3   | `addv3`, `mulv3` |

---

## Common Instructions

### Matrices
```asm
ident m0            ; Identity matrix
rotx m0 1.57        ; X rotation
roty m0 0.785       ; Y rotation
trans m0 1 2 3      ; Translation
mulm m0 m1 m2       ; Matrix multiply
```

### Arithmetic
```asm
addi/addf p0 p1 p2  ; Add
subi/subf p0 p1 p2  ; Subtract
muli/mulf p0 p1 p2  ; Multiply
divi/divf p0 p1 p2  ; Divide
```

### Vector Operations (Vec3)
```asm
vec3 m0 p0 p1 p2    ; Construct Vec3
dotv3 p0 m0 m1      ; Dot product → scalar
crossv3 m0 m1 m2    ; Cross product → Vec3
lenv3 p0 m0         ; Length of Vec3
normv3 m0 m1        ; Normalize
```

### Math & Utility
```asm
sin/cos/tan p0 x    ; Trig
sqrt p0 x           ; Square root
abs[i|f] p0 x       ; Absolute value
neg[i|f] p0 x       ; Negate
cmpi eq p0 100      ; Compare, set cFlag
!addi p0 p0 1       ; Conditional (if cFlag)
lduf p0 0           ; Load uniform
mvp m0              ; Output vertex
col m0              ; Extract RGB → PR, PG, PB
exit                ; End shader
```

---

## Conditional Execution

Prefix instruction with `!` to execute only if `cFlag == 1`:

```asm
cmpi lt p0 10.0     ; if (p0 < 10)
!addf p0 p0 1.0     ;   p0++
exit
```

**Conditions:** `eq`, `neq`, `lt`, `gt`, `lte`, `gte`

---

## Compilation

```bash
./compiler input.asm    # Creates output.txt
```

**output.txt contains:**
```c
uint64_t bin_shader[] = {
    0x4030090B, 0x3E030E3E,
    // ... more instructions ...
};
```

**Usage:**
```c
VRAMADDR shader_addr;
mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode,
                          bin_shader, sizeof(bin_shader), &shader_addr);
mGOP3D->GpuBindVertShader(mGOP3D, shader_addr, sizeof(bin_shader));
```

---

## Complete Example: Vertex Shader

```asm
; Load MVP matrix from uniform offset 0
ldum m0 0

; Transform vertex: m1 = m0 (MVP) * m_in (vertex)
mulv m1 m0 mv

; Output
mvp m1
exit
```

---

## Complete Example: Fragment Shader

```asm
; Normalize pixel coordinates
divf p0 px 640.0
divf p1 py 480.0

; Create pattern
mulf p0 p0 2.0
subf p0 p0 1.0
sin p2 p0
cos p3 p1
addf p4 p2 p3

; Convert to color
mulf p4 p4 127.5
addf p4 p4 127.5
mini p4 p4 255

; Output
mov pr p4
mov pg p4
mov pb p4
exit
```

---

## Error Handling

| Error | Solution |
|-------|----------|
| `Invalid register` | Check m0–m7, p0–p7, px, py, pr, pg, pb |
| `Expected argument` | Add missing operand |
| `Undefined label` | Define with `:` or check spelling |
| `Unknown instruction` | Check spelling and type suffix |

---

## Label Resolution (Two-Pass)

**Pass 1:** Tokenize, scan labels, record addresses  
**Pass 2:** Generate code, resolve label references

Forward references supported:
```asm
jmp end      ; Forward reference
mov p0 1
end:
exit         ; Label definition
```

---

## Tips

- Use **M registers** for matrices/vectors
- Use **P registers** for scalars
- Use **FMA** for `a*b + c` instead of separate ops
- Pre-compute constants in uniforms, not in shader
- Minimize sin/cos/atan calls

---

## Instruction Quick Reference

See [Shader ISA Specification](./shaderSpec.md) for complete list of 47 instructions.

**Most Common:**
- `mov` — Move/copy
- `add`, `sub`, `mul`, `div` — Arithmetic
- `sin`, `cos`, `tan` — Trig
- `ldu` — Load uniform
- `mvp` — Output vertex
- `col` — Output color
- `exit` — End shader
- `jmp` — Jump
- `cmpi`/`cmpf` — Compare

---

