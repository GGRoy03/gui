#pragma once

#define internal      static
#define global        static
#define local_persist static

#define Kilobyte(n) (((cim_u64)(n)) << 10)
#define Megabyte(n) (((cim_u64)(n)) << 20)
#define Gygabyte(n) (((cim_u64)(n)) << 30)

#define AlignPow2(x,b) (((x) + (b) - 1)&(~((b) - 1)))

#define Min(A,B) (((A)<(B))?(A):(B))
#define Max(A,B) (((A)>(B))?(A):(B))
#define ClampTop(A,X) Min(A,X)
#define ClampBot(X,B) Max(X,B)

#include <stdint.h>

typedef int      cim_b32;

typedef float    cim_f32;
typedef double   cim_f64;

typedef uint8_t  cim_u8;
typedef uint16_t cim_u16;
typedef uint32_t cim_u32;
typedef uint64_t cim_u64;

typedef int       cim_i32;
typedef long long cim_i64;

typedef cim_u32  cim_bit_field;
