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
    {"POS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"COL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"STY", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
};

read_only global u8 D3D11RectVShader[] =
"// [Inputs/Outputs]                                                                           \n"
"                                                                                              \n"
"cbuffer Constants : register(b0)                                                              \n"
"{                                                                                             \n"
"    float3x3 Transform;                                                                       \n"
"    float2   ViewportSizePixel;                                                               \n"
"};                                                                                            \n"
"                                                                                              \n"
"struct CPUToVertex                                                                            \n"
"{                                                                                             \n"
"    float4 DestRectPixel : POS;                                                               \n"
"    float4 Col           : COL;                                                               \n"
"    float4 StyleParams   : STY;        // X: BorderWidth                                      \n"
"    uint   VertexId      : SV_VertexID;                                                       \n"
"};                                                                                            \n"
"                                                                                              \n"
"struct VertexToPixel                                                                          \n"
"{                                                                                             \n"
"   float4 Position     : SV_POSITION;                                                         \n"
"   float4 Col          : COLOR;                                                               \n"
"   float2 SDFSamplePos : SDF;                                                                 \n"
"                                                                                              \n"
"   nointerpolation float2 RectHalfSize_Pixel : RHS;                                           \n"
"   nointerpolation float  BorderWidth_Pixel  : BDW;                                           \n"
"};                                                                                            \n"
"                                                                                              \n"
"// [Helpers]                                                                                  \n"
"                                                                                              \n"
"// NOTE: There seems to be a shortcut to do this. This is a basic impl for prototyping.       \n"
"float RectSDF(float2 SamplePosition, float2 RectHalfSize)                                     \n"
"{                                                                                             \n"
"    float2 FirstQuadrantPos = abs(SamplePosition) - RectHalfSize;                             \n"
"    float  OuterDistance    = length(max(FirstQuadrantPos, 0.0));                             \n"
"    float  InnerDistance    = min(max(FirstQuadrantPos.x, FirstQuadrantPos.y), 0.0);          \n"
"    return OuterDistance + InnerDistance;                                                     \n"
"}                                                                                             \n"
"                                                                                              \n"
"VertexToPixel VSMain(CPUToVertex Input)                                                       \n"
"{                                                                                             \n"
"    float2 RectTopL_Pixel = Input.DestRectPixel.xy;                                           \n"
"    float2 RectBotR_Pixel = Input.DestRectPixel.zw;                                           \n"
"    float2 RectSize_Pixel = abs(RectBotR_Pixel - RectTopL_Pixel);                             \n"
"                                                                                              \n"
"    float2 CornerPositionPixel[] =                                                            \n"
"    {                                                                                         \n"
"        float2(RectTopL_Pixel.x, RectBotR_Pixel.y),                                           \n"
"        float2(RectTopL_Pixel.x, RectTopL_Pixel.y),                                           \n"
"        float2(RectBotR_Pixel.x, RectBotR_Pixel.y),                                           \n"
"        float2(RectBotR_Pixel.x, RectTopL_Pixel.y),                                           \n"
"    };                                                                                        \n"
"                                                                                              \n"
"    float2 CornerAxisPercent;                                                                 \n"
"    CornerAxisPercent.x = (Input.VertexId >> 1) ? 1.f : 0.f;                                  \n"
"    CornerAxisPercent.y = (Input.VertexId &  1) ? 0.f : 1.f;                                  \n"
"                                                                                              \n"
"    float2 Transformed = mul(Transform, float3(CornerPositionPixel[Input.VertexId], 1.f)).xy; \n"
"    Transformed.y = ViewportSizePixel.y - Transformed.y;                                      \n"
"                                                                                              \n"
"    VertexToPixel Output;                                                                     \n"
"    Output.Position.xy        = ((2.f * Transformed) / ViewportSizePixel) - 1.f;              \n"
"    Output.Position.z         = 0.f;                                                          \n"
"    Output.Position.w         = 1.f;                                                          \n"
"    Output.BorderWidth_Pixel  = Input.StyleParams.x;                                          \n"
"    Output.RectHalfSize_Pixel = RectSize_Pixel / 2.f;                                         \n"
"    Output.SDFSamplePos       = (2.f * CornerAxisPercent - 1.f) * Output.RectHalfSize_Pixel;  \n"
"    Output.Col                = Input.Col;                                                    \n"
"                                                                                              \n"
"    return Output;                                                                            \n"
"}                                                                                             \n"
"                                                                                              \n"
"float4 PSMain(VertexToPixel Input) : SV_TARGET                                                \n"
"{                                                                                             \n"
"    float BorderSDF = 1;                                                                      \n"
"    if(Input.BorderWidth_Pixel > 0)                                                           \n"
"    {                                                                                         \n"
"        float2 SamplePosition = Input.SDFSamplePos;                                           \n"
"        float2 RectHalfSize   = Input.RectHalfSize_Pixel - Input.BorderWidth_Pixel;           \n"
"                                                                                              \n"
"        BorderSDF = RectSDF(SamplePosition, RectHalfSize);                                    \n"
"    }                                                                                         \n"
"                                                                                              \n"
"    if(BorderSDF < 0.001f) discard;                                                           \n"
"                                                                                              \n"
"    float4 Output = Input.Col;                                                                \n"
"                                                                                              \n"
"    return Output;                                                                            \n"
"}                                                                                             \n"
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