# GPU Architecture 1.1
Interacting with MMIO and VRAM BARs, overall objectives

### Main focus with this version
Since this is inital implementation of this emulated graphics card, we want to mainly focus on most basic operations in single application context. What it needs to cover:
- Basic switching between GOP and our 3D rendering using our EFI protocol.
- Rendering multiple objects with basic rasterizer using point, line, polygon_wireframe, polygon_fill (no textures)
- Basic shader execution including vertex shaders, fragments shaders
- Easy control and streamlined implementation


## MMIO BAR [BAR 0]
MMIO BAR is main way to control GPUs behaviour, constains information about pointers for other data structures and very imporant control bytes.

#### TODOS
- is even uint8 vaiable if we use 32bit addresing?


| Offset (Hex) | Register Name   | Field in `GpuState`            | Size | R/W | Role                                                         |
|--------------|-----------------|--------------------------------|------|-----|--------------------------------------------------------------|
| **0x00**     | `GPU_MODE`      | `gpu_mode`                     | 4B   | RW  | GPU Status ( GOP mode, 3D mode)                      |
| **0x04**     | `RING_HEAD`     | `ring_buffer_head`             | 4B   | RW  | Ring Buffer Head (CPU write pointer)                        |
| **0x08**     | `RING_TAIL`     | `ring_buffer_tail`             | 4B   | R   | Ring Buffer Tail (GPU read pointer)                         |
| **0x10**     | `VS_PTR`   | `vs_code_addr`                 | 4B   | RW  | VRAM Offset for Vertex Shader Code (Set via CMD)            |
| **0x14**     | `FS_PTR`   | `fs_code_addr`                 | 4B   | RW  | VRAM Offset for Fragment Shader Code (Set via CMD)          |
| **0x18**     | `WIDTH`         | `width`                        | 4B   | RW  | Frame Buffer Width in Pixels                                |
| **0x1C**     | `HEIGHT`        | `height`                       | 4B   | RW  | Frame Buffer Height in Pixels                               |
| **0x20**     | `FB_ADDR`       | `framebuffer_vram_offset`      | 4B   | RW  | VRAM Offset for the Frame Buffer (Back Buffer)              |
| **0x24**     | `GPU_TIME`      | `gpu_time`                     | 4B   | R   | GPU Time Counter (elapsed since boot/reset)                 |


## VRAM BAR [BAR 1]
VRAM BAR holds data needed for pipeline construction, framebuffer region and all data regarding vertecies and edges.

In VRAM instead of segmenting the memory and allocating data with dicated bounds, we create it dynamicly using data structures.

First we need to start with:
### Main Frame Buffer:
This is just a linear buffer of memory used as a main display frambuffer, its size is dependant on inital parameters set during driver start.

Starts at 0x0 adress of VRAM BAR


### Ring buffer:

The GPU begins executing commands when `RING_HEAD` register has changed.

**Driver writes commands → updates HEAD → GPU detects change → executes.**

#### GPU Command Table

| Command Name              | Opcode (Hex) | Payload Structure                                              | Description / Purpose                                                                                                  |
| ------------------------- | ------------ | -------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------- |
| **CMD_NOOP**              | `0x00`       | *(none)*                                                       | Does nothing. Used for padding or alignment.                                                                           |
| **CMD_DRAW_PRIMITIVE**    | `0x01`       | `DrawPrimitivePayload`<br>• `PrimitiveType type`               | Issues a draw call using current pipeline state. Renders points, lines, or triangles depending on `type`.              |
| **CMD_SET_STATE**         | `0x02`       | `SetStatePayload`<br>• `StateID state_id`<br>• `value` (union) | Updates internal GPU pipeline state such as VBO config, edge buffer config, uniform buffer config, or shader pointers. |
| **CMD_CLEAR_FRAMEBUFFER** | `0x03`       | `ClearFramebufferPayload`<br>• `uint8_t options`               | Clears the currently bound framebuffer. Future options may include masks or clear color modes.                         |

---

### Payload Table

| Payload Type                | Fields                                                                                                                                     | Description                                                                   |
| --------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------- |
| **DrawPrimitivePayload**    | `PrimitiveType type`                                                                                                                       | Specifies what primitive to draw: POINTS, LINES, or TRIANGLES.                |
| **SetStatePayload**         | `StateID state_id`<br>`value` (union):<br>• `GenericBufferConfig buffer_config`<br>• `{ uint32_t vs_addr; uint32_t fs_addr; } shader_ptrs` | Updates GPU internal state. The union member used depends on `state_id`.      |
| **ClearFramebufferPayload** | `uint8_t options`                                                                                                                          | Reserved for future clear modes. Currently performs a full framebuffer clear. |
| **GenericBufferConfig**     | `uint32_t vbo_addr`<br>`uint32_t size`<br>`DataType element_type`                                                                          | Describes a buffer in VRAM, used for vertex, edge, or uniform data.           |



### Frame Buffer
Unlike Main Farme Buffer, it contains much more information as it could be blited with multiple other framebuffers to main FB

FRAME_BUFFER
|        Field      |               Purpose              |  Type  | Offset |
|-------------------|------------------------------------|--------|--------|
| Width             | Framebuffer width                  | uint32 |   0    |
| Height            | Framebuffer height                 | uint32 |   4    |
| TotalSize         | Total size of framebuffer in bytes | uint32 |   8    |
| ColorFormat       | Color format used by this buffer   | uint32 |   12   |
| FrameBufferPtr    | Pointer to framebuffer memory      | void*  |   16   |

### Pipeline
Pipeline is object containg instructions for rendering given objects

PIPELINE
|        Field      |                   Purpose                     |  Type  | Offset |
|-------------------|-----------------------------------------------|--------|--------|
| DrawType          | Sets rasterization type (point, line, fill)   | uint32 |   0    |
| VertexShaderPtr   | Pointer to vertex shader structure            | void*  |   4    |
| FragmentShaderPtr | Pointer to fragment shader structure          | void*  |   8    |
| FrameBufferPtr    | Pointer to frame buffer structure             | void*  |   12   |


Below is a **clear, technical, self-contained description** of how shaders work in your GPU architecture, based on the structures you defined.

---

### Shaders

Your GPU uses a **custom, software-executed shader model** with two programmable stages:

1. **Vertex Shader (VS)**
2. **Fragment Shader (FS)**

Both shader types are stored in **VRAM** as structured shader objects made of:

* A header (`Shader_Header`)
* A linear array of instructions (`code_section`)

The GPU interprets and executes these instructions when processing vertices and fragments.

Each shader in VRAM is represented by:

```
typedef struct {
    Shader_Header header_section;
    uint32_t code_section;
} Shader;
```


#### Shader_Header

```
typedef struct {
    uint32_t shader_type;        // VS or FS
    uint32_t num_instructions;
    
    uint32_t num_uniforms;
    ShaderResourceMap uniform_map[MAX_UNIFORMS_PER_SHADER];
    
    uint32_t num_attributes;
    AttributeMap attribute_map[MAX_ATTRIBUTES_PER_SHADER];
} Shader_Header;
```

#### • `shader_type`

* Identifies this is a **vertex** or **fragment** shader.

#### • `num_instructions`

* Instruction count for the interpreter loop.

#### • Uniform binding table

Describes **where in the Uniform Buffer** each uniform lives in current uniform buffer.

```
ShaderResourceMap {
    DataType data_type;
    uint32_t offset_in_buffer;
}
```

#### • Attribute binding table

Describes **how attributes are read** from the active VBO.

Each entry:

```
AttributeMap {
    DataType data_type;
    uint32_t offset;
}
```

Below is a **clear, technical description of all buffer configuration structures** used in your GPU (VBO, edge buffer, uniform buffer). It explains their purpose, how they are bound, and how shaders use them.

---

## Buffers

GPU uses a unified buffer description type:

```
typedef struct {
    uint32_t vbo_addr;      // VRAM address of the buffer
    uint32_t size;          // Size in bytes
    DataType element_type;  // Type of each element in this buffer
} GenericBufferConfig;
```

This single structure is used for:

* **Vertex Buffer Object (VBO)**
* **Edge buffer**
* **Uniform buffer**

Each buffer type is activated by sending a **CMD_SET_STATE** command with the appropriate `StateID`.

---

###  Vertex Buffer Configuration (VBO)

####  State ID:

```
STATE_ID_VBO_CONFIG
```

####  Structure Used:

`GenericBufferConfig buffer_config`

####  Purpose:

Defines the source of **vertex attributes** for the Vertex Shader.

####  Fields:

| Field          | Meaning                                                      |
| -------------- | ------------------------------------------------------------ |
| `vbo_addr`     | VRAM address where vertex data begins                        |
| `size`         | Total size in bytes of the vertex buffer                     |
| `element_type` | Format of one vertex element (FLOAT, VEC2, VEC3, VEC4) |

---

### Edge Buffer Configuration

#### State ID:

```
STATE_ID_EDGE_CONFIG
```

#### Structure:

`GenericBufferConfig buffer_config`

#### Purpose:

Specifies the index/edge list used by line rendering or other primitive assembly logic.

#### Fields (same structure as VBO):

* `vbo_addr` → address of index buffer in VRAM
* `size` → size of index array
* `element_type` → usually `D_TYPE_UINT32`


### Uniform Buffer Configuration

#### State ID:

```
STATE_ID_UNIFORM_CONFIG
```

#### Structure:

`GenericBufferConfig buffer_config`

#### Purpose:

Holds all uniform values required by shaders (VS and FS).

### How Uniforms Are Accessed:

Uniform access is determined by the shader’s `uniform_map`

```
ShaderResourceMap {
    DataType data_type;
    uint32_t offset_in_buffer;
}
```

The actual value fetch is:

```
address = uniform_buffer.vbo_addr + offset_in_buffer
```


## Memory managment

#### TODOS
- Finish implementation description
This version implements basic "Buddy allocator" for its memory allocation. 

Each of structure or buffer be it pipeline or simple vertex data buffer is allocated dynamicly using memory allocator.
