#pragma once

// [Macros and linking]

#define COBJMACROS
DisableWarning(4201)
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_3.h>
#include <dxgidebug.h>
#pragma comment (lib, "d3d11")
#pragma comment (lib, "dxgi")
#pragma comment (lib, "dxguid")
#pragma comment (lib, "d3dcompiler")

// [Core Types]

typedef struct d3d11_rect_uniform_buffer
{
    vec4_f32 Transform[3];
    vec2_f32 ViewportSize;
} d3d11_rect_uniform_buffer;

typedef struct d3d11_input_layout
{
    read_only D3D11_INPUT_ELEMENT_DESC *Desc;
              u32                       Count;
} d3d11_input_layout;

typedef struct d3d11_backend
{
    // Base Objects
    ID3D11Device           *Device;
    ID3D11DeviceContext    *DeviceContext;
    IDXGISwapChain1        *SwapChain;
    ID3D11RenderTargetView *RenderView;

    // Default Pipeline State
    ID3D11BlendState *DefaultBlendState;

    // Buffers
    ID3D11Buffer *VBuffer64KB;

    // Pipelines
    ID3D11InputLayout  *ILayouts[RenderPass_Type_Count];
    ID3D11VertexShader *VShaders[RenderPass_Type_Count];
    ID3D11PixelShader  *PShaders[RenderPass_Type_Count];
    ID3D11Buffer       *UBuffers[RenderPass_Type_Count];

    // State
    vec2_i32 LastResolution;
} d3d11_backend;

// [Globals]

read_only global D3D11_INPUT_ELEMENT_DESC D3D11RectILayout[] =
{
    {"POS" , 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"COL" , 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"CORR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"STY" , 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
};

read_only global u8 D3D11RectVShader[] =
"// [Inputs/Outputs]                                                                             \n"
"                                                                                                \n"
"cbuffer Constants : register(b0)                                                                \n"
"{                                                                                               \n"
"    float3x3 Transform;                                                                         \n"
"    float2   ViewportSizePixel;                                                                 \n"
"};                                                                                              \n"
"                                                                                                \n"
"struct CPUToVertex                                                                              \n"
"{                                                                                               \n"
"    float4 DestRectPixel    : POS;                                                              \n"
"    float4 Col              : COL;                                                              \n"
"    float4 CornerRadii_Pixel: CORR;                                                             \n"
"    float4 StyleParams      : STY;        // X: BorderWidth, Y: Softness                        \n"
"    uint   VertexId         : SV_VertexID;                                                      \n"
"};                                                                                              \n"
"                                                                                                \n"
"struct VertexToPixel                                                                            \n"
"{                                                                                               \n"
"   float4 Position           : SV_POSITION;                                                     \n"
"   float4 Col                : COLOR;                                                           \n"
"   float2 SDFSamplePos       : SDF;                                                             \n"
"   float  CornerRadius_Pixel : COR;                                                             \n"
"                                                                                                \n"
"   nointerpolation float2 RectHalfSize_Pixel : RHS;                                             \n"
"   nointerpolation float  Softness_Pixel     : SFT;                                             \n"
"   nointerpolation float  BorderWidth_Pixel  : BDW;                                             \n"
"};                                                                                              \n"
"                                                                                                \n"
"// [Helpers]                                                                                    \n"
"                                                                                                \n"
"float RectSDF(float2 SamplePosition, float2 RectHalfSize, float Radius)                         \n" // Everything is solved in the first quadrant because of symmetry on both axes.
"{                                                                                               \n"
"    float2 FirstQuadrantPos = abs(SamplePosition) - RectHalfSize + Radius;                      \n" // Quadrant Folding Inwards offset by corner radius | > 0 == Outside
"    float  OuterDistance    = length(max(FirstQuadrantPos, 0.0));                               \n" // > 0 == Outside, on each axis. Length(p) == Closest Edge from outside
"    float  InnerDistance    = min(max(FirstQuadrantPos.x, FirstQuadrantPos.y), 0.0);            \n" // < = == Inside. If any axis > 0, Inner == 0.
"                                                                                                \n"
"    return OuterDistance + InnerDistance - Radius;                                              \n"
"}                                                                                               \n"
"                                                                                                \n"
"VertexToPixel VSMain(CPUToVertex Input)                                                         \n"
"{                                                                                               \n"
"    float2 RectTopL_Pixel = Input.DestRectPixel.xy;                                             \n"
"    float2 RectBotR_Pixel = Input.DestRectPixel.zw;                                             \n"
"    float2 RectSize_Pixel = abs(RectBotR_Pixel - RectTopL_Pixel);                               \n"
"                                                                                                \n"
"    float2 CornerPosition_Pixel[] =                                                             \n"
"    {                                                                                           \n"
"        float2(RectTopL_Pixel.x, RectBotR_Pixel.y),                                             \n"
"        float2(RectTopL_Pixel.x, RectTopL_Pixel.y),                                             \n"
"        float2(RectBotR_Pixel.x, RectBotR_Pixel.y),                                             \n"
"        float2(RectBotR_Pixel.x, RectTopL_Pixel.y),                                             \n"
"    };                                                                                          \n"
"                                                                                                \n"
"    float CornerRadii_Pixel[] =                                                                 \n"
"    {                                                                                           \n"
"        Input.CornerRadii_Pixel.y,                                                              \n"
"        Input.CornerRadii_Pixel.x,                                                              \n"
"        Input.CornerRadii_Pixel.w,                                                              \n"
"        Input.CornerRadii_Pixel.z,                                                              \n"
"    };                                                                                          \n"
"                                                                                                \n"
"    float2 CornerAxisPercent;                                                                   \n"
"    CornerAxisPercent.x = (Input.VertexId >> 1) ? 1.f : 0.f;                                    \n"
"    CornerAxisPercent.y = (Input.VertexId &  1) ? 0.f : 1.f;                                    \n"
"                                                                                                \n"
"    float2 Transformed = mul(Transform, float3(CornerPosition_Pixel[Input.VertexId], 1.f)).xy;  \n"
"    Transformed.y = ViewportSizePixel.y - Transformed.y;                                        \n"
"                                                                                                \n"
"    VertexToPixel Output;                                                                       \n"
"    Output.Position.xy        = ((2.f * Transformed) / ViewportSizePixel) - 1.f;                \n"
"    Output.Position.z         = 0.f;                                                            \n"
"    Output.Position.w         = 1.f;                                                            \n"
"    Output.CornerRadius_Pixel = CornerRadii_Pixel[Input.VertexId];                              \n"
"    Output.BorderWidth_Pixel  = Input.StyleParams.x;                                            \n"
"    Output.Softness_Pixel     = Input.StyleParams.y;                                            \n"
"    Output.RectHalfSize_Pixel = RectSize_Pixel / 2.f;                                           \n"
"    Output.SDFSamplePos       = (2.f * CornerAxisPercent - 1.f) * Output.RectHalfSize_Pixel;    \n"
"    Output.Col                = Input.Col;                                                      \n"
"                                                                                                \n"
"    return Output;                                                                              \n"
"}                                                                                               \n"
"                                                                                                \n"
"float4 PSMain(VertexToPixel Input) : SV_TARGET                                                  \n"
"{                                                                                               \n"
"    float BorderSDF = 1;                                                                        \n"
"    if(Input.BorderWidth_Pixel > 0)                                                             \n"
"    {                                                                                           \n"
"        float2 SamplePosition = Input.SDFSamplePos;                                             \n"
"        float2 Softness       = float2(2.f * Input.Softness_Pixel, 2.f * Input.Softness_Pixel); \n"
"        float2 RectHalfSize   = Input.RectHalfSize_Pixel - Input.BorderWidth_Pixel - Softness;  \n"
"        float  Radius         = max(Input.CornerRadius_Pixel - Input.BorderWidth_Pixel, 0);     \n" // Prevent negative radius
"                                                                                                \n"
"        BorderSDF = RectSDF(SamplePosition, RectHalfSize, Radius);                              \n"
"        BorderSDF = smoothstep(0, 2 * Input.Softness_Pixel, BorderSDF);                         \n" // 0->1 clamping based on softness. BorderSDF == 0 if inside, BorderSDF == 1 if far.
"    }                                                                                           \n"
"                                                                                                \n"
"    if(BorderSDF < 0.001f) discard;                                                             \n" // If it is not part of the border mask discard it (Hollow Center)
"                                                                                                \n"
"    float CornerSDF = 1;                                                                        \n"
"    if(Input.CornerRadius_Pixel > 0 || Input.Softness_Pixel > 0.75f)                            \n"
"    {                                                                                           \n"
"        float2 SamplePosition = Input.SDFSamplePos;                                             \n"
"        float2 Softness       = float2(2.f * Input.Softness_Pixel, 2.f * Input.Softness_Pixel); \n"
"        float2 RectHalfSize   = Input.RectHalfSize_Pixel - Softness;                            \n"
"        float  CornerRadius   = Input.CornerRadius_Pixel;                                       \n"
"                                                                                                \n"
"        CornerSDF = RectSDF(SamplePosition, RectHalfSize, CornerRadius);                        \n"
"        CornerSDF = 1.0 - smoothstep(0, 2 * Input.Softness_Pixel, CornerSDF);                   \n" // 0->1 clamping based on softness. == 1 if inside, < 1 if outside.
"    }                                                                                           \n"
"                                                                                                \n"
"    float4 Output = Input.Col;                                                                  \n"
"    Output.a *= CornerSDF;                                                                      \n"
"    Output.a *= BorderSDF;                                                                      \n"
"                                                                                                \n"
"    return Output;                                                                              \n"
"}                                                                                               \n"
;

read_only global byte_string D3D11ShaderSourceTable[] =
{
    byte_string_compile(D3D11RectVShader),
};

read_only global d3d11_input_layout D3D11ILayoutTable[] =
{
    {D3D11RectILayout, ArrayCount(D3D11RectILayout)},
};

read_only global u32 D3D11UniformBufferSizeTable[] =
{
    {(u32)sizeof(d3d11_rect_uniform_buffer)},
};