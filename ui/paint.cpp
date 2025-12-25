// -----------------------------------------------------------------------------------
// Painting internal Implementation

// NOTE:
// Slight fritiction with the resource API. The members of resource state are too verbose 
// and can already be inferred from the context.

static render_batch_list *
GetPaintBatchList(text *Text, memory_arena *Arena, rect_float RectangleClip)
{
    VOID_ASSERT(Arena); // Internal Corruption

    core::void_context &Context = core::GetVoidContext();

    render_pass           *Pass     = GetRenderPass(Arena, RenderPass_UI);
    render_pass_params_ui *UIParams = &Pass->Params.UI.Params;
    rect_group_node       *Node     = UIParams->Last;

    rect_group_params Params = {};
    {
        Params.Transform = Mat3x3Identity();
        Params.Clip      = RectangleClip;

        if(Text)
        {
            auto FontState = FindResourceByKey(Text->FontKey, core::FindResourceFlag::None, Context.ResourceTable);
            if(FontState.Resource)
            {
                font *Font = static_cast<font *>(FontState.Resource);

                Params.Texture     = Font->CacheView;
                Params.TextureSize = Font->TextureSize;
            }
        }
    }

    bool CanMergeNodes = (Node && CanMergeRectGroupParams(&Node->Params, &Params));
    if(CanMergeNodes)
    {
        Params.Texture     = IsValidRenderHandle(Node->Params.Texture) ? Node->Params.Texture     : Params.Texture;
        Params.TextureSize = IsValidRenderHandle(Node->Params.Texture) ? Node->Params.TextureSize : Params.TextureSize;
    }

    if(!Node || !CanMergeNodes)
    {
        Node = PushStruct(Arena, rect_group_node);
        Node->BatchList.BytesPerInstance = sizeof(core::rect);

        AppendToLinkedList(UIParams, Node, UIParams->Count);
    }

    VOID_ASSERT(Node);

    Node->Params = Params;

    render_batch_list *Result = &Node->BatchList;
    return Result;
}


static void
PaintUIRect(rect_float Rect, core::color Color, core::corner_radius CornerRadii, float BorderWidth, float Softness, render_batch_list *BatchList, memory_arena *Arena)
{
    core::rect *UIRect = (core::rect *)PushDataInBatchList(Arena, BatchList);
    UIRect->RectBounds    = Rect;
    UIRect->ColorTL       = Color;
    UIRect->ColorBL       = Color;
    UIRect->ColorTR       = Color;
    UIRect->ColorBR       = Color; 
    UIRect->CornerRadii   = CornerRadii;
    UIRect->BorderWidth   = BorderWidth;
    UIRect->Softness      = Softness;
    UIRect->TextureSource = {};
    UIRect->SampleTexture = 0;
}

static void
PaintUIImage(rect_float Rect, rect_float Source, render_batch_list *BatchList, memory_arena *Arena)
{
    core::rect *UIRect = (core::rect *)PushDataInBatchList(Arena, BatchList);
    UIRect->RectBounds    = Rect;
    UIRect->ColorTL       = {.R  = 1, .G  = 1, .B  = 1, .A  = 1};
    UIRect->ColorBL       = {.R  = 1, .G  = 1, .B  = 1, .A  = 1};
    UIRect->ColorTR       = {.R  = 1, .G  = 1, .B  = 1, .A  = 1};
    UIRect->ColorBR       = {.R  = 1, .G  = 1, .B  = 1, .A  = 1};
    UIRect->CornerRadii   = {.TL = 0, .TR = 0, .BR = 0, .BL = 0};
    UIRect->BorderWidth   = 0;
    UIRect->Softness      = 0;
    UIRect->TextureSource = Source;
    UIRect->SampleTexture = 1;
}

static void
PaintUIGlyph(rect_float Rect, core::color Color, rect_float Source, render_batch_list *BatchList, memory_arena *Arena)
{
    core::rect *UIRect = (core::rect *)PushDataInBatchList(Arena, BatchList);
    UIRect->RectBounds    = Rect;
    UIRect->ColorTL       = Color;
    UIRect->ColorBL       = Color;
    UIRect->ColorTR       = Color;
    UIRect->ColorBR       = Color;
    UIRect->CornerRadii   = {.TL = 0, .TR = 0, .BR = 0, .BL = 0};
    UIRect->BorderWidth   = 0;
    UIRect->Softness      = 0;
    UIRect->TextureSource = Source;
    UIRect->SampleTexture = 1;
}

// -----------------------------------------------------------------------------------
// Painting Public API Implementation

static void
ExecutePaintCommands(paint_buffer Buffer, memory_arena *Arena)
{
    VOID_ASSERT(Buffer.Commands);  // Internal Corruption
    VOID_ASSERT(Arena);            // Internal Corruption

    core::void_context &Context = core::GetVoidContext();

    for(uint32_t Idx = 0; Idx < Buffer.Size; ++Idx)
    {
        paint_command &Command = Buffer.Commands[Idx];

        rect_float       Rect     = Command.Rectangle;
        core::color         Color    = Command.Color;
        core::corner_radius Radius   = Command.CornerRadius;
        float            Softness = Command.Softness;

        auto  TextState = FindResourceByKey(Command.TextKey , core::FindResourceFlag::None, Context.ResourceTable);
        auto *Text      = static_cast<text  *>(TextState.Resource);

        // TODO: Can this return NULL?
        render_batch_list *BatchList = GetPaintBatchList(Text, Arena, Command.RectangleClip);

        if(Color.A > 0.f)
        {
            PaintUIRect(Rect, Color, Radius, 0, Softness, BatchList, Arena);
        }

        core::color BorderColor = Command.BorderColor;
        float    BorderWidth = Command.BorderWidth;

        if(BorderColor.A > 0.f && BorderWidth > 0.f)
        {
            PaintUIRect(Rect, BorderColor, Radius, BorderWidth, Softness, BatchList, Arena);
        }

        if(Text)
        {
            core::color TextColor = Command.TextColor;

            for(uint32_t Idx = 0; Idx < Text->ShapedCount; ++Idx)
            {
                const shaped_glyph &Glyph = Text->Shaped[Idx];

                PaintUIGlyph(Glyph.Position, TextColor, Glyph.Source, BatchList, Arena);

                VOID_ASSERT(Glyph.Source.Left <= Glyph.Source.Right );
                VOID_ASSERT(Glyph.Source.Top  <= Glyph.Source.Bottom);
            }
        }

        // TODO: RE-IMPLEMENT TEXT-INPUT & IMAGE DRAWING (TRIVIAL, JUST READ RESOURCE KEY?)
        // TODO: RE-IMPLEMENT CLIPPING AND WHATNOT
        // TODO: RE-IMPLEMENT DEBUG DRAWING
    }
}
