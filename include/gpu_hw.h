#ifndef GPU_HW_H
#define GPU_HW_H

#define TYPE_PCI_GPU_DEVICE "AREK"
#define GPU_DEVICE_ID       0x2137
#define PCI_VENDOR_ID_CUSTOM 0x6969

#define GPU_MMIO_BAR    0
#define GPU_VRAM_BAR    1

#define GPU_VRAM_SIZE (1 << 25)   /* 32 MB */
#define GPU_CMD_SIZE  0x1000      /* BAR0 command size */

/* MMIO Register Offsets */
#define REG_GPU_MODE_ADDR              0x0
#define REG_RING_BUFFER_HEAD_ADDR      0x4
#define REG_RING_BUFFER_TAIL_ADDR      0x8
#define REG_RING_BUFFER_START_ADDR     0xC
#define REG_RING_BUFFER_END_ADDR       0x10
#define REG_VERTEX_SHADER_ADDR         0x14
#define REG_FRAGMENT_SHADER_ADDR       0x18
#define REG_FB_WIDTH_ADDR              0x1c
#define REG_FB_HEIGHT_ADDR             0x20
#define REG_FRAMEBUFFER_ADDR           0x24
#define REG_GPU_TIME_ADDR              0x28
#define REG_ZBUFFER_ADDR               0x2C

typedef enum {
    GPU_MODE_GOP,
    GPU_MODE_3D,
    GPU_MODE_IDLE
} GpuMode;


#define GPU_FB_WIDTH  640
#define GPU_FB_HEIGHT 480
#endif