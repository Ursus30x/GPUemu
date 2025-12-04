# GPU Architecture 1.0
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


|        Name           |         Prupose                                                    |  R/W  | Size | Type   | Address |
|-----------------------|--------------------------------------------------------------------|-------|------|--------|---------|
| GPU_MODE              | Specifies if GPU is in GOP mode or 3D MODE                         |  RW   |  1B  | uint8  |    0    |
| GPU_READY             | Bits represeting readiness state of FBs (Future mulit-buffering)   |   R   |  1B  | bits   |    1    |
| GPU_UPDATE **?**      | Bits controling work of the GPU? Probably should be as IRQ lines?  |  RW   |  1B? | bits?  |    2    |
| GPU_TIMER             | GPUs internal timer                                                |   R   |  4B  | uint32 |    3    |
| GPU_COLOR_FORMAT      | Dictates color format used to output framebuffer data              |  RW   |  1B? | uint8  |    7    |
| GPU_FB_WIDTH          | Dictates frame buffer width in pixels (Set during driver start)    |  RW   |  4B  | uint32 |    8    |
| GPU_FB_HEIGHT         | Dictates frame buffer height in pixels (Set during driver start)   |  RW   |  4B  | uint32 |    12   |
| GPU_FB_MAIN_ADDR      | Main frame buffer address (Framebuffer thats presented on screen)  |  R    |  4B  | uint32 |    16   |
| GPU_CONTEXT_MAIN_ADDR | Main GPU context address                                           |  R    |  4B  | uint32 |    20   |


## VRAM BAR [BAR 1]
VRAM BAR holds data needed for pipeline construction, framebuffer region and all data regarding vertecies and edges.

In VRAM instead of segmenting the memory and allocating data with dicated bounds, we create it dynamicly using data structures.

First we need to start with:
### Main Frame Buffer:
This is just a linear buffer of memory used as a main display frambuffer, its size is dependant on inital parameters set during driver start.

Starts at 0x0 adress of VRAM BAR


### Main context:
Main context is used to track segments of data such as frambuffers, object structures, shader code.

Start at an offset of main frame buffer size.

#### TODOS 
- better names?

MAIN_CONTEXT
|        Field              |                           Purpose                 |  Type  | Offset |
|---------------------------|---------------------------------------------------|--------|--------|
| Fb_Array_Size             | Current amount of framebuffers                    | uint32 |   0    |  
| Fb_Array_Ptr              | Pointer to array holding pointers to framebuffers | void*  |   4    |
| Pipeline_Array_Size       | Current amount of framebuffers                    | uint32 |   8    |
| Pipeline_Array_Ptr        | Pointer to array holding pointers to pipelines    | void*  |   12   |
| Object_Array_Size         | Current amount of framebuffers                    | uint32 |   16   |
| Object_Array_Ptr          | Pointer to array holding pointers to objects      | void*  |   20   |
| Shadre_Array_Size         | Current amount of framebuffers                    | uint32 |   24   |
| Shader_Array_Ptr          | Pointer to array holding pointers to shaders      | void*  |   28   |

1. Offsets are dependant on BAR addressing, we assume 32-bit addressing

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

### Object 
Object represents a single model

OBJECT
|        Field      |                   Purpose                     |  Type  | Offset |
|-------------------|-----------------------------------------------|--------|--------|
| VertexDataSize    | Amount of verts present in vertex buffer      | uint32 |   0    |
| VertexDataPtr     | Pointer to vertex data buffer                 | void*  |   4    |
| IndiciesDataSize  | Amount of indicies present in indicies buffer | uint32 |   8    |
| IndiciesDataPtr   | Pointer to indicies data buffer               | void*  |   12   |


### Shader
It contains code needed for GPU to compute the output

Its just a linear stream of instructions in memory

SHADER
|        Field      |                   Purpose                |  Type  | Offset |
|-------------------|------------------------------------------|--------|--------|
| Type              | Declares type of the shader              | uint32 |   0    |  
| Size              | Size of the code buffer in bytes         | uint32 |   4    |
| ShaderCodePtr     | Pointer to buffer containing shader code | void*  |   8    |

## Memory managment

#### TODOS
- Finish implementation description
This version implements basic "Buddy allocator" for its memory allocation. 

Each of structure or buffer be it pipeline or simple vertex data buffer is allocated dynamicly using memory allocator.
