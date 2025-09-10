// [API IMPLEMENTATION]

// [Constructors]

internal vec2_f32
Vec2F32(f32 X, f32 Y)
{
	vec2_f32 Result;
	Result.X = X;
	Result.Y = Y;

	return Result;
}

internal vec4_f32
Vec4F32(f32 X, f32 Y, f32 Z, f32 W)
{
	vec4_f32 Result;
	Result.X = X;
	Result.Y = Y;
	Result.Z = Z;
	Result.W = W;

	return Result;
}

// [Helpers]

internal b32
Vec2I32IsEqual(vec2_i32 Vec1, vec2_i32 Vec2)
{
	b32 Result = (Vec1.X == Vec2.X) && (Vec1.Y == Vec2.Y);
	return Result;
}

internal b32
Vec2F32IsEqual(vec2_f32 Vec1, vec2_f32 Vec2)
{
	b32 Result = (Vec1.X == Vec2.X) && (Vec1.Y == Vec2.Y);
	return Result;
}

// [Misc]

internal vec4_f32 
NormalizeColors(vec4_f32 Vector)
{
	f32      Inverse = 1.f / 255;
	vec4_f32 Result  = Vec4F32(Vector.X * Inverse, Vector.Y * Inverse, Vector.Z * Inverse, Vector.W * Inverse);
	return Result;
}