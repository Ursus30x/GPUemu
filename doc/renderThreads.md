# Multithreaded Renderer Architecture

This document explains the high-performance, multithreaded architecture implemented in the renderer.c.

## High-Level Architecture Overview

The renderer transitions from a slow, single-threaded pipeline to a highly efficient Dual-Pass Parallel Architecture. It successfully solves the two classic bottlenecks of software rasterizers:

- Shader Register Clashing: Virtualized by creating isolated GpuState copies on each worker thread's local stack.

- Memory Overlap (Race Conditions): Handled by dividing the screen vertically into non-overlapping horizontal scanline bands (lock-free/atomic-free rasterization).

```mermaid
graph TD
    Start([gpu_render_triangles]) --> Alloc[Allocate Global TransformedVertex Cache]
    Alloc --> Pass1[PASS 1: Parallel Vertex Shading]
    Pass1 --> Sync1[pthread_join: Synchronization Barrier]
    Sync1 --> Pass2[PASS 2: Parallel Scanline-Band Rasterization]
    Pass2 --> Sync2[pthread_join: Frame Finalized]
    Sync2 --> Free[Free Vertex Cache]
    Free --> End([Frame Rendered & GPU Idle])


```

## PASS 1: Parallel Vertex Shading & Virtualization

This pass processes raw vertex data from `VERTEX_TABLE(gpu)`. By copying *(args->orig_gpu) to the thread's stack frame, each worker operates with isolated hardware registers during shader execution.

To avoid execution register corruption, each thread isolates its shader state. In addition, vertices are shaded exactly once instead of once per triangle, saving massive amounts of redundant vertex shader (`exec_shader`) execution.

### Thread Isolation and Memory Layout
```mermaid
graph TD
    VBO[(Input VBO: Raw Vertices)] -->|Divided by NUM_THREADS| Dispatcher{Work Dispatcher}
    
    Dispatcher -->|Chunk 0| T0[Worker Thread 0]
    Dispatcher -->|Chunk N| T1[Worker Thread N]
    
    subgraph "Thread 0 Stack"
        T0 -->|Deep Copy| L0[thread_local_gpu]
        L0 -->|exec_shader| S0[Clip-Space Pos & Color]
    end

    subgraph "Thread N Stack"
        T1 -->|Deep Copy| L1[thread_local_gpu]
        L1 -->|exec_shader| S1[Clip-Space Pos & Color]
    end

    S0 -->|Write| Cache[(Shared Shaded Vertex Cache)]
    S1 -->|Write| Cache
    
    classDef memory fill:#e1f5fe,stroke:#03a9f4,stroke-width:2px;
    classDef thread fill:#fff3e0,stroke:#ff9800,stroke-width:2px;
    class VBO,Cache memory;
    class T0,T1 thread;
```

### Process Details:

Every thread processes its allocated chunk of Vertices, writes input registers, runs exec_shader (the Vertex Shader), and caches the resultant Clip-Space Position and interpolated colors inside transformed_vertices.

## PASS 2: Lock-Free Horizontal Band Rasterization

This pass divides the framebuffer height dynamically among `NUM_RENDER_THREADS`. Because each thread strictly rasterizes a non-overlapping horizontal strip of screen coordinates, they read from the shared transformed_vertices array concurrently with zero thread contention or locking.

By partitioning the screen into horizontal bands, threads never touch the same pixel, completely eliminating the need for atomics.

```mermaid
graph TD
    subgraph "Shared Read-Only Memory (Heap)"
        TV["transformed_vertices (Position & Color Cache)"]
        IB["TRIANGLES_TABLE(gpu) (Triangle Indices)"]
    end

    subgraph "Thread 0 Execution (worker_rasterize_bands)"
        T0["Thread 0 Space <br/> band_min_y = 0 <br/> band_max_y = chunk_y - 1"]
        F0["For Loop: 0 to triangle_size - 1"]
        D0["draw_triangle_band(...)"]
    end

    subgraph "Thread i Execution (worker_rasterize_bands)"
        Ti["Thread i Space <br/> band_min_y = i * chunk_y <br/> band_max_y = (i+1)*chunk_y - 1"]
        Fi["For Loop: 0 to triangle_size - 1"]
        Di["draw_triangle_band(...)"]
    end

    subgraph "GPU VRAM"
        FB0["Framebuffer Rows <br/> [0 ... chunk_y - 1]"]
        FBi["Framebuffer Rows <br/> [start_y ... end_y - 1]"]
    end

    TV -->|Read Vertex Positions & Colors| F0
    IB -->|Read Triangle Node Indices| F0

    TV -->|Read Vertex Positions & Colors| Fi
    IB -->|Read Triangle Node Indices| Fi

    F0 --> D0
    Fi --> Di

    D0 -->|Lock-Free Writes| FB0
    Di -->|Lock-Free Writes| FBi

    classDef shared fill:#e1f5fe,stroke:#03a9f4,stroke-width:2px;
    classDef target fill:#ffebee,stroke:#f44336,stroke-width:2px;
    class TV,IB shared;
    class FB0,FBi target;
```

### Process Details:

*The Workload*: Every thread is assigned a vertical screen range ($start\_y$ to $end\_y$).

*Broadcasting Triangles*: Every thread loops through the entire index table of triangles. However, inside draw_triangle_band(), we compute the triangle's bounding box and intersect it with the thread's horizontal band:


$$min\_y = \max(band\_min\_y, \min(y_0, y_1, y_2))$$

$$max\_y = \min(band\_max\_y, \max(y_0, y_1, y_2))$$

*Fast Culling*: If $min\_y > max\_y$, the triangle does not overlap the thread's screen portion, and is instantly skipped, costing practically zero CPU cycles.

*Zero-Lock Writing*: If a triangle falls into the thread's band, the thread loops through the pixels. Since Thread 0 only writes to rows 0...119 and Thread 1 only writes to rows 120...239, they can safely execute standard reads and writes to both the Framebuffer and Z-Buffer with zero synchronization overhead.
