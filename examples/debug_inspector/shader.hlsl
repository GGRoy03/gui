// [Inputs/Outputs]

cbuffer Constants : register(b0)
{
    float3x3 Transform;
    float2   ViewportSizeInPixel;
    float2   AtlasSizeInPixel;
};

struct CPUToVertex
{
    float4 RectInPixel        : POS;
    float4 AtlasSrcInPixel    : FONT;
    float4 ColorTopLeft       : COL0;
    float4 ColorBotLeft       : COL1;
    float4 ColorTopRight      : COL2;
    float4 ColorBotRight      : COL3;
    float4 CornerRadiiInPixel : CORR;
    float4 StyleParams        : STY;   // X: BorderWidth, Y: Softness, Z: Sample Atlas
    uint   VertexId           : SV_VertexID;
};

struct VertexToPixel
{
    float4 Position            : SV_POSITION;
    float4 Tint                : TINT;
    float2 TexCoordInPercent   : TXP;
    float2 SDFSamplePos        : SDF;
    float  CornerRadiusInPixel : COR;

    nointerpolation float2 RectHalfSizeInPixel : RHS;
    nointerpolation float  SoftnessInPixel     : SFT;
    nointerpolation float  BorderWidthInPixel  : BDW;
    nointerpolation float  MustSampleAtlas     : MSA;
};

Texture2D    AtlasTexture : register(t0);
SamplerState AtlasSampler : register(s0);

// [Helpers]

float RectSDF(float2 SamplePosition, float2 RectHalfSize, float Radius)
{
    float2 FirstQuadrantPos = abs(SamplePosition) - RectHalfSize + Radius;
    float  OuterDistance   = length(max(FirstQuadrantPos, 0.0));
    float  InnerDistance   = min(max(FirstQuadrantPos.x, FirstQuadrantPos.y), 0.0);

    return OuterDistance + InnerDistance - Radius;
}

VertexToPixel VSMain(CPUToVertex Input)
{
    float2 RectTopLeftInPixel   = Input.RectInPixel.xy;
    float2 RectBotRightInPixel  = Input.RectInPixel.zw;
    float2 AtlasTopLeftInPixel  = Input.AtlasSrcInPixel.xy;
    float2 AtlasBotRightInPixel = Input.AtlasSrcInPixel.zw;
    float2 RectSizeInPixel      = abs(RectBotRightInPixel - RectTopLeftInPixel);

    float2 CornerPositionInPixel[] =
    {
        float2(RectTopLeftInPixel.x , RectBotRightInPixel.y),
        float2(RectTopLeftInPixel.x , RectTopLeftInPixel.y),
        float2(RectBotRightInPixel.x, RectBotRightInPixel.y),
        float2(RectBotRightInPixel.x, RectTopLeftInPixel.y),
    };

    float CornerRadiusInPixel[] =
    {
        Input.CornerRadiiInPixel.y,
        Input.CornerRadiiInPixel.x,
        Input.CornerRadiiInPixel.w,
        Input.CornerRadiiInPixel.z,
    };

    float2 AtlasSourceInPixel[] =
    {
        float2(AtlasTopLeftInPixel.x , AtlasBotRightInPixel.y),
        float2(AtlasTopLeftInPixel.x , AtlasTopLeftInPixel.y),
        float2(AtlasBotRightInPixel.x, AtlasBotRightInPixel.y),
        float2(AtlasBotRightInPixel.x, AtlasTopLeftInPixel.y),
    };

    float4 SourceColor[] =
    {
        Input.ColorBotLeft,
        Input.ColorTopLeft,
        Input.ColorBotRight,
        Input.ColorTopRight,
    };

    float2 CornerAxisPercent;
    CornerAxisPercent.x = (Input.VertexId >> 1) ? 1.f : 0.f;
    CornerAxisPercent.y = (Input.VertexId &  1) ? 0.f : 1.f;

    float2 Transformed = mul(Transform, float3(CornerPositionInPixel[Input.VertexId], 1.f)).xy;
    Transformed.y = ViewportSizeInPixel.y - Transformed.y;

    VertexToPixel Output;
    Output.Position.xy         = ((2.f * Transformed) / ViewportSizeInPixel) - 1.f;
    Output.Position.z          = 0.f;
    Output.Position.w          = 1.f;
    Output.CornerRadiusInPixel = CornerRadiusInPixel[Input.VertexId];
    Output.BorderWidthInPixel  = Input.StyleParams.x;
    Output.SoftnessInPixel     = Input.StyleParams.y;
    Output.RectHalfSizeInPixel = RectSizeInPixel / 2.f;
    Output.SDFSamplePos        = (2.f * CornerAxisPercent - 1.f) * Output.RectHalfSizeInPixel;
    Output.Tint                = SourceColor[Input.VertexId];
    Output.MustSampleAtlas     = Input.StyleParams.z;
    Output.TexCoordInPercent   = AtlasSourceInPixel[Input.VertexId] / AtlasSizeInPixel;

    return Output;
}

float4 PSMain(VertexToPixel Input) : SV_TARGET
{
    float4 AlbedoSample = float4(1, 1, 1, 1);
    if(Input.MustSampleAtlas > 0)
    {
        AlbedoSample = AtlasTexture.Sample(AtlasSampler, Input.TexCoordInPercent);
    }

    float BorderSDF = 1;
    if(Input.BorderWidthInPixel > 0)
    {
        float2 SamplePosition = Input.SDFSamplePos;
        float2 Softness       = float2(2.f * Input.SoftnessInPixel, 2.f * Input.SoftnessInPixel);
        float2 RectHalfSize   = Input.RectHalfSizeInPixel - Input.BorderWidthInPixel - Softness;
        float  Radius         = max(Input.CornerRadiusInPixel - Input.BorderWidthInPixel, 0);

        BorderSDF = RectSDF(SamplePosition, RectHalfSize, Radius);
        BorderSDF = smoothstep(0, 2 * Input.SoftnessInPixel, BorderSDF);
    }

    if(BorderSDF < 0.001f) discard;

    float CornerSDF = 1;
    if(Input.CornerRadiusInPixel > 0 || Input.SoftnessInPixel > 0.75f)
    {
        float2 SamplePosition = Input.SDFSamplePos;
        float2 Softness       = float2(2.f * Input.SoftnessInPixel, 2.f * Input.SoftnessInPixel);
        float2 RectHalfSize   = Input.RectHalfSizeInPixel - Softness;
        float  CornerRadius   = Input.CornerRadiusInPixel;

        CornerSDF = RectSDF(SamplePosition, RectHalfSize, CornerRadius);
        CornerSDF = 1.0 - smoothstep(0, 2 * Input.SoftnessInPixel, CornerSDF);
    }

    float4 Output = AlbedoSample;
    Output   *= Input.Tint;
    Output.a *= CornerSDF;
    Output.a *= BorderSDF;

    return Output;
}
