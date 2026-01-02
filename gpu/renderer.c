#include "renderer.h"
#include "math3d.h"
#include "math.h"
#include "debug_gpu.h"

#define PRINT_V4(v) DEBUG_PRINT("[%f, %f, %f, %f]\n", v.x, v.y, v.z, v.w);


void put_pixel(GpuState *gpu, int x, int y, uint32_t color)
{
    if(1) // TO DO TRIGGER FRAGMENT
    {
        gpu->pRegs[REG_PX].u32 = x;
        gpu->pRegs[REG_PY].u32 = y;
        uint8_t r  = (color >> 16) & 0xFF;
        uint8_t g  = (color >> 8) & 0xFF;
        uint8_t b  = color & 0xFF;
        gpu->pRegs[REG_PR].u32 = r;
        gpu->pRegs[REG_PG].u32 = g;
        gpu->pRegs[REG_PB].u32 = b;
        exec_shader(gpu, gpu->fs_code_addr);     
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
    void *program_address = gpu->vram_ptr+program_offset;
    void *program_begin =program_address;
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
            gpu->v_out.right = gpu->regs[instr.dest].right;
            print_mat4(&gpu->regs[instr.dest], "V_OUT");
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
            InstrArg a = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
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
            InstrArg a = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
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
            if(instr.opType == OP_TYPE_VEC4)
            {
                Mat4 *a =  &gpu->regs[instr.arg0.u32];
                Vec4 b = gpu->regs[instr.arg1.u32].right;
                if(instr.arg1.u32 == REG_M_IN)
                    b = gpu->v_pos.right;
                Vec4 res = mat4_mul_vec4(a,b);
                gpu->regs[instr.dest].right = res;
                print_mat4(&gpu->regs[instr.dest], "MUL");
                break;
            }
            if (instr.opType == OP_TYPE_VEC3)
            {
                Mat4 *a =  &gpu->regs[instr.arg0.u32];
                Vec3Raw b = gpu->regs[instr.arg1.u32].vec3;
                Vec4 b4 = {b.x, b.y, b.z, 1.0f};
                Vec4 res = mat4_mul_vec4(a,b4);
                gpu->regs[instr.dest].vec3.x = res.x;
                gpu->regs[instr.dest].vec3.y = res.y;
                gpu->regs[instr.dest].vec3.z = res.z;
                print_mat4(&gpu->regs[instr.dest], "MUL");
                break;
            }
           ARITHMETIC_OP(*=);
           break;
        }
        case INSTR_ADD:
        {
            if(instr.opType == OP_TYPE_VEC4)
            {
                Vec4 a = gpu->regs[instr.arg0.u32].right;
                Vec4 b = gpu->regs[instr.arg1.u32].right;
                if(instr.arg1.u32 == REG_M_IN)
                    b = gpu->v_pos.right;
                Vec4 res = vec4_add(a,b);
                gpu->regs[instr.dest].right = res;
                print_mat4(&gpu->regs[instr.dest], "ADD");
                break;
            }
            if (instr.opType == OP_TYPE_VEC3)
            {
                Vec3Raw a = gpu->regs[instr.arg0.u32].vec3;
                Vec3Raw b = gpu->regs[instr.arg1.u32].vec3;
                Vec3Raw res;
                res.x = a.x + b.x;
                res.y = a.y + b.y;
                res.z = a.z + b.z;
                gpu->regs[instr.dest].vec3 = res;
                print_mat4(&gpu->regs[instr.dest], "ADD");
                break;
            }
            
            ARITHMETIC_OP(+=);
            break;
        }
        case INSTR_SUB:
        {
            if (instr.opType == OP_TYPE_VEC4)
            {
                Vec4 a = gpu->regs[instr.arg0.u32].right;
                Vec4 b = gpu->regs[instr.arg1.u32].right;
                if(instr.arg1.u32 == REG_M_IN)
                    b = gpu->v_pos.right;
                Vec4 res;
                res.x = a.x - b.x;
                res.y = a.y - b.y;
                res.z = a.z - b.z;
                res.w = a.w - b.w;
                gpu->regs[instr.dest].right = res;
                print_mat4(&gpu->regs[instr.dest], "SUB");
                break;
            }
            if (instr.opType == OP_TYPE_VEC3)
            {
                Vec3Raw a = gpu->regs[instr.arg0.u32].vec3;
                Vec3Raw b = gpu->regs[instr.arg1.u32].vec3;
                Vec3Raw res;
                res.x = a.x - b.x;
                res.y = a.y - b.y;
                res.z = a.z - b.z;
                gpu->regs[instr.dest].vec3 = res;
                print_mat4(&gpu->regs[instr.dest], "SUB");
                break;
            }
            
            ARITHMETIC_OP(-=);
            break;
        }
        case INSTR_DIV:
        {
            if(instr.opType == OP_TYPE_VEC4)
            {
                Vec4 a = gpu->regs[instr.arg0.u32].right;
                Vec4 b = gpu->regs[instr.arg1.u32].right;
                if(instr.arg1.u32 == REG_M_IN)
                    b = gpu->v_pos.right;
                Vec4 res;
                res.x = a.x / b.x;
                res.y = a.y / b.y;
                res.z = a.z / b.z;
                res.w = a.w / b.w;
                gpu->regs[instr.dest].right = res;
                print_mat4(&gpu->regs[instr.dest], "DIV");
                break;
            }
            if (instr.opType == OP_TYPE_VEC3)
            {
                Vec3Raw a = gpu->regs[instr.arg0.u32].vec3;
                Vec3Raw b = gpu->regs[instr.arg1.u32].vec3;
                Vec3Raw res;
                res.x = a.x / b.x;
                res.y = a.y / b.y;
                res.z = a.z / b.z;
                gpu->regs[instr.dest].vec3 = res;
                print_mat4(&gpu->regs[instr.dest], "DIV");
                break;
            }
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
            InstrArg x = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            InstrArg y = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
            InstrArg z = get_arg_scalar_value(gpu, instr.arg2Type, instr.arg2);
            gpu->regs[instr.dest] = mat4_translate(x.u32, y.u32, z.u32);
            print_mat4(&gpu->regs[instr.dest], "TRANS");
            break;
        }
        case INSTR_LDU:
        {
            uint32_t offset = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0).u32;
            void *ptr = gpu->vram_ptr + gpu->uinform_config.addr + offset;
            switch (instr.opType)
            {
            case OP_TYPE_MATRIX:
                gpu->regs[instr.dest] = *(Mat4 *)ptr;
                print_mat4(& gpu->regs[instr.dest] , "LDUM");
                break;
            case OP_TYPE_VEC4:
                gpu->regs[instr.dest].right = (*(Mat4 *)ptr).right;
                break;
            case OP_TYPE_U32:
                gpu->pRegs[instr.dest].u32 = *(uint32_t *)ptr;
                break;
            case OP_TYPE_F32:
                gpu->pRegs[instr.dest].f32 = *(float *)ptr;
            break;

            default:
                break;
            } 
            break;
        }
        case INSTR_CLAMP:
        {
            InstrArg val = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            InstrArg min = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
            InstrArg max = get_arg_scalar_value(gpu, instr.arg2Type, instr.arg2);

            if(instr.opType == OP_TYPE_F32)
            {
                if(val.f32 < min.f32) gpu->pRegs[instr.dest].f32 = min.f32;
                else if(val.f32 > max.f32) gpu->pRegs[instr.dest].f32 = max.f32;
                else gpu->pRegs[instr.dest].f32 = val.f32;
                DEBUG_PRINT("CLAMP F32: %f -> %f\n", val.f32, gpu->pRegs[instr.dest].f32);
            }
            else 
            {
                if(val.u32 < min.u32) gpu->pRegs[instr.dest].u32 = min.u32;
                else if(val.u32 > max.u32) gpu->pRegs[instr.dest].u32 = max.u32;
                else gpu->pRegs[instr.dest].u32 = val.u32;
                DEBUG_PRINT("CLAMP U32: %u -> %u\n", val.u32, gpu->pRegs[instr.dest].u32);
            }
            break;
        }
        case INSTR_NEG:
        {
            InstrArg a = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);

            if(instr.opType == OP_TYPE_F32)
            {
                gpu->pRegs[instr.dest].f32 = -a.f32;
                DEBUG_PRINT("NEG F32: %f -> %f\n", a.f32, gpu->pRegs[instr.dest].f32);  
            }
            else 
            {
                gpu->pRegs[instr.dest].u32 = (uint32_t)(-((int32_t)a.u32));   
                DEBUG_PRINT("NEG U32: %u -> %u\n", a.u32, gpu->pRegs[instr.dest].u32);
            }
            break;
        }
        case INSTR_RECIP:
        {
            InstrArg a = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);

            if(instr.opType == OP_TYPE_F32)
            {
                gpu->pRegs[instr.dest].f32 = 1.0f / a.f32;
                DEBUG_PRINT("RECIP F32: %f -> %f\n", a.f32, gpu->pRegs[instr.dest].f32);
            }
            else 
                gpu->pRegs[instr.dest].u32 = (uint32_t)(1 / (float)(a.u32));   
            break;
        }
        case INSTR_RSQRT:
        {
            InstrArg a = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            gpu->pRegs[instr.dest].f32 = 1.0f / sqrtf(a.f32);
            DEBUG_PRINT("RSQRT: %f -> %f\n", a.f32, gpu->pRegs[instr.dest].f32);
            break;
        }
        case INSTR_MIN:
        {
            InstrArg a = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            InstrArg b = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);

            if(instr.opType == OP_TYPE_F32)
            {
                gpu->pRegs[instr.dest].f32 = fminf(a.f32, b.f32);
                DEBUG_PRINT("MIN F32: %f, %f -> %f\n", a.f32, b.f32, gpu->pRegs[instr.dest].f32);
            }
            else 
            {
                gpu->pRegs[instr.dest].u32 = (a.u32 < b.u32) ? a.u32 : b.u32;   
                DEBUG_PRINT("MIN U32: %u, %u -> %u\n", a.u32, b.u32, gpu->pRegs[instr.dest].u32);
            }
            break;
        }
        case INSTR_MAX:
        {
            InstrArg a = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            InstrArg b = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);

            if(instr.opType == OP_TYPE_F32)
            {
                gpu->pRegs[instr.dest].f32 = fmaxf(a.f32, b.f32);
                DEBUG_PRINT("MAX F32: %f, %f -> %f\n", a.f32, b.f32, gpu->pRegs[instr.dest].f32);
            }
            else 
            {
                gpu->pRegs[instr.dest].u32 = (a.u32 > b.u32) ? a.u32 : b.u32;  
                DEBUG_PRINT("MAX U32: %u, %u -> %u\n", a.u32, b.u32, gpu->pRegs[instr.dest].u32);
            }
            break;
        }
        case INSTR_FMA:
        {
            InstrArg a = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            InstrArg b = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
            InstrArg c = get_arg_scalar_value(gpu, instr.arg2Type, instr.arg2);

            if(instr.opType == OP_TYPE_F32)
                gpu->pRegs[instr.dest].f32 = a.f32 * b.f32 + c.f32;
            else 
                gpu->pRegs[instr.dest].u32 = (uint32_t)( (float)(a.u32 * b.u32) + (float)(c.u32) );      
            break;
            DEBUG_PRINT("FMA: %u * %u + %u -> %u\n", a.u32, b.u32, c.u32, gpu->pRegs[instr.dest].u32);
        }
        case INSTR_MAD:
        {
            InstrArg a = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            InstrArg b = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
            InstrArg c = get_arg_scalar_value(gpu, instr.arg2Type, instr.arg2);

            if(instr.opType == OP_TYPE_F32)
                gpu->pRegs[instr.dest].f32 = a.f32 * b.f32 + c.f32;
            else 
                gpu->pRegs[instr.dest].u32 = (uint32_t)( (float)(a.u32 * b.u32) + (float)(c.u32) );      
            break;
            DEBUG_PRINT("MAD: %u * %u + %u -> %u\n", a.u32, b.u32, c.u32, gpu->pRegs[instr.dest].u32);
        }
        case INSTR_SAT:
        {
            InstrArg val = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);

            if(instr.opType == OP_TYPE_F32)
            {
                if(val.f32 < 0.0f) gpu->pRegs[instr.dest].f32 = 0.0f;
                else if(val.f32 > 1.0f) gpu->pRegs[instr.dest].f32 = 1.0f;
                else gpu->pRegs[instr.dest].f32 = val.f32;
                DEBUG_PRINT("SAT F32: %f -> %f\n", val.f32, gpu->pRegs[instr.dest].f32);
            }
            else 
            {
                if((int)val.u32 < 0) gpu->pRegs[instr.dest].u32 = 0;
                else if(val.u32 > 255) gpu->pRegs[instr.dest].u32 = 255;
                else gpu->pRegs[instr.dest].u32 = val.u32;
                DEBUG_PRINT("SAT U32: %u -> %u\n", val.u32, gpu->pRegs[instr.dest].u32);
            }
            break;
        }
        case INSTR_ATAN:
        {
            InstrArg y = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            InstrArg x = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);

            gpu->pRegs[instr.dest].f32 = atan2f(y.f32, x.f32);
            DEBUG_PRINT("ATAN2: y=%f, x=%f -> %f\n", y.f32, x.f32, gpu->pRegs[instr.dest].f32);
            break;
        }
        case INSTR_TAN:
        {
            InstrArg a = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);

            gpu->pRegs[instr.dest].f32 = tanf(a.f32);
            DEBUG_PRINT("TAN: %f -> %f\n", a.f32, gpu->pRegs[instr.dest].f32);
            break;
        }
        case INSTR_PCMP:
        {
            InstrArg a = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            InstrArg b = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
            if(instr.opType == OP_TYPE_F32)
                gpu->pRegs[instr.dest].u32 = cmp_f32(a.f32, b.f32, instr.cFlag);
            else 
                gpu->pRegs[instr.dest].u32 = cmp_u32(a.u32, b.u32, instr.cFlag);   
            DEBUG_PRINT("PCMP result: %u\n", gpu->pRegs[instr.dest].u32);
            break;
        }
        case INSTR_VEC3:
        {
            InstrArg x = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            InstrArg y = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
            InstrArg z = get_arg_scalar_value(gpu, instr.arg2Type, instr.arg2);
            Vec3Raw vec = {x.f32, y.f32, z.f32};
            gpu->regs[instr.dest].vec3 = vec;
            print_mat4(&gpu->regs[instr.dest], "VEC3");
            break;
        }
    
        case INSTR_LEN:
        {
            Vec3Raw vec = gpu->regs[instr.arg0.u32].vec3;
            gpu->pRegs[instr.dest].f32 = sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
            DEBUG_PRINT("LEN: %f\n", gpu->pRegs[instr.dest].f32);
            break;
        }
        case INSTR_NORM:
        {
            Vec3Raw vec = gpu->regs[instr.arg0.u32].vec3;
            float len = sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
            if(len > 0.0f)
            {
                vec.x /= len;
                vec.y /= len;
                vec.z /= len;
            }
            gpu->regs[instr.dest].vec3 = vec;
            print_mat4(&gpu->regs[instr.dest], "NORM");
            break;
        }
        case INSTR_DOT:
        {
            Vec3Raw v1 = gpu->regs[instr.arg0.u32].vec3;
            Vec3Raw v2 = gpu->regs[instr.arg1.u32].vec3;
            gpu->pRegs[instr.dest].f32 = v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
            DEBUG_PRINT("DOT: %f\n", gpu->pRegs[instr.dest].f32);
            break;
        }
        case INSTR_CROSS:
        {
            Vec3Raw v1 = gpu->regs[instr.arg0.u32].vec3;
            Vec3Raw v2 = gpu->regs[instr.arg1.u32].vec3;
            Vec3Raw res;
            res.x = v1.y * v2.z - v1.z * v2.y;
            res.y = v1.z * v2.x - v1.x * v2.z;
            res.z = v1.x * v2.y - v1.y * v2.x;
            gpu->regs[instr.dest].vec3 = res;
            print_mat4(&gpu->regs[instr.dest], "CROSS");
            break;
        }
        case INSTR_SIGN:
        {
            InstrArg a = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);

            if(instr.opType == OP_TYPE_F32)
            {
                if(a.f32 > 0.0f) gpu->pRegs[instr.dest].f32 = 1.0f;
                else if(a.f32 < 0.0f) gpu->pRegs[instr.dest].f32 = -1.0f;
                else gpu->pRegs[instr.dest].f32 = 0.0f;
                DEBUG_PRINT("SIGN F32: %f -> %f\n", a.f32, gpu->pRegs[instr.dest].f32);
            }
            else 
            {
                int32_t val = (int32_t)a.u32;
                if(val > 0) gpu->pRegs[instr.dest].u32 = 1;
                else if(val < 0) gpu->pRegs[instr.dest].u32 = (uint32_t)(-1);
                else gpu->pRegs[instr.dest].u32 = 0;
                DEBUG_PRINT("SIGN U32: %d -> %d\n", val, (int32_t)gpu->pRegs[instr.dest].u32);
            }
            break;
        }
        case INSTR_JMP:
        {
            uint32_t target = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0).u32;
            program_address = program_begin + target;
            DEBUG_PRINT("JMP to %u\n", target);
            continue;
        }
        case INSTR_EXIT:
            end = 0;
            return;
        break;
        }
        program_address+=sizeof(Instr);
    }while(end);

}

float edge_func(Vec3 a, Vec3 b, Vec3 c) 
{
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

void draw_triangle(Vec4 v0, Vec4 v1, Vec4 v2, Col3 color, GpuState *gpu)
{
    uint32_t width = gpu->width;
    uint32_t height =  gpu->height;
    Vec3 s[3];
    Vec4 v[3] = {v0, v1, v2};

    for(int i = 0; i < 3; i++) 
    {
        float w = (v[i].w < 0.1f) ? 0.1f : v[i].w;
        float inv_w = 1.0f / w;
        
        float ndc_x = v[i].x * inv_w;
        float ndc_y = v[i].y * inv_w;

        s[i].x = (ndc_x + 1.0f) * 0.5f * width;
        s[i].y = (1.0f - (ndc_y + 1.0f) * 0.5f) * height;
        s[i].z = inv_w; 
    }

   
    float area = edge_func(s[0], s[1], s[2]);
    if (area >= 0) return; 

    int min_x = fmax(0, fmin(s[0].x, fmin(s[1].x, s[2].x)));
    int max_x = fmin(width-1, fmax(s[0].x, fmax(s[1].x, s[2].x)));
    int min_y = fmax(0, fmin(s[0].y, fmin(s[1].y, s[2].y)));
    int max_y = fmin(height-1, fmax(s[0].y, fmax(s[1].y, s[2].y)));

   
    for (int y = min_y; y <= max_y; y++) 
    {
        for (int x = min_x; x <= max_x; x++)
        {
            Vec3 p = {x + 0.5f, y + 0.5f, 0};
            float w0 = edge_func(s[1], s[2], p) / area;
            float w1 = edge_func(s[2], s[0], p) / area;
            float w2 = edge_func(s[0], s[1], p) / area;

            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                //float z_inv = w0 * s[0].z + w1 * s[1].z + w2 * s[2].z;
                float z = w0 * (1.0f/s[0].z) + w1 * (1.0f/s[1].z) + w2 * (1.0f/s[2].z);
                
                if (z < Z_BUFFER(gpu)[y * width + x]) {
                    Z_BUFFER(gpu)[y * width + x] = z;
                    uint8_t r = (uint8_t)(w0 * GET_R(color.a_col) + w1 * GET_R(color.b_col) + w2 * GET_R(color.c_col));
                    uint8_t g = (uint8_t)(w0 * GET_G(color.a_col) + w1 * GET_G(color.b_col) + w2 * GET_G(color.c_col));
                    uint8_t b = (uint8_t)(w0 * GET_B(color.a_col) + w1 * GET_B(color.b_col) + w2 * GET_B(color.c_col));
                    put_pixel(gpu, x,y, RGB_TO_UINT(r,g,b));
                }
            }
        }
    }
}
void gpu_render_triangles(void *opaque)
{
     GpuState *gpu = opaque;
    if(gpu->gpu_mode == GPU_MODE_IDLE)//REG_EXEC_VERTEX_SHADER(gpu) == 1
    {
        return;
    }
    gpu->gpu_mode = GPU_MODE_IDLE;
    uint32_t triangle_size =  gpu->edge_config.size;

        
    Vec3 *vertices = VERTEX_TABLE(gpu);
    Triangle *indices = TRIANGLES_TABLE(gpu);
    for (int i = 0; i < triangle_size; i++) 
    {
            Vec4 v[3];
            v[0].x = vertices[indices[i].a].x;
            v[0].y = vertices[indices[i].a].y;
            v[0].z = vertices[indices[i].a].z;
            v[0].w = 1.0f;

            v[1].x = vertices[indices[i].b].x;
            v[1].y = vertices[indices[i].b].y;
            v[1].z = vertices[indices[i].b].z;
            v[1].w = 1.0f;

            v[2].x = vertices[indices[i].c].x;
            v[2].y = vertices[indices[i].c].y;
            v[2].z = vertices[indices[i].c].z;
            v[2].w = 1.0f;

            Col3 color = {vertices[indices[i].a].rgba, vertices[indices[i].b].rgba, vertices[indices[i].c].rgba};

            for(int j=0; j<3; j++) 
            {
                gpu->v_pos.right = v[j];
                exec_shader(gpu, gpu->vs_code_addr);     
                v[j] = gpu->v_out.right;
            }
            draw_triangle(v[0], v[1], v[2], color, gpu);
    }
    

    gpu->gpu_mode = GPU_MODE_3D;
}

void gpu_render_wireframe(void *opaque)
{
    GpuState *gpu = opaque;
    if(gpu->gpu_mode == GPU_MODE_IDLE)//REG_EXEC_VERTEX_SHADER(gpu) == 1
    {
        DEBUG_PRINT("[Render Frame] GPU IS IDLE\n");
        return;
    }
    gpu->gpu_mode = GPU_MODE_IDLE;

    debug_dump_edges(opaque);
    debug_dump_vertices(opaque);
    debug_dump_ubo(opaque);

    uint32_t width = gpu->width;
    uint32_t height =  gpu->height;
    uint32_t vertex_size = gpu->vbo_config.size;
    uint32_t edges_size =  gpu->edge_config.size;
    DEBUG_PRINT("[GPU State] Width: %u, Height: %u, Vertex Size: %u, Edge Size: %u\n", 
       gpu->width, 
       gpu->height, 
       gpu->vbo_config.size, 
       gpu->edge_config.size);
        
    Vec3 *vertices = VERTEX_TABLE(gpu);
    Edge *edges = EDGES_TABLE(gpu);
    
    uint32_t *px = malloc(sizeof(uint32_t)* vertex_size);
    uint32_t *py = malloc(sizeof(uint32_t)* vertex_size);
    for(uint32_t i=0;i<vertex_size;i++) 
    {
        Vec4 v = {vertices[i].x, vertices[i].y, vertices[i].z, 1.0f};
        gpu->v_pos.right = v;
        //exec vertex shader
        exec_shader(gpu, gpu->vs_code_addr);     
        Vec4 tv = gpu->v_out.right;
        float ndc_x = tv.x / tv.w;
        float ndc_y = tv.y / tv.w;
        px[i] = (int)((ndc_x*0.5f + 0.5f) * width);
        py[i] = (int)((-ndc_y*0.5f + 0.5f) * height);
    }
    DEBUG_PRINT("[Render Frame] Drawing lines\n");

    DEBUG_PRINT("[GPU State] px: %p, py: %p, vert: %p,\n", 
       (void*)px, 
       (void*)py, 
       (void*)vertices
        );
        
    for(uint32_t i=0;i<edges_size;i++)
    {
        Edge e = edges[i];
        DEBUG_PRINT("[GPU State] px[e.a]: %u, py[e.a]: %u, px[e.b]: %u, py[e.b]: %u, e.a: %u, e.b %u\n", 
            px[e.a], py[e.a], px[e.b], py[e.b], e.a, e.b);
        draw_line(gpu, px[e.a], py[e.a], px[e.b], py[e.b],  vertices[e.a].rgba, vertices[e.b].rgba);
    }
    
    free(px);
    free(py);
    gpu->gpu_mode = GPU_MODE_3D;
}
