// [API IMPLEMENTATION]

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