# GPU Architecture Specification v1.2

## Overview

This document describes the architecture of a custom emulated graphics card designed for QEMU/UEFI environments. The GPU supports:

- **3D Rasterization** (lines, triangles)
- **Programmable Shaders** (vertex and fragment stages)
- **Software-Executed ISA** (custom instruction set)
- **Dynamic Memory Management** (buddy allocator)
- **Ring Buffer Command Queue**

---

## 1. Hardware Architecture

### 1.1 PCI Device Configuration

| Property        | Value              |
| --------------- | ------------------ |
| **Vendor ID**   | `0x6969`           |
| **Device ID**   | `0x2137`           |
| **Device Type** | `TYPE_PCI_GPU_DEVICE` |
| **Class Code**  | `PCI_CLASS_DISPLAY_OTHER` |
| **BAR0**        | MMIO (4 KB)        |
| **BAR1**        | VRAM (32 MB)       |

---

## 2. MMIO Interface (BAR0)

### 2.1 Register Map

| Offset | Register        | Field               | Size | R/W | Purpose                              |
|--------|-----------------|---------------------|------|-----|--------------------------------------|
| 0x00   | GPU_MODE        | `gpu_mode`          | 4B   | RW  | Operating mode (GOP/3D/IDLE)        |
| 0x04   | RING_HEAD       | `ring_buffer_head`  | 4B   | W   | CPU write pointer for command queue  |
| 0x08   | RING_TAIL       | `ring_buffer_tail`  | 4B   | R   | GPU read pointer (command progress) |
| 0x0C   | RING_START      | `ring_buffer_start` | 4B   | RW  | Ring buffer base address in VRAM    |
| 0x10   | RING_END        | `ring_buffer_end`   | 4B   | RW  | Ring buffer end address in VRAM     |
| 0x14   | VS_PTR          | `vs_code_addr`      | 4B   | RW  | Vertex shader code offset in VRAM   |
| 0x18   | FS_PTR          | `fs_code_addr`      | 4B   | RW  | Fragment shader code offset in VRAM |
| 0x1C   | FB_WIDTH        | `width`             | 4B   | RW  | Framebuffer width in pixels         |
| 0x20   | FB_HEIGHT       | `height`            | 4B   | RW  | Framebuffer height in pixels        |
| 0x24   | FB_ADDR         | `framebuffer_vram_offset` | 4B | RW | Framebuffer memory offset in VRAM   |
| 0x28   | GPU_TIME        | `gpu_time`          | 4B   | R   | GPU elapsed time counter (ms)       |
| 0x2C   | ZBUFFER_ADDR    | `zbuffer_addr`      | 4B   | RW  | Z-buffer memory offset in VRAM      |
| 0x30   | INT_STATUS      | `int_status`        | 4B   | RW1C| Pending interrupts (Write 1 to Clear)|
| 0x34   | INT_MASK        | `int_mask`          | 4B   | RW  | Interrupt enable mask               |

### 2.2 GPU Modes

```c
typedef enum {
    GPU_MODE_GOP,   // Graphics Output Protocol (BIOS framebuffer)
    GPU_MODE_3D,    // 3D rendering pipeline
    GPU_MODE_IDLE   // Idle 
} GpuMode;
```

**Mode Transitions:**

- Write `GPU_MODE_GOP` to disable 3D rendering and return to BIOS framebuffer
- Write `GPU_MODE_3D` to enable 3D rasterization
- GPU auto-sets `GPU_MODE_IDLE` when commands are executed

### 2.3 MMIO Access Semantics

- **Writes to RING_HEAD** trigger command processor immediately
- **Writes to INT_STATUS** clear the corresponding bits (Acknowledge)
- **All writes are word-aligned** (4-byte boundaries)
- **Misaligned accesses** are rejected with error

---

## 3. VRAM Architecture (BAR1)

### 3.1 Memory Layout

VRAM is a flat 32 MB address space managed by a **buddy allocator**. All buffers and structures are allocated dynamically at runtime; there are no fixed segment boundaries.

```
┌─────────────────────────────────────────┐
│                                         │
│  VRAM (32 MB) - Buddy Allocator Pool    │
│  ┌───────────────────────────────────┐  │
│  │ Framebuffer                       │  │ (allocated on init)
│  ├───────────────────────────────────┤  │
│  │ Ring Buffer                       │  │ (allocated on init)
│  ├───────────────────────────────────┤  │
│  │ Vertex Buffer (VBO)               │  │ (allocated by driver)
│  ├───────────────────────────────────┤  │
│  │ Index/Edge Buffer                 │  │ (allocated by driver)
│  ├───────────────────────────────────┤  │
│  │ Uniform Buffer (UBO)              │  │ (allocated by driver)
│  ├───────────────────────────────────┤  │
│  │ Vertex Shader Code                │  │ (allocated by driver)
│  ├───────────────────────────────────┤  │
│  │ Fragment Shader Code              │  │ (allocated by driver)
│  ├───────────────────────────────────┤  │
│  │ Z-Buffer                          │  │ (allocated on init)
│  ├───────────────────────────────────┤  │
│  │ Free Memory                       │  │ (managed by allocator)
│  │ (May be fragmented)               │  │
│  └───────────────────────────────────┘  │
│                                         │
└─────────────────────────────────────────┘
```
- **No fixed offsets:** Allocations are dynamic and depend on driver requests
- **Driver-managed:** The UEFI driver controls all buffer lifetimes via API calls
- **Buddy allocator:** Manages fragmentation and coalescing automatically
- **Page-based:** Minimum allocation unit is 64 bytes (one page)

### 3.2 Buddy Allocator

The GPU uses a **buddy allocator** for memory management:

**Properties:**

| Property       | Value           |
|----------------|-----------------|
| **Page Size**  | 64 bytes        |
| **Strategy**   | Power-of-2 blocks |
| **Max Allocs** | 512 (configurable) |
| **Free on exit** | Auto-coalesce   |

**Allocation Process:**

1. Driver calls `GpuAllocateMem(size)` or `GpuUpdateBuffer(data, size)`
2. Allocator finds smallest 2^N pages that fit the request
3. Returns VRAM offset (byte address)
4. Driver stores offset in configuration registers or buffer pointers

**Deallocation Process:**

1. Driver calls `GpuFreeMem(addr)` to release a buffer
2. Allocator marks pages as free
3. Auto-coalesces adjacent free blocks into larger buddies
4. Pages become available for new allocations


### 3.3 Dynamic Buffer Configuration

All buffers use the unified configuration structure:

```c
typedef struct {
    uint32_t addr;             // VRAM offset (bytes)
    uint32_t size;             // Total size (bytes)
    DataType element_type;     // Element type
} GenericBufferConfig;
```

**Supported Element Types:**

| Type          | Size (bytes) | Usage                |
|---------------|--------------|----------------------|
| `D_TYPE_FLOAT` | 4           | Single floats        |
| `D_TYPE_VEC2`  | 8           | 2D vectors           |
| `D_TYPE_VEC3`  | 12          | 3D vectors (color, position) |
| `D_TYPE_VEC4`  | 16          | 4D vectors (homogeneous) |
| `D_TYPE_MAT4`  | 64          | 4×4 matrices         |
| `D_TYPE_UINT32` | 4          | Indices              |

### 3.4 Buffer Allocation Workflow

**Vertex Buffer (VBO) Allocation:**

1. Driver prepares vertex data in system RAM
2. Driver calls `GpuTransferBuffer(Gop3dBufferTypeVertex, data, size, &addr)`
3. Allocator reserves space via buddy allocator
4. Driver data is copied to VRAM
5. GPU stores `addr` and `size` in `GenericBufferConfig`
6. Driver binds via `CMD_SET_STATE` with `STATE_ID_VBO_CONFIG`

**Example:**

```c
Vec3 vertices[] = { {0,0,0}, {1,0,0}, {0,1,0} };
VRAMADDR vbo_addr;
GpuTransferBuffer(Gop3dBufferTypeVertex, vertices, sizeof(vertices), &vbo_addr);
// vbo_addr = 0x175400 (allocated from free pool)

GenericBufferConfig vbo_config = {
    .addr = vbo_addr,
    .size = sizeof(vertices),
    .element_type = D_TYPE_VEC3
};
// Bind VBO via command: CMD_SET_STATE(STATE_ID_VBO_CONFIG, vbo_config)
```

**Index Buffer (Edge) Allocation:**

Similar workflow for indices:

```c
uint32_t indices[] = { 0, 1, 2 };
VRAMADDR edge_addr;
GpuTransferBuffer(Gop3dBufferTypeIndex, indices, sizeof(indices), &edge_addr);

GenericBufferConfig edge_config = {
    .addr = edge_addr,
    .size = sizeof(indices),
    .element_type = D_TYPE_UINT32
};
```

**Uniform Buffer (UBO) Allocation:**

```c
struct Uniforms {
    Mat4 mvp;
    Vec4 camera_pos;
} uniforms = { /* initialized data */ };

VRAMADDR ubo_addr;
GpuTransferBuffer(Gop3dBufferTypeUniform, &uniforms, sizeof(uniforms), &ubo_addr);

GenericBufferConfig ubo_config = {
    .addr = ubo_addr,
    .size = sizeof(uniforms),
    .element_type = D_TYPE_MAT4  // Mixed, but element type hints at primary content
};
```

**Shader Code Allocation:**

```c
uint64_t shader_code[] = { /* compiled ISA instructions */ };
VRAMADDR shader_addr;
GpuTransferBuffer(Gop3dBufferTypeShaderCode, shader_code, sizeof(shader_code), &shader_addr);

// Bind via: CMD_SET_STATE(STATE_ID_VERTEX_SHADER_PTR, {.shader_ptrs.vs_addr = shader_addr})
```

### 3.5 Reclamation & Buffer Updates

**Deallocating Old Buffers:**

When a buffer is no longer needed:

```c
// Free old VBO
GpuFreeBuffer(&old_vbo_addr);

// Allocate and bind new VBO
VRAMADDR new_vbo_addr;
GpuUpdateBuffer(Gop3dBufferTypeVertex, new_vertices, new_size, &new_vbo_addr);

// Reconfigure: CMD_SET_STATE(STATE_ID_VBO_CONFIG, {new_vbo_addr, new_size, ...})
```

**Update Semantics:**

- `GpuTransferBuffer`: Initial allocation + data copy
- `GpuUpdateBuffer`: Free old buffer (if applicable) + allocate new + copy data
- `GpuFreeBuffer`: Release allocated space back to pool

### 3.6 Memory Debugging

**Debug Functions:**

```c
// Print allocator statistics
GpuDebugPrintAllocatorStats();

// Detailed memory map
GpuDebugDumpMemoryMap();
```

---

## 4. Ring Buffer & Command Queue

### 4.1 Command Queue Operation

The **Ring Buffer** is a circular queue in VRAM where the CPU writes GPU commands.

**Flow:**

1. CPU writes commands to staging buffer in system RAM
2. CPU updates `RING_HEAD` register (triggers GPU)
3. GPU reads commands from `RING_START` to `RING_END`
4. GPU executes and updates `RING_TAIL` (read pointer)
5. When `HEAD == TAIL`, all commands are processed

**Ring Buffer Safety:**

- Automatically wraps at `RING_END` boundary
- Prevents command processor from running off allocated range
- Driver must ensure `RING_START < RING_END`

### 4.2 Command Structure

```c
typedef struct __attribute__((packed)) {
    CommandOpcode opcode;
    union {
        DrawPrimitivePayload draw;
        SetStatePayload state;
        ClearFramebufferPayload clear;
        uint32_t raw_data[8];
    } payload;
} Command;  // Total size: 32 bytes
```

### 4.3 Command Reference

#### CMD_NOOP (0x00)

No operation. Used for no puropse.

**Payload:** *(none)*

---

#### CMD_DRAW_PRIMITIVE (0x01)

Issues a draw call using the current pipeline state.

**Payload:**

```c
typedef struct {
    PrimitiveType type;  // POINTS, LINES, TRIANGLES
} DrawPrimitivePayload;
```

**Behavior:**

1. Reads vertex data from **VBO** at configured address
2. Uses **edge buffer** (indices) to assemble primitives
3. Executes **vertex shader** for each vertex
4. Rasterizes primitives (lines or triangles)
5. Executes **fragment shader** for each pixel

---

#### CMD_SET_STATE (0x02)

Updates GPU pipeline state.

**Payload:**

```c
typedef struct {
    StateID state_id;
    union {
        GenericBufferConfig buffer_config;
        struct {
            uint32_t vs_addr;
            uint32_t fs_addr;
        } shader_ptrs;
    } value;
} SetStatePayload;
```

**State IDs:**

| StateID                       | Purpose                        |
|-------------------------------|--------------------------------|
| `STATE_ID_VBO_CONFIG`         | Bind vertex buffer             |
| `STATE_ID_EDGE_CONFIG`        | Bind index/edge buffer         |
| `STATE_ID_UNIFORM_CONFIG`     | Bind uniform buffer            |
| `STATE_ID_VERTEX_SHADER_PTR`  | Load vertex shader             |
| `STATE_ID_FRAGMENT_SHADER_PTR` | Load fragment shader           |

---

#### CMD_CLEAR_FRAMEBUFFER (0x03)

Clears framebuffer and Z-buffer.

**Payload:**

```c
typedef struct {
    uint8_t options;  // Reserved for future (clear masks, color modes)
} ClearFramebufferPayload;
```

**Current Behavior:**

- Fills framebuffer with black (0xFF000000)
- Sets all Z-buffer entries to FLT_MAX (no depth)

---

## 5. Shader System

### 5.1 Shader Storage

Shaders are stored in VRAM as **compiled instruction sequences** (ISA code). There are no shader headers or metadata structures—just the raw bytecode.

**Storage Format:**

```c
// Shader is simply an array of Instr structs
typedef struct {
    uint8_t  opcode;
    uint8_t  cFlag;
    uint8_t  dest;
    uint8_t  arg0Type:2;
    uint8_t  arg1Type:2;
    uint8_t  arg2Type:2;
    uint8_t  opType:2;
    InstrArg arg0;
    InstrArg arg1;
    InstrArg arg2;
} Instr;  // 16 bytes each

```

**Shader Address:**

- Stored in MMIO register `VS_PTR` (vertex shader) or `FS_PTR` (fragment shader)
- Represents a **VRAM byte offset** pointing to the start of the instruction sequence
- Set via `CMD_SET_STATE` with `STATE_ID_VERTEX_SHADER_PTR` or `STATE_ID_FRAGMENT_SHADER_PTR`

### 5.2 Shader Execution

- **Linear execution:** Instructions are fetched sequentially from VRAM
- **No metadata:** Shader size is unknown; execution terminates on `EXIT` instruction
- **Conditional execution:** Instructions can be gated by the `cFlag` register
- **Register state:** Persisted across instructions (M registers, P registers, special registers)

### 5.3 Vertex Shader

**Purpose:** Transform vertex attributes and produce output position

**Input:**

- Vertex position in `M_IN` (special input register)
- Vertex attributes (colors, normals, etc.) passed via `M_IN` or `M0–M7`
- Uniforms loaded from uniform buffer via `LDU` instruction

**Output:**

- Transformed position written to `v_out` (GPU state variable)
- Used for rasterization and perspective division

**Typical Flow:**

```
1. Load uniforms (MVP matrix, etc.) with LDU
2. Read vertex from M_IN
3. Apply transformations (MUL, ADD, SUB, etc.)
4. Write result to v_out via MVP instruction
5. EXIT
```

**Example:**

```
; Assume M0 = MVP matrix, M_IN = vertex position
LDUm M0 0          ; Load MVP matrix from uniform offset 0
MULV4 M1 M0 M_IN   ; M1 = MVP * vertex
MVP M1             ; Write to output
EXIT
```

---

### 5.4 Fragment Shader

**Purpose:** Compute final pixel color

**Input:**

- Pixel coordinates in `PX`, `PY` registers (set by rasterizer)
- Interpolated vertex attributes (colors, normals, etc.)
- Uniforms loaded from uniform buffer via `LDU` instruction

**Output:**

- Final color in `PR`, `PG`, `PB` registers (8-bit color components)
- Written to framebuffer by rasterizer

**Typical Flow:**

```
1. PX, PY already set by rasterizer
2. Load uniforms with LDU
3. Perform lighting/shading calculations
4. Write color to PR, PG, PB
5. EXIT
```

**Example:**

```
; Simple flat color shader
MOV p0 255         ; Red component
MOV p1 0           ; Green component
MOV p2 0           ; Blue component
MOV PR p0          ; Write to PR
MOV PG p1          ; Write to PG
MOV PB p2          ; Write to PB
EXIT
```

---

### 5.5 Shader Invocation

**Vertex Shader Invocation:**

For each vertex in the VBO:

```c
gpu->v_pos.right = vertex_data;  // Set M_IN implicitly
exec_shader(gpu, gpu->vs_code_addr);
Vec4 transformed = gpu->v_out.right;  // Read output
```

**Fragment Shader Invocation:**

For each pixel during rasterization:

```c
gpu->pRegs[REG_PX].f32 = (float)x;
gpu->pRegs[REG_PY].f32 = (float)y;
// ... set interpolated vertex data ...
exec_shader(gpu, gpu->fs_code_addr);
uint32_t color = (gpu->pRegs[REG_PR].u32 << 16) |
                 (gpu->pRegs[REG_PG].u32 << 8)  |
                  gpu->pRegs[REG_PB].u32;
```

---

## 6. Rasterization Pipeline

### 6.1 Vertex Processing

For each vertex in the VBO:

1. **Read** Set register `M_IN` to current vertex attriubute
2. **Execute** vertex shader
3. **Transform** position from clip space to screen space
4. **Perspective divide** (convert homogeneous coordinates)

### 6.2 Primitive Assembly

Indices from edge buffer are grouped into primitives:

- **LINES:** Pairs of vertices
- **TRIANGLES:** Triplets of vertices

### 6.3 Rasterization

**Line Drawing:** Bresenham's algorithm with per-pixel interpolation

**Triangle Rasterization:** Barycentric coordinates with depth testing

**Depth Test:** Compare Z value with Z-buffer; write only if closer

### 6.4 Fragment Processing

For each pixel:

1. **Set** PX, PY to pixel coordinates
2. **Interpolate** vertex attributes (barycentric)
3. **Execute** fragment shader
4. **Output** final color (PR, PG, PB)
5. **Write** to framebuffer if depth test passes

---

## 7. Memory Management

### 7.1 Buddy Allocator

VRAM uses a **buddy allocator** for dynamic memory:

- **Page size:** 64 bytes
- **Strategy:** Power-of-2 allocation with coalescing
- **Tracking:** Page status array + allocation sizes array

**Allocation:**

```c
VRAMADDR GpuAllocateMem(UINT32 bytesToAlloc, CHAR8 *Tag);
```

**Deallocation:**

```c
BOOLEAN GpuFreeMem(VRAMADDR addr);
```

**Debug Mode** (optional):

- Stores allocation tags for debugging
- Prints memory fragmentation stats
- Disabled in release builds

---

## 8. Instruction Set

See [Shader ISA Specification](./shaderSpec.md)

---

## 9. Framebuffer Format

### 9.1 Color Format

```c
typedef uint32_t Color;  // 0xAARRGGBB (ARGB8888)
```

**Bit Layout:**

| Bits 31-24 | Bits 23-16 | Bits 15-8 | Bits 7-0 |
|------------|------------|-----------|----------|
| Alpha      | Red        | Green     | Blue     |

### 9.2 Framebuffer Size

**Resolution:** 640 × 480 pixels

**Size:** 640 × 480 × 4 bytes = **1,228,800 bytes** (~1.2 MB)

**Offset in VRAM:** Configurable via `FB_ADDR` register

---

## 10. Z-Buffer

### 10.1 Depth Format

```c
typedef float ZValue;  // 32-bit IEEE 754 float
```

**Range:** [0.0, 1.0] (normalized depth)

**Initial Value:** FLT_MAX (infinitely far)

### 10.2 Depth Test

```c
if (z_fragment < z_buffer[pixel]) {
    z_buffer[pixel] = z_fragment;
    write_pixel(pixel, color);
}
```

**Always-on** (no configuration)

---

## 11. Synchronization & Timing

### 11.1 Command Completion

Driver polls `RING_TAIL` to detect command completion:

```c
while (gpu->ring_buffer_head != gpu->ring_buffer_tail) {
    // Wait for GPU to process commands
}
```
---

## 12. GOP 3D Protocol API

### 12.1 Protocol Overview

The **GOP 3D Protocol** (`gGop3dProtocolGuid`) is the primary software interface for UEFI applications to interact with the GPU. It provides a set of function pointers that abstract away direct MMIO register access and command buffer management.

**Protocol Location:** `Protocol/Gop3D.h`

**Protocol GUID:** `{ 0x12345678, 0x1234, 0x5678, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0 } }`

### 12.2 Core Data Types

```c
typedef UINT32 VRAMADDR;  // GPU VRAM memory offset (32-bit address)

typedef enum {
  Gop3dBufferTypeVertex,    // Vertex position/attribute data
  Gop3dBufferTypeIndex,     // Triangle or line indices
  Gop3dBufferTypeUniform,   // Shader uniform constants
  Gop3dBufferTypeShaderCode // Compiled shader ISA
} GOP_3D_BUFFER_TYPE;

typedef enum {
  Gop3dTopologyPoints,      // Point rendering (future)
  Gop3dTopologyLines,       // Line/wireframe rendering
  Gop3dTopologyTriangles    // Triangle rasterization
} GOP_3D_TOPOLOGY;
```

### 12.3 Protocol Functions

#### **GpuInit**

```c
EFI_STATUS GpuInit(IN GOP_3D_PROTOCOL *This);
```

**Purpose:** Initialize the GPU driver, allocate memory structures, and set up internal state.

**Parameters:**
- `This` — Pointer to the GOP_3D_PROTOCOL instance

**Returns:** `EFI_SUCCESS` on success, error code otherwise

**Example:**
```c
EFI_STATUS Status = mGOP3D->GpuInit(mGOP3D);
if (EFI_ERROR(Status)) {
    Print(L"GPU initialization failed: %r\n", Status);
    return Status;
}
```

---

#### **GpuDestroy**

```c
EFI_STATUS GpuDestroy(IN GOP_3D_PROTOCOL *This);
```

**Purpose:** Clean up GPU resources and shut down the driver.

**Parameters:**
- `This` — Pointer to the GOP_3D_PROTOCOL instance

**Returns:** `EFI_SUCCESS` on success

**Example:**
```c
mGOP3D->GpuDestroy(mGOP3D);
```

---

#### **GpuSetMode**

```c
EFI_STATUS GpuSetMode(IN GOP_3D_PROTOCOL *This, IN UINT32 Mode);
```

**Purpose:** Switch GPU operating mode between GOP (framebuffer) and 3D rendering.

**Parameters:**
- `This` — Pointer to the GOP_3D_PROTOCOL instance
- `Mode` — GPU mode (0 = GOP, 1 = 3D)

**Returns:** `EFI_SUCCESS` on success

**Example:**
```c
// Enable 3D rendering
mGOP3D->GpuSetMode(mGOP3D, 1);

// ... render commands ...

// Switch back to GOP (BIOS framebuffer)
mGOP3D->GpuSetMode(mGOP3D, 0);
```

---

#### **GpuTransferBuffer**

```c
EFI_STATUS GpuTransferBuffer(
    IN  GOP_3D_PROTOCOL    *This,
    IN  GOP_3D_BUFFER_TYPE Type,
    IN  VOID               *HostData,
    IN  UINT32             Size,
    OUT VRAMADDR           *GpuAddress
);
```

**Purpose:** Allocate VRAM space and copy data from system RAM to GPU memory.

**Parameters:**
- `This` — Pointer to the GOP_3D_PROTOCOL instance
- `Type` — Buffer type (vertex, index, uniform, shader code)
- `HostData` — Pointer to source data in system RAM
- `Size` — Size in bytes to allocate and copy
- `GpuAddress` — [OUTPUT] VRAM offset where data was uploaded

**Returns:** `EFI_SUCCESS` on success, `EFI_OUT_OF_RESOURCES` if VRAM allocation fails

**Example:**
```c
Vec3 vertices[] = { {0,0,0}, {1,0,0}, {0,1,0} };
VRAMADDR vbo_addr;

EFI_STATUS Status = mGOP3D->GpuTransferBuffer(
    mGOP3D,
    Gop3dBufferTypeVertex,
    vertices,
    sizeof(vertices),
    &vbo_addr
);

if (EFI_ERROR(Status)) {
    Print(L"Failed to transfer vertex buffer: %r\n", Status);
    return Status;
}
Print(L"Vertex buffer uploaded to VRAM offset: 0x%x\n", vbo_addr);
```

---

#### **GpuUpdateBuffer**

```c
EFI_STATUS GpuUpdateBuffer(
    IN  GOP_3D_PROTOCOL    *This,
    IN  GOP_3D_BUFFER_TYPE Type,
    IN  VOID               *HostData,
    IN  UINT32             Size,
    OUT VRAMADDR           *GpuAddress
);
```

**Purpose:** Reallocate and update a buffer (frees old buffer if applicable, allocates new space).

**Parameters:** Same as `GpuTransferBuffer`

**Returns:** Same as `GpuTransferBuffer`

**Difference:** Use this for dynamic data that changes per-frame (e.g., MVP matrices). The old allocation is freed automatically.

**Example:**
```c
Mat4 mvp = /* computed transformation */;

// First call
if (hMVP == 0) {
    mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeUniform, &mvp, sizeof(mvp), &hMVP);
}
// Subsequent calls (per-frame update)
else {
    mGOP3D->GpuUpdateBuffer(mGOP3D, Gop3dBufferTypeUniform, &mvp, sizeof(mvp), &hMVP);
}
```

---

#### **GpuFreeBuffer**

```c
EFI_STATUS GpuFreeBuffer(
    IN  GOP_3D_PROTOCOL *This,
    IN  VRAMADDR        *GpuAddress
);
```

**Purpose:** Release VRAM allocated by `GpuTransferBuffer` or `GpuUpdateBuffer`.

**Parameters:**
- `This` — Pointer to the GOP_3D_PROTOCOL instance
- `GpuAddress` — VRAM offset to deallocate

**Returns:** `EFI_SUCCESS` on success

**Example:**
```c
mGOP3D->GpuFreeBuffer(mGOP3D, &vbo_addr);
mGOP3D->GpuFreeBuffer(mGOP3D, &shader_addr);
```

---

#### **GpuBindVertShader** / **GpuBindFragShader**

```c
EFI_STATUS GpuBindVertShader(
    IN GOP_3D_PROTOCOL *This,
    IN VRAMADDR        ShaderAddr,
    IN UINT32          ShaderSize
);

EFI_STATUS GpuBindFragShader(
    IN GOP_3D_PROTOCOL *This,
    IN VRAMADDR        ShaderAddr,
    IN UINT32          ShaderSize
);
```

**Purpose:** Bind shader code to the pipeline.

**Parameters:**
- `ShaderAddr` — VRAM offset of compiled shader code
- `ShaderSize` — Size of shader in bytes

**Example:**
```c
mGOP3D->GpuBindVertShader(mGOP3D, hVS, sizeof(bin_vertex_shader));
mGOP3D->GpuBindFragShader(mGOP3D, hFS, sizeof(bin_fragment_shader));
```

---

#### **GpuBindVBO** / **GpuBindIBO** / **GpuBindUBO**

```c
EFI_STATUS GpuBindVBO(
    IN GOP_3D_PROTOCOL *This,
    IN VRAMADDR        BufferAddr,
    IN UINT32          ElementCount
);

EFI_STATUS GpuBindIBO(
    IN GOP_3D_PROTOCOL *This,
    IN VRAMADDR        BufferAddr,
    IN UINT32          ElementCount
);

EFI_STATUS GpuBindUBO(
    IN GOP_3D_PROTOCOL *This,
    IN VRAMADDR        BufferAddr,
    IN UINT32          Size
);
```

**Purpose:** Bind vertex, index, and uniform buffers to the pipeline.

**Parameters:**
- `BufferAddr` — VRAM offset of buffer
- `ElementCount` / `Size` — Number of elements or total size

**Example:**
```c
mGOP3D->GpuBindVBO(mGOP3D, hVBO, 8);      // 8 vertices
mGOP3D->GpuBindIBO(mGOP3D, hIBO, 12);     // 12 indices
mGOP3D->GpuBindUBO(mGOP3D, hMVP, sizeof(Mat4));
```

---

#### **GpuCmdBegin** / **GpuCmdEnd**

```c
EFI_STATUS GpuCmdBegin(IN GOP_3D_PROTOCOL *This);
EFI_STATUS GpuCmdEnd(IN GOP_3D_PROTOCOL *This);
```

**Purpose:** Bracket command recording. All state changes and draw calls between `Begin` and `End` are batched.

**Example:**
```c
mGOP3D->GpuCmdBegin(mGOP3D);
// ... record commands (bind buffers, draw calls) ...
mGOP3D->GpuCmdEnd(mGOP3D);
mGOP3D->GpuSubmitCmd(mGOP3D);  // Submit to GPU
```

---

#### **GpuClearFrame**

```c
EFI_STATUS GpuClearFrame(
    IN GOP_3D_PROTOCOL *This,
    IN UINT32          Color
);
```

**Purpose:** Clear framebuffer to a solid color.

**Parameters:**
- `Color` — 32-bit color value (0xAARRGGBB)

**Example:**
```c
mGOP3D->GpuClearFrame(mGOP3D, 0xFF000000);  // Clear to black
```

---

#### **GpuDraw**

```c
EFI_STATUS GpuDraw(
    IN GOP_3D_PROTOCOL *This,
    IN GOP_3D_TOPOLOGY Topology,
    IN UINT32          VertexCount
);
```

**Purpose:** Issue a draw call with the currently bound buffers and shaders.

**Parameters:**
- `Topology` — Primitive type (Lines, Triangles)
- `VertexCount` — Number of vertices/indices to render

**Example:**
```c
// Draw wireframe
mGOP3D->GpuDraw(mGOP3D, Gop3dTopologyLines, 24);

// Draw filled triangles
mGOP3D->GpuDraw(mGOP3D, Gop3dTopologyTriangles, 36);
```

---

#### **GpuSubmitCmd**

```c
EFI_STATUS GpuSubmitCmd(IN GOP_3D_PROTOCOL *This);
```

**Purpose:** Submit the recorded command batch to the GPU for execution.

**Example:**
```c
mGOP3D->GpuCmdBegin(mGOP3D);
// ... record commands ...
mGOP3D->GpuCmdEnd(mGOP3D);
mGOP3D->GpuSubmitCmd(mGOP3D);  // Send to GPU
```

---

#### **GpuPresent**

```c
EFI_STATUS GpuPresent(IN GOP_3D_PROTOCOL *This);
```

**Purpose:** Present the rendered frame to the display.

**Example:**
```c
mGOP3D->GpuPresent(mGOP3D);
```

---

### 12.4 Typical Rendering Workflow

```c
// 1. Initialize
mGOP3D->GpuInit(mGOP3D);
mGOP3D->GpuSetMode(mGOP3D, 1);  // Enable 3D

// 2. Upload static assets (once)
VRAMADDR hVBO, hIBO, hVS, hFS;
mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeVertex, 
                          vertices, sizeof(vertices), &hVBO);
mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeIndex, 
                          indices, sizeof(indices), &hIBO);
mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode, 
                          shader_vs, sizeof(shader_vs), &hVS);
mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeShaderCode, 
                          shader_fs, sizeof(shader_fs), &hFS);

// 3. Render loop
while (!done) {
    // Update dynamic data
    Mat4 mvp = ComputeTransformation();
    VRAMADDR hMVP;
    if (first_frame) {
        mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeUniform, 
                                  &mvp, sizeof(mvp), &hMVP);
    } else {
        mGOP3D->GpuUpdateBuffer(mGOP3D, Gop3dBufferTypeUniform, 
                                &mvp, sizeof(mvp), &hMVP);
    }

    // Record frame
    mGOP3D->GpuCmdBegin(mGOP3D);
    mGOP3D->GpuClearFrame(mGOP3D, 0xFF000000);
    
    mGOP3D->GpuBindVertShader(mGOP3D, hVS, sizeof(shader_vs));
    mGOP3D->GpuBindFragShader(mGOP3D, hFS, sizeof(shader_fs));
    mGOP3D->GpuBindVBO(mGOP3D, hVBO, vertex_count);
    mGOP3D->GpuBindIBO(mGOP3D, hIBO, index_count);
    mGOP3D->GpuBindUBO(mGOP3D, hMVP, sizeof(mvp));
    
    mGOP3D->GpuDraw(mGOP3D, Gop3dTopologyTriangles, index_count);
    
    mGOP3D->GpuCmdEnd(mGOP3D);
    mGOP3D->GpuSubmitCmd(mGOP3D);
    mGOP3D->GpuPresent(mGOP3D);
}

// 4. Cleanup
mGOP3D->GpuFreeBuffer(mGOP3D, &hVBO);
mGOP3D->GpuFreeBuffer(mGOP3D, &hIBO);
mGOP3D->GpuFreeBuffer(mGOP3D, &hVS);
mGOP3D->GpuFreeBuffer(mGOP3D, &hFS);
mGOP3D->GpuFreeBuffer(mGOP3D, &hMVP);
mGOP3D->GpuSetMode(mGOP3D, 0);  // Return to GOP
mGOP3D->GpuDestroy(mGOP3D);
```

---

### 12.5 Error Handling

All protocol functions return `EFI_STATUS`. Common return values:

| Status | Meaning |
|--------|---------|
| `EFI_SUCCESS` | Operation completed successfully |
| `EFI_OUT_OF_RESOURCES` | VRAM allocation failed (out of memory) |
| `EFI_INVALID_PARAMETER` | Invalid argument (e.g., null pointer) |
| `EFI_DEVICE_ERROR` | GPU hardware error |
| `EFI_NOT_READY` | GPU not initialized |

**Best Practice:**
```c
EFI_STATUS Status = mGOP3D->GpuTransferBuffer(...);
if (EFI_ERROR(Status)) {
    Print(L"Buffer transfer failed: %r\n", Status);
    // Handle error (cleanup, return, etc.)
    return Status;
}
```

---

### 12.6 Memory Management via Protocol

The protocol automatically manages VRAM allocation through the buddy allocator. Applications need not worry about:
- Fragmentation
- Coalescing
- Memory tracking

However, applications **must**:
- Call `GpuFreeBuffer` for each buffer allocated via `GpuTransferBuffer` / `GpuUpdateBuffer`
- Avoid using freed addresses
- Respect maximum VRAM capacity (32 MB)

---
## 13. Interrupt Management (MSI)

### 13.1 Overview

The GPU uses **Message Signaled Interrupts (MSI)** to notify the CPU of specific hardware events. This reduces CPU overhead by eliminating the need for continuous polling of the `RING_TAIL` register.

### 13.2 Interrupt Registers

#### **INT_STATUS (0x30) [Read / Write-1-to-Clear]**
A 32-bit bitmask indicating which events have occurred.

| Bit | Name | Description |
|-----|------|-------------|
| 0   | `GPU_INT_CMD_DONE` | Set when the command processor becomes idle (Head == Tail). |
| 1-31| *Reserved* | Future use. |

#### **INT_MASK (0x34) [Read / Write]**
A 32-bit bitmask used to enable or disable the signaling of specific interrupts.

- If a bit is `1`, the GPU will send an MSI message when the corresponding event occurs.
- If a bit is `0`, the GPU will update `INT_STATUS` but will **not** trigger an interrupt signal.

### 13.3 Interrupt Workflow (The Handshake)

1. **Initialization:** The driver sets `INT_MASK` to enable desired interrupts (e.g., `0x01` for Command Done).
2. **Execution:** The CPU submits commands and continues other work.
3. **GPU Signal:** When the GPU finishes, it sets the `GPU_INT_CMD_DONE` bit in `INT_STATUS`.
4. **MSI Trigger:** If the mask bit is enabled, the GPU sends an MSI message to the CPU.
5. **Acknowledgment:** Upon receiving the interrupt, the driver reads `INT_STATUS` to identify the event and then writes that same value back to `INT_STATUS` to clear (Acknowledge) the interrupt.

### 13.4 Simple example of interrupt in practice

Example how does the interrupt work in GpuPresent:
```cpp
    1 EFI_STATUS GpuPresent(GOP_3D_PROTOCOL *This) {
    2     // 1. Send the batch to VRAM and "Kick" the GPU by writing to RING_HEAD
    3     GpuSubmitCmd(This);
    4
    5     // 2. WAIT for the Interrupt Status bit (Bit 0) to flip to 1
    6     // The CPU is now "Spinning" or "Waiting" efficiently
    7     while (!(GpuMmioRead32(REG_INT_STATUS_ADDR) & GPU_INT_CMD_DONE)) {
    8         gBS->Stall(10);
    9     }
   10
   11     // 3. ACK the interrupt (Tell the GPU we saw the report)
   12     GpuRingBufferAckInterrupt(GPU_INT_CMD_DONE);
   13
   14     return EFI_SUCCESS;
   15 }
```

Which roughly equates to:
```txt
* START:    STATUS=0, MASK=1
* GPU BUSY: STATUS=0, MASK=1
* GPU DONE: STATUS=1, MASK=1 \\MSI Sent!
* CPU ACK:  STATUS=0, MASK=1 \\Ready for the next frame
```