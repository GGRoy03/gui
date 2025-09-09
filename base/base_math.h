#pragma once

// [Core Types]

typedef struct vec2_f32
{
	f32 X, Y;
} vec2_f32;

typedef struct vec4_f32
{
	f32 X, Y, Z, W;
} vec4_f32;

typedef struct vec2_i32
{
	i32 X, Y;
} vec2_i32;

typedef struct matrix_3x3
{
	f32 c0r0, c0r1, c0r2;
	f32 c1r0, c1r1, c1r2;
	f32 c2r0, c2r1, c2r2;
} matrix_3x3;

typedef struct matrix_4x4
{
	f32 c0r0, c0r1, c0r2, c0r3;
	f32 c1r0, c1r1, c1r2, c1r3;
	f32 c2r0, c2r1, c2r2, c2r3;
	f32 c3r0, c3r1, c3r2, c3r3;
} matrix_4x4;

// [API]

// [Helpers]

#define ToVec2F32(X, Y)       (vec2_f32){(X), (Y)}
#define ToVec4F32(X, Y, Z, W) (vec4_f32){(X), (Y), (Z), (W)}

internal b32 Vec2I32IsEqual(vec2_i32 Vec1, vec2_i32 Vec2);
internal b32 Vec2F32IsEqual(vec2_f32 Vec1, vec2_f32 Vec2);