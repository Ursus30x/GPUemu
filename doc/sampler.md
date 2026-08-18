# 2D Texture Sampling Support

This document details the 2D Texture Sampling pipeline implemented across the GPU emulator, JIT compiler, renderer, and UEFI/EDK2 GOP3D API.

---

## 1. Architecture Overview

```
Application (UEFI / GOP3D)
  │
  ├─► Upload Pixel Data (Gop3dBufferTypeTexture) ──► VRAM (hTexData)
  ├─► Upload Texture Descriptor (GOP_3D_TEXTURE_DESC) ──► VRAM (hTexDesc)
  └─► Bind Texture: GpuBindTexture(slot, hTexDesc) ──► Ring Buffer (CMD_SET_STATE, STATE_ID_TEXTURE_CONFIG)
                                                              │
                                                              ▼
                                                   GPU Emulator (QEMU)
                                                              │
                                                              ├─► Parse STATE_ID_TEXTURE_CONFIG
                                                              │   Populate GpuState.textures[slot]
                                                              │
                                                              ▼
                                                   Renderer (renderer.c)
                                                              │
                                                              ├─► bind_resources_to_context()
                                                              │   ExecutionContext.binding_buffers[slot] = &GpuState.textures[slot]
                                                              │
                                                              ▼
                                                   JIT SIMT Fragment Shading
                                                              │
                                                              └─► OpImageSampleImplicitLod / OpImageSampleExplicitLod
                                                                  └─► sample_texture_2d_simt() (16 lanes)
                                                                      ├─ Filter: Nearest / Bilinear
                                                                      └─ Wrap: Repeat / Clamp
```

---

## 2. API / Protocol (`Protocol/Gop3D.h` & `include/vram.h`)

### Data Structures & Enums

```c
typedef enum {
  Gop3dBufferTypeVertex,
  Gop3dBufferTypeIndex,
  Gop3dBufferTypeUniform,
  Gop3dBufferTypeShaderCode,
  Gop3dBufferTypeTexture,
  Gop3dBufferTypeTextureDesc
} GOP_3D_BUFFER_TYPE;

typedef enum {
  Gop3dFilterNearest = 0,
  Gop3dFilterLinear  = 1
} GOP_3D_FILTER_MODE;

typedef enum {
  Gop3dWrapRepeat = 0,
  Gop3dWrapClamp  = 1
} GOP_3D_WRAP_MODE;

typedef struct {
  VRAMADDR            DataAddr;    // VRAM offset where pixel bytes start
  UINT32              Width;
  UINT32              Height;
  UINT32              Channels;    // 1, 2, 3, 4
  GOP_3D_FILTER_MODE  Filter;      // 0: Nearest, 1: Linear
  GOP_3D_WRAP_MODE    Wrap;        // 0: Repeat, 1: Clamp
} GOP_3D_TEXTURE_DESC;
```

### Protocol Function

```c
/**
 * Binds a texture descriptor in VRAM to a specific shader sampler binding slot.
 * @param BindingSlot   The shader binding slot (e.g. layout(binding = X)).
 * @param DescAddress   VRAM address of the texture descriptor.
 */
typedef
EFI_STATUS
(EFIAPI *GOP_3D_BIND_TEXTURE)(
  IN GOP_3D_PROTOCOL      *This,
  IN UINT32               BindingSlot,
  IN VRAMADDR             DescAddress
);
```

### Hardware Command & Descriptor (`include/vram.h`)

```c
typedef struct __attribute__((packed)) {
    uint32_t data_vram_addr; // VRAM offset where pixel bytes start
    uint32_t width;
    uint32_t height;
    uint32_t channels;       // 1, 2, 3, 4
    uint32_t filter;         // 0: FILTER_NEAREST, 1: FILTER_LINEAR
    uint32_t wrap;           // 0: WRAP_REPEAT,    1: WRAP_CLAMP
} GpuTextureDescriptorVram;

typedef struct __attribute__((packed)) {
    uint32_t binding_slot;   // 0..MAX_BINDINGS-1
    uint32_t desc_vram_addr; // VRAM offset of GpuTextureDescriptorVram
} SetTexturePayload;

#define CMD_SET_TEXTURE(ring_buffer_base, slot, desc_addr) \
{ \
    Command cmd1 = { \
        .opcode = CMD_SET_STATE, \
        .payload.state = { \
            .state_id = STATE_ID_TEXTURE_CONFIG, \
            .value.texture_config = { \
                .binding_slot = slot, \
                .desc_vram_addr = desc_addr \
            }}}; \
    memcpy(ring_buffer_base + current_offset, &cmd1, cmd_size); \
    current_offset += cmd_size; \
}
```

---

## 3. GPU State & Command Processing (`gpu/gpu.c`, `gpu/gpu.h`)

`GpuState` maintains texture descriptors and addresses:
```c
uint32_t texture_desc_addr[MAX_BINDINGS];
TextureSamplerDescriptor textures[MAX_BINDINGS];
```

When receiving `STATE_ID_TEXTURE_CONFIG`, the GPU parses the VRAM descriptor and populates `textures[slot]`.

---

## 4. Renderer Integration (`gpu/renderer.c`)

- **Resource Binding**: `bind_resources_to_context()` binds both UBOs (`binding_buffers[0]`) and active texture descriptors (`binding_buffers[slot]`) into the JIT `ExecutionContext`.
- **SIMT Shading**: `execute_shader_and_write()` executes SIMT fragment shaders with `SimtVec4` outputs, converting normalized `[0.0, 1.0]` (or `[0..255]`) values to 32-bit framebuffer pixels via `color_to_u8()`.

---

## 5. Usage Example (DemoApp / Application)

```c
// 1. Upload raw texture bytes
VRAMADDR hTexData = 0;
mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeTexture, pixels, sizeof(pixels), &hTexData);

// 2. Upload texture descriptor
GOP_3D_TEXTURE_DESC desc = {
    .DataAddr = hTexData,
    .Width = 4,
    .Height = 4,
    .Channels = 4,
    .Filter = Gop3dFilterNearest,
    .Wrap = Gop3dWrapRepeat
};
VRAMADDR hTexDesc = 0;
mGOP3D->GpuTransferBuffer(mGOP3D, Gop3dBufferTypeTextureDesc, &desc, sizeof(desc), &hTexDesc);

// 3. Record draw commands
mGOP3D->GpuCmdBegin(mGOP3D);
mGOP3D->GpuClearFrame(mGOP3D, 0xFF000000);
mGOP3D->GpuBindVertShader(mGOP3D, hVS, vs_size);
mGOP3D->GpuBindFragShader(mGOP3D, hFS, fs_size);
mGOP3D->GpuBindVBO(mGOP3D, hVBO, vert_count);
mGOP3D->GpuBindIBO(mGOP3D, hIBO, index_count);
mGOP3D->GpuBindTexture(mGOP3D, 1, hTexDesc); // Bind texture descriptor to slot 1
mGOP3D->GpuDraw(mGOP3D, Gop3dTopologyTriangles, index_count * 3);
mGOP3D->GpuCmdEnd(mGOP3D);
mGOP3D->GpuPresent(mGOP3D);
```