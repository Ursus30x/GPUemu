#include "renderer.h"
#include "math3d.h"
#include "math.h"
#include "debug_gpu.h"
#include <pthread.h>

#define PRINT_V4(v) DEBUG_PRINT("[%f, %f, %f, %f]\n", v.x, v.y, v.z, v.w);


void put_pixel(GpuState *gpu, int x, int y, uint32_t color)
{
    if(1) // TO DO TRIGGER FRAGMENT
    {
        gpu->pRegs[REG_PX].f32 = (float)x;
        gpu->pRegs[REG_PY].f32 = (float)y;
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
            InstrArg a = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0); \
            InstrArg b = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1); \
            if(instr.opType == OP_TYPE_U32) { \
                gpu->pRegs[instr.dest].u32 = a.u32 op b.u32; } \
            else { \
                gpu->pRegs[instr.dest].f32 = a.f32 op b.f32; }


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
                DEBUG_PRINT("MOV MATRIX: reg[%u] -> reg[%u]\n", instr.arg0.u32, instr.dest);
                break;
            }
            InstrArg a = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            gpu->pRegs[instr.dest] = a;
            DEBUG_PRINT("MOV: in=%u -> reg[%u]=%u\n", a.u32, instr.dest, gpu->pRegs[instr.dest].u32);
            break;
        }
        case INSTR_ROTX:
        {
            InstrArg rot = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            gpu->regs[instr.dest] = mat4_rotate_x(rot.f32);
            print_mat4(&gpu->regs[instr.dest], "ROTX");
            DEBUG_PRINT("ROTX: angle=%f\n", rot.f32);
            break;
        }
        case INSTR_ROTY:
        {
            InstrArg rot = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            gpu->regs[instr.dest] = mat4_rotate_y(rot.f32);
            print_mat4(&gpu->regs[instr.dest], "ROTY");
            DEBUG_PRINT("ROTY: angle=%f\n", rot.f32);
            break;
        }
        case INSTR_SIN:
        {
            InstrArg rot = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            gpu->pRegs[instr.dest].f32 = sinf(rot.f32);
            DEBUG_PRINT("SIN: angle=%f -> sin=%f\n", rot.f32, gpu->pRegs[instr.dest].f32);
            break;
        }
        case INSTR_COS:
        {
            InstrArg rot = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            gpu->pRegs[instr.dest].f32 = cosf(rot.f32);
            DEBUG_PRINT("COS: angle=%f -> cos=%f\n", rot.f32, gpu->pRegs[instr.dest].f32);
            break;
        }
        case INSTR_SQRT:
        {
            InstrArg rot = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            gpu->pRegs[instr.dest].f32 = sqrtf(rot.f32);
            DEBUG_PRINT("SQRT: %f -> %f\n", rot.f32, gpu->pRegs[instr.dest].f32);
            break;
        }
        case INSTR_FSAN:
        {
            float val = gpu->pRegs[instr.dest].f32;
            if(!isfinite(val))
                gpu->pRegs[instr.dest].f32 = 0.0f;
            DEBUG_PRINT("FSAN: %f -> %f\n", val, gpu->pRegs[instr.dest].f32);
            break;
        }
        case INSTR_IDENT:
        {
            gpu->regs[instr.dest] = mat4_identity();
            DEBUG_PRINT("IDENT: identity matrix created\n");
            break;
        }
        case INSTR_MVP:
        {
            gpu->v_out.right = gpu->regs[instr.dest].right;
            print_mat4(&gpu->regs[instr.dest], "V_OUT");
            DEBUG_PRINT("MVP: matrix applied to output\n");
            break;
        }
        case INSTR_MOD:
        {
            InstrArg a = gpu->pRegs[instr.arg0.u32];
            InstrArg b = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
            gpu->pRegs[instr.dest].u32 = a.u32 % b.u32;
            DEBUG_PRINT("MOD: %u %% %u -> %u\n", a.u32, b.u32, gpu->pRegs[instr.dest].u32);
            break;
        }

        case INSTR_COL:
        {
            Vec3Raw col = gpu->regs[instr.dest].vec3;
            gpu->pRegs[REG_PR].u32 = (uint32_t)(fmaxf(0.0f, fminf(1.0f, col.x)) * 255.0f);
            gpu->pRegs[REG_PG].u32 = (uint32_t)(fmaxf(0.0f, fminf(1.0f, col.y)) * 255.0f);
            gpu->pRegs[REG_PB].u32 = (uint32_t)(fmaxf(0.0f, fminf(1.0f, col.z)) * 255.0f);
            DEBUG_PRINT("COL VEC3: r=%u, g=%u, b=%u\n", gpu->pr, gpu->pg, gpu->pb);
            DEBUG_PRINT("COL VEC3: r=%f, g=%f, b=%f\n", col.x, col.y, col.z);

            break;
        }
        case INSTR_ABS:
        {
            InstrArg a = gpu->pRegs[instr.arg0.u32];

            if(instr.opType == OP_TYPE_F32)
                gpu->pRegs[instr.dest].f32 = fabsf(a.f32);
            else
                gpu->pRegs[instr.dest].u32 = abs(a.u32);

            DEBUG_PRINT("ABS: in=%u -> out=%u\n", a.u32, gpu->pRegs[instr.dest].u32);
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

            DEBUG_PRINT("LERP: a=%f, b=%f, t=%f -> out=%f\n", a.f32, b.f32, t.f32, gpu->pRegs[instr.dest].f32);

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
            DEBUG_PRINT("BLEND: a=%u, b=%u, w=%u -> out=%u\n", a.u32, b.u32, w.u32, gpu->pRegs[instr.dest].u32);
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
                DEBUG_PRINT("MUL MATRIX: reg[%u] * reg[%u] -> reg[%u]\n", instr.arg0.u32, instr.arg1.u32, instr.dest);
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
                DEBUG_PRINT("MUL VEC4: mat * vec -> out=[%f,%f,%f,%f]\n", res.x, res.y, res.z, res.w);
                break;
            }
            if (instr.opType == OP_TYPE_VEC3)
            {
                Vec3Raw a = gpu->regs[instr.arg0.u32].vec3;
                InstrArg b = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
                gpu->regs[instr.dest].vec3.x = a.x * b.f32;
                gpu->regs[instr.dest].vec3.y = a.y * b.f32;
                gpu->regs[instr.dest].vec3.z = a.z * b.f32;
                print_mat4(&gpu->regs[instr.dest], "MUL");
                DEBUG_PRINT("MUL VEC3: [%f,%f,%f] * %f -> out=[%f,%f,%f]\n", a.x, a.y, a.z, b.f32, gpu->regs[instr.dest].vec3.x, gpu->regs[instr.dest].vec3.y, gpu->regs[instr.dest].vec3.z);
                break;
            }
            DEBUG_VAR InstrArg a_DBG = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            DEBUG_VAR InstrArg b_DBG = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
            ARITHMETIC_OP(*);
            if(instr.opType == OP_TYPE_U32)
            {
                DEBUG_PRINT("MUL SCALAR: a=%u * b=%u -> dest=%u\n", a_DBG.u32, b_DBG.u32, gpu->pRegs[instr.dest].u32);
            }
            else
            {
                DEBUG_PRINT("MUL SCALAR: a=%f * b=%f -> dest=%f\n", a_DBG.f32, b_DBG.f32, gpu->pRegs[instr.dest].f32);
            }
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
                DEBUG_PRINT("ADD VEC4: [%f,%f,%f,%f] + [%f,%f,%f,%f] -> [%f,%f,%f,%f]\n", a.x, a.y, a.z, a.w, b.x, b.y, b.z, b.w, res.x, res.y, res.z, res.w);
                break;
            }
            if (instr.opType == OP_TYPE_VEC3)
            {
                Vec3Raw a = gpu->regs[instr.arg0.u32].vec3;
                InstrArg b = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
                Vec3Raw res;
                res.x = a.x + b.f32;
                res.y = a.y + b.f32;
                res.z = a.z + b.f32;
                gpu->regs[instr.dest].vec3 = res;
                print_mat4(&gpu->regs[instr.dest], "ADD");
                DEBUG_PRINT("ADD VEC3: [%f,%f,%f] + %f -> [%f,%f,%f]\n", a.x, a.y, a.z, b.f32, res.x, res.y, res.z);
                break;
            }

            DEBUG_VAR InstrArg a_DBG = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            DEBUG_VAR InstrArg b_DBG = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
            ARITHMETIC_OP(+);
            if(instr.opType == OP_TYPE_U32)
            {
                DEBUG_PRINT("ADD SCALAR: a=%u + b=%u -> dest=%u\n", a_DBG.u32, b_DBG.u32, gpu->pRegs[instr.dest].u32);
            }
            else
            {
                DEBUG_PRINT("ADD SCALAR: a=%f + b=%f -> dest=%f\n", a_DBG.f32, b_DBG.f32, gpu->pRegs[instr.dest].f32);
            }
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
                DEBUG_PRINT("SUB VEC4: [%f,%f,%f,%f] - [%f,%f,%f,%f] -> [%f,%f,%f,%f]\n", a.x, a.y, a.z, a.w, b.x, b.y, b.z, b.w, res.x, res.y, res.z, res.w);
                break;
            }
            if (instr.opType == OP_TYPE_VEC3)
            {
                Vec3Raw a = gpu->regs[instr.arg0.u32].vec3;
                InstrArg b = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
                Vec3Raw res;
                res.x = a.x - b.f32;
                res.y = a.y - b.f32;
                res.z = a.z - b.f32;
                gpu->regs[instr.dest].vec3 = res;
                print_mat4(&gpu->regs[instr.dest], "SUB");
                DEBUG_PRINT("SUB VEC3: [%f,%f,%f] - %f -> [%f,%f,%f]\n", a.x, a.y, a.z, b.f32, res.x, res.y, res.z);
                break;
            }

            DEBUG_VAR InstrArg a_DBG = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            DEBUG_VAR InstrArg b_DBG = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
            ARITHMETIC_OP(-);
            if(instr.opType == OP_TYPE_U32)
            {
                DEBUG_PRINT("SUB SCALAR: a=%u - b=%u -> dest=%u\n", a_DBG.u32, b_DBG.u32, gpu->pRegs[instr.dest].u32);
            }
            else
            {
                DEBUG_PRINT("SUB SCALAR: a=%f - b=%f -> dest=%f\n", a_DBG.f32, b_DBG.f32, gpu->pRegs[instr.dest].f32);
            }
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
                DEBUG_PRINT("DIV VEC4: [%f,%f,%f,%f] / [%f,%f,%f,%f] -> [%f,%f,%f,%f]\n", a.x, a.y, a.z, a.w, b.x, b.y, b.z, b.w, res.x, res.y, res.z, res.w);
                break;
            }
            if (instr.opType == OP_TYPE_VEC3)
            {
                Vec3Raw a = gpu->regs[instr.arg0.u32].vec3;
                InstrArg b = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
                Vec3Raw res;
                res.x = a.x / b.f32;
                res.y = a.y / b.f32;
                res.z = a.z / b.f32;
                gpu->regs[instr.dest].vec3 = res;
                print_mat4(&gpu->regs[instr.dest], "DIV");
                DEBUG_PRINT("DIV VEC3: [%f,%f,%f] / %f -> [%f,%f,%f]\n", a.x, a.y, a.z, b.f32, res.x, res.y, res.z);
                break;
            }
            DEBUG_VAR InstrArg a_DBG = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            DEBUG_VAR InstrArg b_DBG = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
            ARITHMETIC_OP(/);
            if(instr.opType == OP_TYPE_U32)
            {
                DEBUG_PRINT("DIV SCALAR: a=%u / b=%u -> dest=%u\n", a_DBG.u32, b_DBG.u32, gpu->pRegs[instr.dest].u32);
            }
            else
            {
                DEBUG_PRINT("DIV SCALAR: a=%f / b=%f -> dest=%f\n", a_DBG.f32, b_DBG.f32, gpu->pRegs[instr.dest].f32);  
            }
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
            DEBUG_PRINT("CMP: in_a=%u, in_b=%u, flag=%u -> cFlag=%u\n", a.u32, b.u32, instr.cFlag, gpu->cFlag);
            break;
        }
        case INSTR_CAST:
        {
            if(instr.opType == OP_TYPE_F32)
                gpu->pRegs[instr.arg0.u32].f32 = (float) (gpu->pRegs[instr.arg0.u32].u32);
            else 
                gpu->pRegs[instr.arg0.u32].u32 = (int)(gpu->pRegs[instr.arg0.u32].f32);   
            DEBUG_PRINT("CAST: reg[%u] -> reg[%u]\n", instr.arg0.u32, instr.arg0.u32);
            break;
        }
        case INSTR_TRANS:
        {
            InstrArg x = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
            InstrArg y = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
            InstrArg z = get_arg_scalar_value(gpu, instr.arg2Type, instr.arg2);
            gpu->regs[instr.dest] = mat4_translate(x.u32, y.u32, z.u32);
            print_mat4(&gpu->regs[instr.dest], "TRANS");
            DEBUG_PRINT("TRANS: x=%u, y=%u, z=%u\n", x.u32, y.u32, z.u32);
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
                DEBUG_PRINT("LDU MATRIX: offset=%u\n", offset);
                break;
            case OP_TYPE_VEC4:
                gpu->regs[instr.dest].right = (*(Mat4 *)ptr).right;
                DEBUG_PRINT("LDU VEC4: offset=%u\n", offset);
                break;
            case OP_TYPE_U32:
                gpu->pRegs[instr.dest].u32 = *(uint32_t *)ptr;
                DEBUG_PRINT("LDU U32: offset=%u, value=%u\n", offset, gpu->pRegs[instr.dest].u32);
                break;
            case OP_TYPE_F32:
                gpu->pRegs[instr.dest].f32 =  *(float *)ptr;
                DEBUG_PRINT("LDU F32: offset=%u, value=%f\n", offset, gpu->pRegs[instr.dest].f32);
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
                DEBUG_PRINT("RECIP F32: in=%f -> out=%f\n", a.f32, gpu->pRegs[instr.dest].f32);
            }
            else
            {
                gpu->pRegs[instr.dest].u32 = (uint32_t)(1 / (float)(a.u32));
                DEBUG_PRINT("RECIP U32: in=%u -> out=%u\n", a.u32, gpu->pRegs[instr.dest].u32);
            }
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
            DEBUG_PRINT("FMA: %u * %u + %u -> %u\n", a.u32, b.u32, c.u32, gpu->pRegs[instr.dest].u32);

            break;
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
           // DEBUG_PRINT("MAD: %u * %u + %u -> %u\n", a.u32, b.u32, c.u32, gpu->pRegs[instr.dest].u32);
            break;
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
        case INSTR_EXP:
        {
            InstrArg a = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);

            gpu->pRegs[instr.dest].f32 = expf(a.f32);
            DEBUG_PRINT("EXP: %f -> %f\n", a.f32, gpu->pRegs[instr.dest].f32);
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
            DEBUG_PRINT("PCMP: in_a=%u, in_b=%u, flag=%u -> out=%u\n", a.u32, b.u32, instr.cFlag, gpu->pRegs[instr.dest].u32);
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
            DEBUG_PRINT("VEC3: x=%f, y=%f, z=%f\n", x.f32, y.f32, z.f32);
            break;
        }

        case INSTR_LEN:
        {
            if(instr.opType == OP_TYPE_F32)
            {
                InstrArg a = get_arg_scalar_value(gpu, instr.arg0Type, instr.arg0);
                InstrArg b = get_arg_scalar_value(gpu, instr.arg1Type, instr.arg1);
                gpu->pRegs[instr.dest].f32 = sqrtf(a.f32 * a.f32 + b.f32 * b.f32);
                DEBUG_PRINT("LEN2: x=%f, y=%f -> length=%f\n", a.f32, b.f32, gpu->pRegs[instr.dest].f32);
                break;
            }
            Vec3Raw vec = gpu->regs[instr.arg0.u32].vec3;
            gpu->pRegs[instr.dest].f32 = sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
            DEBUG_PRINT("LEN: vec=(%f,%f,%f) -> length=%f\n", vec.x, vec.y, vec.z, gpu->pRegs[instr.dest].f32);
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
            DEBUG_PRINT("NORM: vec=(%f,%f,%f) -> normalized\n", vec.x, vec.y, vec.z);
            break;
        }
        case INSTR_DOT:
        {
            Vec3Raw v1 = gpu->regs[instr.arg0.u32].vec3;
            Vec3Raw v2 = gpu->regs[instr.arg1.u32].vec3;
            gpu->pRegs[instr.dest].f32 = v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
            DEBUG_PRINT("DOT: v1=(%f,%f,%f) dot v2=(%f,%f,%f) -> %f\n", v1.x, v1.y, v1.z, v2.x, v2.y, v2.z, gpu->pRegs[instr.dest].f32);
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
            DEBUG_PRINT("CROSS: res=(%f,%f,%f)\n", res.x, res.y, res.z);
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
            DEBUG_PRINT("EXIT: shader execution complete\n");
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

static void draw_triangle_band(Vec4 v0, Vec4 v1, Vec4 v2, Col3 color, GpuState *gpu, int band_min_y, int band_max_y)
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

    int min_x = fmax(0, fmin(s[0].x, fmin(s[1].x, s[2].x)));
    int max_x = fmin(width-1, fmax(s[0].x, fmax(s[1].x, s[2].x)));
    
    // Intersect the triangle bound's vertical footprint with the thread's assigned scanline band
    int min_y = fmax(band_min_y, fmin(s[0].y, fmin(s[1].y, s[2].y)));
    int max_y = fmin(band_max_y, fmax(s[0].y, fmax(s[1].y, s[2].y)));

    if (min_y > max_y || min_x > max_x) return;

    for (int y = min_y; y <= max_y; y++)
    {
        for (int x = min_x; x <= max_x; x++)
        {
            Vec3 p = {x + 0.5f, y + 0.5f, 0};
            float w0 = edge_func(s[1], s[2], p) / area;
            float w1 = edge_func(s[2], s[0], p) / area;
            float w2 = edge_func(s[0], s[1], p) / area;

            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                float z = w0 * (1.0f/s[0].z) + w1 * (1.0f/s[1].z) + w2 * (1.0f/s[2].z);
                int idx = y * width + x;

                if (z < Z_BUFFER(gpu)[idx]) 
                {
                    Z_BUFFER(gpu)[idx] = z;
                    uint8_t r = (uint8_t)(w0 * GET_R(color.a_col) + w1 * GET_R(color.b_col) + w2 * GET_R(color.c_col));
                    uint8_t g = (uint8_t)(w0 * GET_G(color.a_col) + w1 * GET_G(color.b_col) + w2 * GET_G(color.c_col));
                    uint8_t b = (uint8_t)(w0 * GET_B(color.a_col) + w1 * GET_B(color.b_col) + w2 * GET_B(color.c_col));
                    put_pixel(gpu, x, y, RGB_TO_UINT(r,g,b));
                }
            }
        }
    }
}

void draw_triangle(Vec4 v0, Vec4 v1, Vec4 v2, Col3 color, GpuState *gpu)
{
    draw_triangle_band(v0, v1, v2, color, gpu, 0, gpu->height - 1);
}


// pass 1: Parallel Vertex Shader execution across all VBO elements
static void* worker_transform_vertices(void* opaque) {
    RenderThreadArgs *args = (RenderThreadArgs*)opaque;
    GpuState local_gpu = *(args->orig_gpu); 
    GpuState *gpu = &local_gpu;

    Vec3 *vertices = VERTEX_TABLE(gpu);
    TransformedVertex *out_vertices = args->transformed_vertices;

    for (uint32_t i = args->start_idx; i < args->end_idx; i++)
    {
        Vec4 v = {vertices[i].x, vertices[i].y, vertices[i].z, 1.0f};
        gpu->v_pos.right = v;

        uint32_t color = vertices[i].rgba;
        gpu->pRegs[REG_PR].u32 = GET_R(color);
        gpu->pRegs[REG_PG].u32 = GET_G(color);
        gpu->pRegs[REG_PB].u32 = GET_B(color);

        exec_shader(gpu, gpu->vs_code_addr);

        out_vertices[i].pos = gpu->v_out.right;
        out_vertices[i].color = RGB_TO_UINT(
            (uint8_t)gpu->pRegs[REG_PR].u32,
            (uint8_t)gpu->pRegs[REG_PG].u32,
            (uint8_t)gpu->pRegs[REG_PB].u32);
    }
    return NULL;
}

// pass 2: Parallel scanline-band rasterization using the cached pre-transformed geometry
static void* worker_rasterize_bands(void* opaque) 
{
    RenderThreadArgs *args = (RenderThreadArgs*)opaque;
    GpuState local_gpu = *(args->orig_gpu);
    GpuState *gpu = &local_gpu;

    TransformedVertex *vertices = args->transformed_vertices;
    Triangle *indices = TRIANGLES_TABLE(gpu);
    uint32_t triangle_size = args->triangle_size;

    int band_min_y = args->start_y;
    int band_max_y = args->end_y - 1;

    for (uint32_t i = 0; i < triangle_size; i++) 
    {
        Vec4 v0 = vertices[indices[i].a].pos;
        Vec4 v1 = vertices[indices[i].b].pos;
        Vec4 v2 = vertices[indices[i].c].pos;

        Col3 color = {
            .a_col = vertices[indices[i].a].color,
            .b_col = vertices[indices[i].b].color,
            .c_col = vertices[indices[i].c].color,
        };

        draw_triangle_band(v0, v1, v2, color, gpu, band_min_y, band_max_y);
    }
    return NULL;
}

static void* worker_wireframe_vertices(void* opaque) {
    RenderThreadArgs *args = (RenderThreadArgs*)opaque;
    GpuState local_gpu = *(args->orig_gpu);
    GpuState *gpu = &local_gpu;
    
    uint32_t width = gpu->width;
    uint32_t height = gpu->height;
    Vec3 *vertices = VERTEX_TABLE(gpu);

    for(uint32_t i = args->start_idx; i < args->end_idx; i++)
    {
        Vec4 v = {vertices[i].x, vertices[i].y, vertices[i].z, 1.0f};
        gpu->v_pos.right = v;

        uint32_t color = vertices[i].rgba;
        uint8_t r  = GET_R(color);
        uint8_t g  = GET_G(color);
        uint8_t b  = GET_B(color);
        gpu->pRegs[REG_PR].u32 = r;
        gpu->pRegs[REG_PG].u32 = g;
        gpu->pRegs[REG_PB].u32 = b;
        
        exec_shader(gpu, gpu->vs_code_addr);

        vertices[i].rgba = RGB_TO_UINT(
            (uint8_t)gpu->pRegs[REG_PR].u32,
            (uint8_t)gpu->pRegs[REG_PG].u32,
            (uint8_t)gpu->pRegs[REG_PB].u32);

        Vec4 tv = gpu->v_out.right;
        float ndc_x = tv.x / tv.w;
        float ndc_y = tv.y / tv.w;
        args->px[i] = (int)((ndc_x*0.5f + 0.5f) * width);
        args->py[i] = (int)((-ndc_y*0.5f + 0.5f) * height);
    }
    return NULL;
}

static void* worker_wireframe_edges(void* opaque) {
    RenderThreadArgs *args = (RenderThreadArgs*)opaque;
    GpuState local_gpu = *(args->orig_gpu);
    GpuState *gpu = &local_gpu;
    
    Edge *edges = EDGES_TABLE(gpu);
    Vec3 *vertices = VERTEX_TABLE(gpu);

    for(uint32_t i = args->start_idx; i < args->end_idx; i++)
    {
        Edge e = edges[i];
        DEBUG_PRINT("[GPU State] px[e.a]: %u, py[e.a]: %u, px[e.b]: %u, py[e.b]: %u, e.a: %u, e.b %u\n",
            args->px[e.a], args->py[e.a], args->px[e.b], args->py[e.b], e.a, e.b);
        draw_line(gpu, args->px[e.a], args->py[e.a], args->px[e.b], args->py[e.b], vertices[e.a].rgba, vertices[e.b].rgba);
    }
    return NULL;
}


void gpu_render_triangles(void *opaque)
{
    GpuState *gpu = opaque;
    if(gpu->gpu_mode == GPU_MODE_IDLE)
    {
        return;
    }
    gpu->gpu_mode = GPU_MODE_IDLE;
    
    uint32_t vertex_size = gpu->vbo_config.size;
    uint32_t triangle_size = gpu->edge_config.size; 

    // Allocate geometry cache for vertex pre-shading pass
    TransformedVertex *transformed_vertices = malloc(sizeof(TransformedVertex) * vertex_size);
    if (!transformed_vertices)
    {
        gpu->gpu_mode = GPU_MODE_3D;
        return;
    }

    pthread_t threads[NUM_RENDER_THREADS];
    RenderThreadArgs args[NUM_RENDER_THREADS];

    // pass 1: Run vertex transformations in parallel
    uint32_t chunk_v = vertex_size / NUM_RENDER_THREADS;
    for (int i = 0; i < NUM_RENDER_THREADS; i++) 
    {
        args[i].orig_gpu = gpu;
        args[i].transformed_vertices = transformed_vertices;
        args[i].start_idx = i * chunk_v;
        args[i].end_idx = (i == NUM_RENDER_THREADS - 1) ? vertex_size : (i + 1) * chunk_v;
        
        pthread_create(&threads[i], NULL, worker_transform_vertices, &args[i]);
    }
    for (int i = 0; i < NUM_RENDER_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // pass 2: Rasterize horizontal screen scanline bands in parallel
    uint32_t height = gpu->height;
    uint32_t chunk_y = height / NUM_RENDER_THREADS;
    for (int i = 0; i < NUM_RENDER_THREADS; i++) 
    {
        args[i].orig_gpu = gpu;
        args[i].transformed_vertices = transformed_vertices;
        args[i].triangle_size = triangle_size;
        args[i].start_y = i * chunk_y;
        args[i].end_y = (i == NUM_RENDER_THREADS - 1) ? height : (i + 1) * chunk_y;
        
        pthread_create(&threads[i], NULL, worker_rasterize_bands, &args[i]);
    }
    for (int i = 0; i < NUM_RENDER_THREADS; i++) 
    {
        pthread_join(threads[i], NULL);
    }

    free(transformed_vertices);
    gpu->gpu_mode = GPU_MODE_3D;
}

void gpu_render_wireframe(void *opaque)
{
    GpuState *gpu = opaque;
    if(gpu->gpu_mode == GPU_MODE_IDLE)
    {
        DEBUG_PRINT("[Render Frame] GPU IS IDLE\n");
        return;
    }
    gpu->gpu_mode = GPU_MODE_IDLE;

    debug_dump_edges(opaque);
    debug_dump_vertices(opaque);
    debug_dump_ubo(opaque);

    uint32_t vertex_size = gpu->vbo_config.size;
    uint32_t edges_size =  gpu->edge_config.size;
    
    DEBUG_PRINT("[GPU State] Width: %u, Height: %u, Vertex Size: %u, Edge Size: %u\n",
       gpu->width, gpu->height, gpu->vbo_config.size, gpu->edge_config.size);

    uint32_t *px = malloc(sizeof(uint32_t) * vertex_size);
    uint32_t *py = malloc(sizeof(uint32_t) * vertex_size);

    pthread_t threads[NUM_RENDER_THREADS];
    RenderThreadArgs args[NUM_RENDER_THREADS];

    // pass 1: Run vertex transformations in parallel
    uint32_t chunk_v = vertex_size / NUM_RENDER_THREADS;
    for (int i = 0; i < NUM_RENDER_THREADS; i++) 
    {
        args[i].orig_gpu = gpu;
        args[i].px = px;
        args[i].py = py;
        args[i].start_idx = i * chunk_v;
        args[i].end_idx = (i == NUM_RENDER_THREADS - 1) ? vertex_size : (i + 1) * chunk_v;
        
        pthread_create(&threads[i], NULL, worker_wireframe_vertices, &args[i]);
    }
    for (int i = 0; i < NUM_RENDER_THREADS; i++) 
    {
        pthread_join(threads[i], NULL);
    }

    DEBUG_PRINT("[Render Frame] Drawing lines\n");

    // pass 2: Rasterize edges chunks in parallel
    uint32_t chunk_e = edges_size / NUM_RENDER_THREADS;
    for (int i = 0; i < NUM_RENDER_THREADS; i++)
    {
        args[i].start_idx = i * chunk_e;
        args[i].end_idx = (i == NUM_RENDER_THREADS - 1) ? edges_size : (i + 1) * chunk_e;
        
        pthread_create(&threads[i], NULL, worker_wireframe_edges, &args[i]);
    }
    for (int i = 0; i < NUM_RENDER_THREADS; i++)
    {
        pthread_join(threads[i], NULL);
    }

    free(px);
    free(py);
    gpu->gpu_mode = GPU_MODE_3D;
}