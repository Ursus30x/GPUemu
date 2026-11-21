# Shader Interfaces: `location` and `binding`

This guide explains how GPUemu maps GLSL `location` and `binding` qualifiers
onto vertex attributes, inter-stage values, and shader resources. The concepts
are familiar from Vulkan GLSL, but GPUemu currently implements a smaller,
project-specific interface. In particular, descriptor sets are not separate
binding namespaces, and only one fragment color output is written.

## At a Glance

| Qualifier | Identifies | Example |
| --- | --- | --- |
| `location` | An interface slot: vertex input, VS-to-FS varying, or FS output | `layout(location = 1) in vec3 uvw;` |
| `binding` | A shader resource slot: UBO, sampled texture, or compute SSBO | `layout(binding = 1) uniform sampler3D volume;` |
| `set` | Not currently used to select a resource namespace | `layout(set = 0, binding = 1)` resolves to binding 1 |

Do not use `location` as a resource index or `binding` as a vertex attribute
index. They are independent namespaces with different producers and consumers.

## `location`

### Vertex Inputs

A declaration such as `layout(location = N) in vec3 position;` requests vertex
attribute slot `N`. The application must configure and enable the same slot with
the GOP3D API. The GPU reads the attribute from each vertex using the configured
component count, type, stride, and byte offset.

For example, suppose each vertex has six tightly packed floats:

```c
typedef struct {
    float x, y, z;
    float u, v, w;
} Vertex;
```

The matching GLSL inputs and GOP3D configuration are:

```glsl
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 uvw;
```

```c
GpuCmdVertexAttribPointer(gop, 0, 3, Gop3dAttribFloat, FALSE,
                          sizeof(Vertex), OFFSET_OF(Vertex, x));
GpuCmdEnableVertexAttribArray(gop, 0);

GpuCmdVertexAttribPointer(gop, 1, 3, Gop3dAttribFloat, FALSE,
                          sizeof(Vertex), OFFSET_OF(Vertex, u));
GpuCmdEnableVertexAttribArray(gop, 1);
```

The arguments after `location` are the component count, attribute type,
normalization flag, byte stride, and byte offset. The stride is the size of one
`Vertex`; the offset selects the first component of that particular attribute.
For interleaved vertex data, attributes normally share a stride but have
different offsets. GPUemu supports one to four components per attribute and
locations 0 through 7.

The shader declaration alone does not describe the VBO layout. If, for example,
the shader reads three floats at location 1 but the application configures only
two, the third value is not supplied correctly. Keep the shader declaration,
vertex structure, and `GpuCmdVertexAttribPointer()` call in agreement.

### Vertex-to-Fragment Varyings

A vertex shader output and its fragment shader input form a varying interface.
Use the same location and compatible scalar/vector type on both sides. GPUemu
captures vertex shader outputs by their declared SPIR-V locations, then the
rasterizer interpolates those values and supplies them to the fragment shader
input at the same location.

```glsl
// volume.vert
layout(location = 1) in vec3 inUVW;
layout(location = 0) out vec3 texCoord;

void main() {
    gl_Position = ...;
    texCoord = inUVW;
}
```

```glsl
// volume.frag
layout(location = 0) in vec3 texCoord;
layout(binding = 1) uniform sampler3D volumeTexture;
layout(location = 0) out vec4 fragColor;

void main() {
    fragColor = texture(volumeTexture, texCoord);
}
```

For a 3D texture, `texCoord` carries all three coordinates `(u, v, w)`. It is
not sufficient to configure a `vec3` vertex attribute: the vertex shader must
also write all three components to an output, and the fragment shader must read
them from the matching location. This is the full path:

```text
VBO attribute location 1 (uvw)
    -> vertex shader input location 1
    -> vertex shader output location 0
    -> rasterizer interpolation at location 0
    -> fragment shader input location 0
    -> texture(volumeTexture, texCoord)
```

Triangle varyings use perspective-correct interpolation. GPUemu currently does
not implement GLSL interpolation qualifiers such as `flat` or `noperspective`;
they do not change this behavior. The varying storage has eight locations
(0-7), with up to four components per location. `gl_Position` is a built-in
vertex output and is handled separately from user varying locations.

GPUemu does not perform a complete VS/FS interface-link validation. A mismatched
location or type can therefore produce incorrect values rather than a useful
link-time diagnostic. Matching declarations are the shader author's
responsibility.

### Legacy Fragment Inputs

For compatibility with older shaders, a fragment input can use a built-in
fallback when the vertex shader does not declare an output at that location:

| Fragment input location | Legacy fallback value | Typical type |
| --- | --- | --- |
| 0 | Vertex color | `vec4` |
| 1 | Texture coordinates | `vec2` |
| 2 | Vertex normal | `vec3` |

An explicitly declared vertex output takes precedence at its location, even if
its value is zero. Prefer explicit matching VS outputs and FS inputs in new
shaders; fallback values are for compatibility, not a substitute for a declared
interface.

Fragment color output is currently written to the framebuffer from location 0.
Additional fragment output locations are not connected to separate color
attachments.

## `binding`

`layout(binding = N)` selects resource slot `N` in the shader execution context.
The application must bind the corresponding VRAM resource to that same slot.
Binding is independent of the resource's address in VRAM: the shader sees the
slot, while the GOP3D command associates that slot with an address or texture
descriptor.

### Graphics Resource Slots

| Binding | Graphics resource | GOP3D API |
| --- | --- | --- |
| 0 | Uniform buffer (UBO) | `GpuCmdBindUBO()` |
| 1-7 | Sampled texture and sampler descriptor | `GpuCmdBindTexture()` |

Example GLSL declarations:

```glsl
layout(binding = 0) uniform Matrices {
    mat4 mvp;
} matrices;

layout(binding = 1) uniform sampler3D volumeTexture;
```

Example resource binding from UEFI code:

```c
GpuCmdBindUBO(gop, mvpAddress, mvpSize);
GpuCmdBindTexture(gop, 1, textureDescriptorAddress);
```

For a texture, `GpuCmdBindTexture()` receives the descriptor address, not the
pixel-data address. The descriptor points to the texture data and contains such
properties as dimension, size, filtering, and wrap modes. Binding slot 0 is
reserved for the graphics UBO in the current renderer; sampled textures are
looked up in slots 1 through 7. The resource array has eight entries total.

### Compute SSBO Slots

Compute shaders use `binding` to select SSBO slots. `GpuCmdBindSSBO()` associates
each slot with a VRAM address and byte size:

```glsl
layout(binding = 0) buffer InputData {
    uint values[];
} inputData;

layout(binding = 1) buffer OutputData {
    uint values[];
} outputData;
```

```c
GpuCmdBindSSBO(gop, 0, inputAddress, inputSize);
GpuCmdBindSSBO(gop, 1, outputAddress, outputSize);
```

Compute has a UBO pointer in slot 0 as well. UBOs and SSBOs share the same
resource-pointer array, so an SSBO bound at binding 0 overwrites the UBO pointer
there. Do not bind both a compute UBO and an SSBO at binding 0. Use another
binding for the SSBO when the shader needs the UBO at 0.

The compute worker binds SSBO slots 0-7 from the GPU state. The graphics
`bind_resources_to_context()` path described above binds the UBO at 0 and
sampled textures at 1-7. Do not assume that a binding used by one pipeline stage
or resource class is automatically shared with every other stage.

## Descriptor Sets and Supported Limits

The SPIR-V JIT records descriptor-set decorations, but resource lookup uses the
binding number directly. `set` is not part of the runtime slot calculation. For
example:

```glsl
layout(set = 0, binding = 1) uniform sampler2D image;
```

resolves to slot 1. Declaring the same binding number in two different sets
does not create two distinct GPUemu resources. Keep `set` at 0 or omit it, and
use unique binding numbers within the active resource model.

| Interface | Current range / behavior |
| --- | --- |
| Vertex attributes and varyings | Locations 0-7 |
| Resource pointer slots | Bindings 0-7 |
| Fragment color outputs | Location 0 |
| Graphics UBO | Binding 0 |
| Graphics sampled textures | Bindings 1-7 |
| Compute SSBOs | Bindings 0-7; binding 0 conflicts with the UBO pointer |

These are implementation limits, not universal GLSL limits. Arrays and matrix
interfaces spanning multiple locations, multiple render targets, multiple
descriptor sets, and per-varying interpolation modes should not be assumed to
work without extending the JIT and renderer.

## Troubleshooting

- **A varying is zero or has unrelated values:** check that VS `out` and FS
  `in` use the same location, that the VS actually assigns it, and that its
  location is below 8.
- **A `vec3` texture coordinate samples as 2D or has a wrong depth coordinate:**
  verify that the VBO attribute has three components, the VS output is `vec3`,
  and the FS input is `vec3` at the same location.
- **A texture is unbound:** check that `layout(binding = N)` equals the slot in
  `GpuCmdBindTexture()`, and pass the texture descriptor address to that command.
- **A UBO appears to be overwritten in compute:** check whether an SSBO is also
  bound at binding 0.
- **Two resources in different sets alias:** GPUemu currently ignores `set`;
  give resources distinct binding numbers instead.
- **A second fragment output is missing:** only fragment output location 0 is
  currently written to the framebuffer.

## Implementation References

- Vertex fetch, varying storage, and raster interpolation:
  [gpu/rasterizer_simt.c](../gpu/rasterizer_simt.c)
- SPIR-V input/output/resource decoration handling:
  [gpu/jit/jit_mem.c](../gpu/jit/jit_mem.c)
- JIT location and binding limits:
  [gpu/jit/jit.h](../gpu/jit/jit.h)
- Shader compilation and resource state:
  [gpu/gpu.c](../gpu/gpu.c)
- GOP3D resource and vertex-attribute commands:
  [UEFI/OptionRom/gop3d.c](../UEFI/OptionRom/gop3d.c)
- 3D volume varying example:
  [UEFI/SamplerTest/volume3d_vs.h](../UEFI/SamplerTest/volume3d_vs.h) and
  [UEFI/SamplerTest/volume3d_fs.h](../UEFI/SamplerTest/volume3d_fs.h)