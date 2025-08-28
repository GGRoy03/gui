#pragma once

typedef enum CimRenderer_Backend
{
	CimRenderer_D3D = 0,
} CimRenderer_Backend;

typedef enum UIShader_Type
{
    UIShader_Vertex = 0,
    UIShader_Pixel  = 1,
} UIShader_Type;

typedef enum UIPipeline_Type
{
    UIPipeline_None    = 0,
    UIPipeline_Default = 1,
    UIPipeline_Text    = 2,
    UIPipeline_Count   = 3,
} UIPipeline_Type;

typedef struct ui_shader_desc
{
    char         *SourceCode;
    UIShader_Type Type;
} ui_shader_desc;

typedef struct ui_vertex
{
    cim_f32 PosX, PosY;
    cim_f32 U, V;
    cim_f32 R, G, B, A;
} ui_vertex;

typedef struct ui_draw_batch
{
    size_t VertexByteOffset;
    size_t BaseVertexOffset;
    size_t StartIndexRead;

    cim_u32 IdxCount;
    cim_u32 VtxCount;

    cim_rect      ClippingRect;
    cim_bit_field Features;
    UIPipeline_Type PipelineType;
} ui_draw_batch;

typedef struct ui_draw_batch_buffer
{
    cim_u32   GlobalVtxOffset;
    cim_u32   GlobalIdxOffset;
    cim_arena FrameVtx;
    cim_arena FrameIdx;

    ui_draw_batch *Batches;
    cim_u32        BatchCount;
    cim_u32        BatchSize;

    cim_rect        CurrentClipRect;
    UIPipeline_Type CurrentPipelineType;
} ui_draw_batch_buffer;

// NOTE: This probably changes
typedef enum UICommand_Type
{
    UICommand_None   = 0,
    UICommand_Quad   = 1,
    UICommand_Border = 2,
    UICommand_Text   = 3,
} UICommand_Type;

typedef struct ui_draw_command_quad
{
    cim_vector4 Color;
} ui_draw_command_quad;

typedef struct ui_draw_command_border
{
    cim_vector4 Color;
    cim_f32     Width;
} ui_draw_command_border;

typedef struct ui_draw_command
{
    UICommand_Type Type;
    union
    {
        ui_draw_command_quad   Quad;
        ui_draw_command_border Border;
    } ExtraData;

    // Data-Retrieval
    cim_u32  LayoutNodeId;

    cim_rect        ClippingRect;
    UIPipeline_Type Pipeline;
} ui_draw_command;

typedef struct ui_draw_list
{
    ui_draw_command Commands[16];
    cim_u32         CommandCount;
} ui_draw_list;

// V-Table
typedef void DrawUI (cim_i32 Width, cim_i32 Height);

// Text
typedef void    draw_glyph    (stbrp_rect Rect, ui_font Font);
typedef ui_font load_font     (const char *FontName, cim_f32 FontSize);
typedef void    release_font  (ui_font *Font);

typedef struct cim_renderer 
{
	DrawUI *Draw;
    
    // Text
    draw_glyph   *TransferGlyph;
    load_font    *LoadFont;
    release_font *ReleaseFont;
} cim_renderer;

// NOTE: This is an internal proxy only, unsure how to build the interface.
static void TransferGlyph  (stbrp_rect Rect, ui_font Font);