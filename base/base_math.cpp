// [Constructors]

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

internal matrix_3x3 
Mat3x3Zero(void)
{
    matrix_3x3 Result = {};
    return Result;
}

internal matrix_3x3
Mat3x3Identity(void)
{
    matrix_3x3 Result;
    Result.c0r0 = 1;
    Result.c0r1 = 0;
    Result.c0r2 = 0;
    Result.c1r0 = 0;
    Result.c1r1 = 1;
    Result.c1r2 = 0;
    Result.c2r0 = 0;
    Result.c2r1 = 0;
    Result.c2r2 = 1;

    return Result;
}

// [Rect]

internal f32
RoundedRectSDF(rect_sdf_params Params)
{
    // Abuse the symmetry and fold every point into the first quadrant.
    // Offset these points by RectHalfSize, to figure out if they still lay inside the quadrant.
    // If it still does, then we know the point was outside the rect, else it was inside.

    vec2_f32 RadiusVector  = vec2_f32(Params.Radius, Params.Radius);
    vec2_f32 FirstQuadrant = (Params.PointPosition.Absolute() - Params.HalfSize) + RadiusVector;

    // OuterDistance: If any axis is positive, take its length to figure out the closest distance to the boundary.
    // InnerDistance: If any axis is positive, this results in a 0. Else return the "less negative" value (Closest to edge)

    f32 OuterDistance = vec2_f32(Max(FirstQuadrant.X, 0.f), Max(FirstQuadrant.Y, 0.f)).Length();
    f32 InnerDistance = Min(Max(FirstQuadrant.X, FirstQuadrant.Y), 0.f);
    f32 Result        = OuterDistance + InnerDistance - Params.Radius;

    return Result;
}

// [Ranges]

internal b32
IsInRangeF32(f32 Min, f32 Max, f32 Value)
{
    b32 Result = (Value >= Min && Value <= Max);
    return Result;
}

internal b32
Mat3x3AreEqual(matrix_3x3 *m1, matrix_3x3 *m2)
{
	b32 Result = MemoryCompare(m1, m2, sizeof(matrix_3x3)) == 0;
	return Result;
}

// [Misc]
