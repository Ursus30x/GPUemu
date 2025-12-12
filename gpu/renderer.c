#include "renderer.h"
#include "math3d.h"
#include "math.h"
#define DEDUG_MAT
void put_pixel(GpuState *gpu, int x, int y, uint32_t color)
{
    if(0) // TO DO TRIGGER FRAGMENT
    {
        gpu->pRegs[REG_PX].u32 = x;
        gpu->pRegs[REG_PY].u32 = y;
        uint8_t r  = (color >> 16) & 0xFF;
        uint8_t g  = (color >> 8) & 0xFF;
        uint8_t b  = color & 0xFF;
        gpu->pRegs[REG_PR].u32 = r;
        gpu->pRegs[REG_PG].u32 = g;
        gpu->pRegs[REG_PB].u32 = b;
       // exec_shader(gpu, REG_FRAGMENT_SHADER(gpu));
        color = (gpu->pRegs[REG_PR].u32 << 16) |
                (gpu->pRegs[REG_PG].u32 << 8)  |
                    gpu->pRegs[REG_PB].u32;         
    }

    FB(gpu)[y * GPU_FB_WIDTH + x] = color;
}



void draw_line(GpuState *gpu, int x0, int y0, int x1, int y1, uint32_t color1, uint32_t color2) 
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    int length = (int)sqrtf((x1 - x0)*(x1 - x0) + (y1 - y0)*(y1 - y0));
    if (length == 0) length = 1;
    int step = 0;

    for (;;) 
    {
        float t = (float)step / length;

        uint8_t a1 = (color1 >> 24) & 0xFF;
        uint8_t r1 = (color1 >> 16) & 0xFF;
        uint8_t g1 = (color1 >> 8) & 0xFF;
        uint8_t b1 = color1 & 0xFF;

        uint8_t a2 = (color2 >> 24) & 0xFF;
        uint8_t r2 = (color2 >> 16) & 0xFF;
        uint8_t g2 = (color2 >> 8) & 0xFF;
        uint8_t b2 = color2 & 0xFF;

        uint32_t color = ((uint32_t)(a1 + t*(a2 - a1)) << 24) |
                         ((uint32_t)(r1 + t*(r2 - r1)) << 16) |
                         ((uint32_t)(g1 + t*(g2 - g1)) << 8) |
                         ((uint32_t)(b1 + t*(b2 - b1)));

        put_pixel(gpu, x0, y0, color);

        if (x0 == x1 && y0 == y1) break;

        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
        step++;
    }
}





uint8_t cmp_u32(uint32_t a, uint32_t b, uint8_t flag)
{
    
    switch (flag) {
        case C_FLAG_EQ: return a == b;
        case C_FLAG_NEQ: return a != b;
        case C_FLAG_LT: return a <  b;
        case C_FLAG_GT: return a >  b;
        case C_FLAG_LTE: return a <= b;
        case C_FLAG_GTE: return a >= b;
        default: return 0;
    }
}

uint8_t cmp_f32(float a, float b, uint8_t flag)
{
    switch (flag) {
        case C_FLAG_EQ: return a == b;
        case C_FLAG_NEQ: return a != b;
        case C_FLAG_LT: return a <  b;
        case C_FLAG_GT: return a >  b;
        case C_FLAG_LTE: return a <= b;
        case C_FLAG_GTE: return a >= b;
        default: return 0;
    }
}

static inline InstrArg get_arg_scalar_value(GpuState *gpu, uint8_t argType, InstrArg arg)
{
    if(argType == ARG_TYPE_IMM) return arg;
        return gpu->pRegs[arg.u32];
}

#define ARITHMETIC_OP(op) \
            InstrArg b = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1); \
            if(instr.opType == OP_TYPE_U32) \
                 gpu->pRegs[instr.arg0.u32].u32 op b.u32; \
            else \
                gpu->pRegs[instr.arg0.u32].f32 op b.f32;\

void exec_shader(GpuState *gpu, uint32_t program_offset)
{
    void *shader_segment = SHADER_PROGRAM(gpu);
    void *program_address = shader_segment + program_offset;
    int end = 1;
    do{ 
        Instr instr = *(Instr*)program_address;
        if(instr.cFlag == C_FLAG_ENABLE && gpu->cFlag != 1)
        {
            program_address+=sizeof(Instr);
            continue;
        }
        switch (instr.opcode)
        {
        
        case INSTR_MOV:
        {
            if(instr.opType == OP_TYPE_MATRIX)
            {
                gpu->regs[instr.dest] = gpu->regs[instr.arg0.u32];
                break;
            }
            InstrArg a = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            gpu->pRegs[instr.dest] = a;
            break;
        }
        case INSTR_ROTX:
        {
            InstrArg rot = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            gpu->regs[instr.dest] = mat4_rotate_x(rot.f32);
            print_mat4(&gpu->regs[instr.dest], "ROTX");
            break;
        }
        case INSTR_ROTY:
        {
            InstrArg rot = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            gpu->regs[instr.dest] = mat4_rotate_y(rot.f32);
            print_mat4(&gpu->regs[instr.dest], "ROTY");
            break;
        }
        case INSTR_SIN:
        {
            InstrArg rot = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            gpu->pRegs[instr.dest].f32 = sinf(rot.f32);
            break;
        }
        case INSTR_COS:
        {
            InstrArg rot = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            gpu->pRegs[instr.dest].f32 = cosf(rot.f32);
            break;
        }
        case INSTR_SQRT:
        {
            InstrArg rot = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            gpu->pRegs[instr.dest].f32 = sqrtf(rot.f32);
            break;
        }
        case INSTR_FSAN:
        {
            float val = gpu->pRegs[instr.dest].f32;
            if(!isfinite(val))
                gpu->pRegs[instr.dest].f32 = 0.0f;
            break;
        }
        case INSTR_IDENT:
        {
            gpu->regs[instr.dest] = mat4_identity();
            break;
        }
        case INSTR_MVP:
        {
            gpu->mvp = gpu->regs[instr.dest];
            print_mat4(&gpu->regs[instr.dest], "MVP");
            break;
        }
        case INSTR_MOD:
        {
            InstrArg a = gpu->pRegs[instr.arg0.u32];
            InstrArg b = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
            gpu->pRegs[instr.dest].u32 = a.u32 % b.u32;
            break;
        }

        case INSTR_COL:
        {
            InstrArg r = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg1);
            InstrArg g = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
            InstrArg b = get_arg_scalar_value(gpu, instr.arg2Type, instr.arg2);

            gpu->pr = r.u32;
            gpu->pg = g.u32;
            gpu->pb = b.u32;
            break;
        }
        case INSTR_ABS:
        {
            InstrArg a = gpu->pRegs[instr.arg0.u32];

            if(instr.opType == OP_TYPE_F32)
                gpu->pRegs[instr.dest].f32 = fabsf(a.f32);
            else 
                gpu->pRegs[instr.dest].u32 = abs(a.u32);
            
            break;
        }
        // a + (b - a) * t
        case INSTR_LERP:
        {
            InstrArg a = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg1);
            InstrArg b = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
            InstrArg t = get_arg_scalar_value(gpu, instr.arg2Type, instr.arg2);

            if(instr.opType == OP_TYPE_F32)
                gpu->pRegs[instr.dest].f32 = a.f32 + (b.f32 - a.f32) * t.f32;
            else 
                gpu->pRegs[instr.dest].u32 = (int)((float)(a.u32 + (b.u32 - a.u32)) * t.f32);
            
            break;
        }
        // a * (1 - weight) + b * weight
        case INSTR_BLEND:
        {
            InstrArg a = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg1);
            InstrArg b = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
            InstrArg w = get_arg_scalar_value(gpu, instr.arg2Type, instr.arg2);

            if(instr.opType == OP_TYPE_F32)
                gpu->pRegs[instr.dest].f32 = a.f32 * (1.0 - w.f32)  + b.f32 * w.f32;
            else 
                gpu->pRegs[instr.dest].u32 = (int)(a.u32 * (1.0 - w.f32)  + b.u32 * w.f32);      
            break;
        }
        case INSTR_MUL:
        {
            if(instr.opType == OP_TYPE_MATRIX)
            {
                Mat4 *a =  &gpu->regs[instr.arg0.u32];
                Mat4 *b =  &gpu->regs[instr.arg1.u32];
                gpu->regs[instr.dest] = mat4_mul(a,b);
                print_mat4(&gpu->regs[instr.dest], "MUL");
                break;
            }
           ARITHMETIC_OP(*=);
           break;
        }
        case INSTR_ADD:
        {
            ARITHMETIC_OP(+=);
            break;
        }
        case INSTR_SUB:
        {
            ARITHMETIC_OP(-=);
            break;
        }
        case INSTR_DIV:
        {
            ARITHMETIC_OP(/=);
            break;
        }
        case INSTR_CMP:
        {
            InstrArg a = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            InstrArg b = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
            if(instr.opType == OP_TYPE_F32)
                gpu->cFlag = cmp_f32(a.f32, b.f32, instr.cFlag);
            else 
                gpu->cFlag = cmp_u32(a.u32, b.u32, instr.cFlag);   
            break;
        }
        case INSTR_CAST:
        {
            if(instr.opType == OP_TYPE_F32)
                gpu->pRegs[instr.arg0.u32].f32 = (float) (gpu->pRegs[instr.arg0.u32].u32);
            else 
                gpu->pRegs[instr.arg0.u32].u32 = (int)(gpu->pRegs[instr.arg0.u32].f32);   
            break;
        }
        case INSTR_TRANS:
        {
            InstrArg x = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg1);
            InstrArg y = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
            InstrArg z = get_arg_scalar_value(gpu, instr.arg2Type, instr.arg2);
            gpu->regs[instr.dest] = mat4_translate(x.u32, y.u32, z.u32);
            print_mat4(&gpu->regs[instr.dest], "TRANS");
            break;
        }
        case INSTR_EXIT:
            end = 0;
        //    / REG_EXEC_VERTEX_SHADER(gpu) = 0;
            return;
        break;
        }
        program_address+=sizeof(Instr);
    }while(end);

}

static float angle = 0;

void gpu_render_frame(void *opaque)
{
    GpuState *gpu = opaque;
    // if(1)//REG_EXEC_VERTEX_SHADER(gpu) == 1
    // {
    //     return;
    // }

    uint32_t width = gpu->width;
    uint32_t height =  gpu->height;
    uint32_t vertex_size = gpu->vbo_config.size;
    uint32_t edges_size =  gpu->edge_config.size;

        
    Vec3 *vertices = VERTEX_TABLE(gpu);
    Edge *edges = EDGES_TABLE(gpu);
    angle += 0.02;

    Mat4  ry        = mat4_rotate_y(0.2f);
    Mat4  rx        = mat4_rotate_x(angle);
    Mat4  model     = mat4_mul(&ry,&rx);
    Mat4  translate = mat4_translate(0, 0, 5);
    model           = mat4_mul(&translate, &model);

    Mat4  proj      = mat4_perspective(PI/3, (float)width/height, 1.0f, 10.0f);
    Mat4  mvp_local = mat4_mul(&proj, &model);

    print_mat4(&mvp_local, "MVP RENDER");
    
    uint32_t *px = malloc(sizeof(uint32_t)* vertex_size);
    uint32_t *py = malloc(sizeof(uint32_t)* vertex_size);
    for(uint32_t i=0;i<vertex_size;i++) 
    {
        Vec4 v = {vertices[i].x, vertices[i].y, vertices[i].z, 1.0f};
        Vec4 tv = mat4_mul_vec4(&mvp_local, v);
        float ndc_x = tv.x / tv.w;
        float ndc_y = tv.y / tv.w;
        px[i] = (int)((ndc_x*0.5f + 0.5f) * width);
        py[i] = (int)((-ndc_y*0.5f + 0.5f) * height);
    }
    for(uint32_t i=0;i<edges_size;i++)
    {
        Edge e = edges[i];
        draw_line(gpu, px[e.a], py[e.a], px[e.b], py[e.b],  vertices[e.a].rgba, vertices[e.b].rgba);
    }
    free(px);
    free(py);
}
