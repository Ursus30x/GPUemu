#include "renderer.h"
#include "math3d.h"

void put_pixel(GpuState *gpu, int x, int y, uint32_t color)
{
    // gpu->pRegs[REG_PX].u32 = x;
    // gpu->pRegs[REG_PY].u32 = y;
    // uint8_t r  = (color >> 16) & 0xFF;
    // uint8_t g  = (color >> 8) & 0xFF;
    // uint8_t b  = color & 0xFF;
    // gpu->pRegs[REG_PR].u32 = r;
    // gpu->pRegs[REG_PG].u32 = g;
    // gpu->pRegs[REG_PB].u32 = b;
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


 
void exec_shader(GpuState *gpu)
{
    uint32_t program_offset = REG_VERTEX_SHADER(gpu);
    void *shader_segment = SHADER_PROGRAM(gpu);
    void *program_address = shader_segment + program_offset;
    int end = 1;
    #define CHECK_REG_MAT_NUM(num) if(!REG_MAT_NUM_OK(num)){printf("Panic invalid regnum!\n");end = 0;break;}
    #define CHECK_REG_P_NUM(num) if(!REG_P_NUM_OK(num)){printf("Panic invalid regnum!\n");end = 0;break;}

    do{ 
        Instr instr = *(Instr*)program_address;
        switch (instr.opcode)
        {
        
        case INSTR_MOV:
        {
            switch (instr.opType) 
            {
                case OP_TYPE_MAT: 
                {
                    CHECK_REG_MAT_NUM(instr.dst);
                    if(ARG_IS_MEM_ADDR(instr.arg0.u32))
                        memcpy(&gpu->regs[instr.dst], shader_segment+instr.arg0.u32,  sizeof(Mat4));
                    else
                    {
                        uint32_t src = REG_NUM(instr.arg0.u32);
                        CHECK_REG_MAT_NUM(src);
                        gpu->regs[instr.dst] = gpu->regs[src];
                    }
                    break;
                }
                case OP_TYPE_P_P:
                {
                    CHECK_REG_P_NUM(instr.dst);
                    CHECK_REG_P_NUM(instr.arg0.u32);
                    gpu->pRegs[instr.dst] = gpu->pRegs[instr.arg0.u32];
                    break;
                }
                case OP_TYPE_I32:
                case OP_TYPE_F32:
                {
                    CHECK_REG_P_NUM(instr.dst);
                    gpu->pRegs[instr.dst] = instr.arg0;
                    break;
                }
            
            }

            break;
        }
        case INSTR_MUL:
        {
            if (instr.opType == OP_TYPE_MAT)
            {
                /* Matrix multiply */
                CHECK_REG_MAT_NUM(instr.dst);

                Mat4 a = get_mat_from_arg(instr.arg0.u32, gpu->regs, shader_segment);
                Mat4 b = get_mat_from_arg(instr.arg1.u32, gpu->regs, shader_segment);

                gpu->regs[instr.dst] = mat4_mul(&a, &b);
                print_mat4(&gpu->regs[instr.dst], "mul");
            }
            else
            {
                EXEC_P_OP("MUL", *);
            }
            break;
        }
        case INSTR_ADD:
            EXEC_P_OP("ADD", +);
        break;
        case INSTR_DIV:
            EXEC_P_OP("div", /);
        break;
         case INSTR_SUB:
            EXEC_P_OP("SUB", -);
        break;
        case INSTR_MVP:
            CHECK_REG_MAT_NUM(instr.dst);
            gpu->mvp = gpu->regs[instr.dst];
            print_mat4(&gpu->mvp, "mvp");
            break;
        case INSTR_ROTX:
            CHECK_REG_MAT_NUM(instr.dst);
            gpu->regs[instr.dst] = mat4_rotate_x(instr.arg0.f32);
            print_mat4(&gpu->regs[instr.dst], "rotx");
            break;
        case INSTR_ROTY:
            CHECK_REG_MAT_NUM(instr.dst);
            gpu->regs[instr.dst] = mat4_rotate_y(instr.arg0.f32);
            print_mat4(&gpu->regs[instr.dst], "roty");
            break;
        case INSTR_TRANS:
            CHECK_REG_MAT_NUM(instr.dst);
            gpu->regs[instr.dst] = mat4_translate(instr.arg0.f32,instr.arg1.f32,instr.arg2.f32);
            print_mat4(&gpu->regs[instr.dst], "trans");
            break;
        case INSTR_IDENT:
            CHECK_REG_MAT_NUM(instr.dst);
            gpu->regs[instr.dst] = mat4_identity();
            break;
        case INSTR_EXIT:
            end = 0;
            REG_EXEC_VERTEX_SHADER(gpu) = 0;
            break;
        case INSTR_CMP:
        {
            CHECK_REG_P_NUM(instr.arg0.u32)
            CHECK_REG_P_NUM(instr.arg1.u32)
           
            Preg a = gpu->pRegs[instr.arg0.u32];
            Preg b = gpu->pRegs[instr.arg1.u32];
            if (instr.opType == OP_TYPE_I32)
                gpu->cFlag = cmp_u32(a.u32, b.u32, instr.cFlag);
            else if (instr.opType == OP_TYPE_F32)
                gpu->cFlag = cmp_f32(a.f32, b.f32, instr.cFlag);
            break;
        }
        default:
            printf("Panic unkown opcode!\n");
            end = 0;
            break;
        }
        program_address+=sizeof(Instr);
    }while(end);

}



void gpu_render_frame(void *opaque)
{
    GpuState *gpu = opaque;
    if(REG_EXEC_VERTEX_SHADER(gpu) == 1)
    {
        return;
    }

    uint32_t width =  REG_FB_WIDTH(gpu);
    uint32_t height =  REG_FB_HEIGHT(gpu);
    uint32_t vertex_size = REG_VERTEX_SIZE(gpu);
    uint32_t edges_size = REG_EDGE_SIZE(gpu);

    uint32_t *fb = FB(gpu);
    Vec3 *vertices = VERTEX_TABLE(gpu);
    Edge *edges = EDGES_TABLE(gpu);

    for(uint32_t i=0;i<width*height;i++) fb[i] = 0xFF000000;


    Mat4  ry        = mat4_rotate_y(0.2f);
    Mat4  rx        = mat4_rotate_x(0.2f);
    Mat4  model     = mat4_mul(&ry,&rx);
    Mat4  translate = mat4_translate(0, 0, 5);
    model           = mat4_mul(&translate, &model);

    Mat4  proj      = mat4_perspective(PI/3, (float)width/height, 1.0f, 10.0f);
    Mat4  mvp_local = mat4_mul(&proj, &gpu->mvp);
    
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
