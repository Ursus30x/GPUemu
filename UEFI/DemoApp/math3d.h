#ifndef MATH3D_H
#define MATH3D_H

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>

#define PI 3.14159265359f

// --- Definitions ---
// Matches GPU layout. defined here, DO NOT redefine in app.c
typedef struct { double x, y, z; UINT32 color; } Vec3; 
typedef struct { UINT32 v1, v2; } Edge; 

typedef struct {
    union {
        float m[4][4];
        float elements[16];
    };
} Mat4;

// --- Math Functions (Pointer Based to avoid SSE Returns) ---

// Taylor series for Sin(x)
static void Sin(float x, float *out) {
    // Normalize to -PI to PI
    while (x > PI) x -= 2 * PI;
    while (x < -PI) x += 2 * PI;
    
    float res = x;
    float term = x;
    for (int i = 1; i <= 6; i++) {
        term *= -1 * x * x / ((2 * i) * (2 * i + 1));
        res += term;
    }
    *out = res;
}

static void Cos(float x, float *out) {
    Sin(x + PI / 2.0f, out);
}

static void Tan(float x, float *out) {
    float s, c;
    Sin(x, &s);
    Cos(x, &c);
    if (ABS(c) < 1e-5) *out = 0;
    else *out = s / c;
}

// --- Matrix Functions (Pointer Based) ---

static void Mat4_Identity(Mat4 *res) {
    SetMem(res, sizeof(Mat4), 0);
    res->m[0][0] = 1.0f; res->m[1][1] = 1.0f; 
    res->m[2][2] = 1.0f; res->m[3][3] = 1.0f;
}

static void Mat4_Mul(Mat4 *a, Mat4 *b, Mat4 *res) {
    Mat4 temp = {0}; // Use temp to allow A*B -> A
    for(int r=0; r<4; r++) {
        for(int c=0; c<4; c++) {
            for(int k=0; k<4; k++) {
                temp.m[r][c] += a->m[r][k] * b->m[k][c];
            }
        }
    }
    CopyMem(res, &temp, sizeof(Mat4));
}

static void Mat4_RotateX(float angle, Mat4 *res) {
    Mat4_Identity(res);
    float c, s;
    Cos(angle, &c);
    Sin(angle, &s);
    res->m[1][1] = c;  res->m[1][2] = -s;
    res->m[2][1] = s;  res->m[2][2] = c;
}

static void Mat4_RotateY(float angle, Mat4 *res) {
    Mat4_Identity(res);
    float c, s;
    Cos(angle, &c);
    Sin(angle, &s);
    res->m[0][0] = c;   res->m[0][2] = s;
    res->m[2][0] = -s;  res->m[2][2] = c;
}

static void Mat4_Translate(float x, float y, float z, Mat4 *res) {
    Mat4_Identity(res);
    res->m[0][3] = x; res->m[1][3] = y; res->m[2][3] = z;
}

static void Mat4_Scale(float s, Mat4 *res) {
    Mat4_Identity(res);
    res->m[0][0] = s; res->m[1][1] = s; res->m[2][2] = s;
}

static void Mat4_Perspective(float fov, float aspect, float nearPlane, float farPlane, Mat4 *res) {
    SetMem(res, sizeof(Mat4), 0);
    float t;
    Tan(fov * 0.5f, &t);
    float f = 1.0f / t;
    
    res->m[0][0] = f / aspect;
    res->m[1][1] = f;
    res->m[2][2] = farPlane / (farPlane - nearPlane);
    res->m[2][3] = (-farPlane * nearPlane) / (farPlane - nearPlane);
    res->m[3][2] = 1.0f;
}

#endif