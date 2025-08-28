typedef enum CimWindow_Flags
{
    CimWindow_AllowDrag = 1 << 0,
} CimWindow_Flags;

#define UIBeginContext() for (UIPass_Type Pass = UIPass_Layout; Pass !=  UIPass_Ended; Pass = (UIPass_Type)((cim_u32)Pass + 1))
#define UIEndContext()   if (Pass == UIPass_Layout) {UIEndLayout();} else if (Pass == UIPass_Draw) {UIEndDraw(UIP_LAYOUT.Tree.DrawList);}

static ui_component *
FindComponent(const char *Key)
{
    Cim_Assert(CimCurrent);
    ui_component_hashmap *Hashmap = &UI_LAYOUT.Components;

    if (!Hashmap->IsInitialized)
    {
        Hashmap->GroupCount = 32;

        cim_u32 BucketCount = Hashmap->GroupCount * CimBucketGroupSize;
        size_t  BucketSize = BucketCount * sizeof(ui_component_entry);
        size_t  MetadataSize = BucketCount * sizeof(cim_u8);

        Hashmap->Buckets = (ui_component_entry *)malloc(BucketSize);
        Hashmap->Metadata = (cim_u8 *)malloc(MetadataSize);

        if (!Hashmap->Buckets || !Hashmap->Metadata)
        {
            return NULL;
        }

        memset(Hashmap->Buckets, 0, BucketSize);
        memset(Hashmap->Metadata, CimEmptyBucketTag, MetadataSize);

        Hashmap->IsInitialized = true;
    }

    cim_u32 ProbeCount = 0;
    cim_u32 Hashed = CimHash_String32(Key);
    cim_u32 GroupIdx = Hashed & (Hashmap->GroupCount - 1);

    while (true)
    {
        cim_u8 *Meta = Hashmap->Metadata + GroupIdx * CimBucketGroupSize;
        cim_u8  Tag = (Hashed & 0x7F);

        __m128i Mv = _mm_loadu_si128((__m128i *)Meta);
        __m128i Tv = _mm_set1_epi8(Tag);

        cim_i32 TagMask = _mm_movemask_epi8(_mm_cmpeq_epi8(Mv, Tv));
        while (TagMask)
        {
            cim_u32 Lane = CimHash_FindFirstBit32(TagMask);
            cim_u32 Idx = (GroupIdx * CimBucketGroupSize) + Lane;

            ui_component_entry *E = Hashmap->Buckets + Idx;
            if (strcmp(E->Key, Key) == 0)
            {
                return &E->Value;
            }

            TagMask &= TagMask - 1;
        }

        __m128i Ev = _mm_set1_epi8(CimEmptyBucketTag);
        cim_i32 EmptyMask = _mm_movemask_epi8(_mm_cmpeq_epi8(Mv, Ev));
        if (EmptyMask)
        {
            cim_u32 Lane = CimHash_FindFirstBit32(EmptyMask);
            cim_u32 Idx = (GroupIdx * CimBucketGroupSize) + Lane;

            Meta[Lane] = Tag;

            ui_component_entry *E = Hashmap->Buckets + Idx;
            strcpy_s(E->Key, sizeof(E->Key), Key);
            E->Key[sizeof(E->Key) - 1] = 0;

            return &E->Value;
        }

        ProbeCount++;
        GroupIdx = (GroupIdx + ProbeCount * ProbeCount) & (Hashmap->GroupCount - 1);
    }

    return NULL;
}

// NOTE: Then do we simply want a helper macro that does this stuff for us user side?
static void
UILayoutRow(bool DrawEdges)
{
    DrawEdges = false; // Because unused, but this would be used for debugging. 

    ui_layout_node *LayoutNode = PushLayoutNode(true, NULL); Cim_Assert(LayoutNode);
    LayoutNode->Padding       = {5, 5, 5, 5};
    LayoutNode->Spacing       = {20, 0};
    LayoutNode->ContainerType = UIContainer_Row;
}

static void
UIEndLayoutRow()
{
    PopParent();
}

// TODO: Figure this out.
static void
UILayoutText(const char *TextToRender)
{
    Cim_Assert(CimCurrent && CimCurrent->CurrentFont.IsValid && TextToRender);

    // TODO: How do we query the state. Can we simply hash the string?
    // That seems heavy.
}

static bool
UILayoutWindow(const char *Id, const char *ThemeId, ui_component_state *State)
{
    Cim_Assert(CimCurrent && Id && ThemeId);

    ui_layout_node *LayoutNode = PushLayoutNode(true, State); Cim_Assert(LayoutNode);
    ui_component   *Component  = FindComponent(Id);           Cim_Assert(Component);

    ui_window_theme Theme = GetWindowTheme(ThemeId, Component->ThemeId);
    Cim_Assert(Theme.ThemeId.Value);

    LayoutNode->Padding       = Theme.Padding;
    LayoutNode->Spacing       = Theme.Spacing;
    LayoutNode->ContainerType = UIContainer_Column;

    // TODO: Get the X,Y from somewhere else and do a better job for the layout.
    LayoutNode->X = 500.0f;
    LayoutNode->Y = 400.0f;

    if (Theme.BorderWidth > 0)
    {
        ui_draw_command *Command = AllocateDrawCommand(UIP_LAYOUT.Tree.DrawList);
        Command->Type         = UICommand_Border;
        Command->Pipeline     = UIPipeline_Default;
        Command->ClippingRect = {};
        Command->LayoutNodeId = LayoutNode->Id;

        // We now set extra data the layout doesn't care about.
        Command->ExtraData.Border.Color = Theme.BorderColor;
        Command->ExtraData.Border.Width = Theme.BorderWidth;
    }

    {
        ui_draw_command *Command = AllocateDrawCommand(UIP_LAYOUT.Tree.DrawList);
        if (Command)
        {
            Command->Type         = UICommand_Quad;
            Command->Pipeline     = UIPipeline_Default;
            Command->ClippingRect = {};
            Command->LayoutNodeId = LayoutNode->Id;

            // We now set extra data the layout doesn't care about.
            Command->ExtraData.Quad.Color = Theme.Color;
        }
    }

    // Cache it for faster retrieval. Could be done at initialization if we have that.
    Component->ThemeId = Theme.ThemeId;

    return true;
}

static void
UILayoutButton(const char *Id, const char *ThemeId, ui_component_state *State)
{
    Cim_Assert(CimCurrent && Id && ThemeId);

    ui_layout_node *LayoutNode = PushLayoutNode(false, State); Cim_Assert(LayoutNode);
    ui_component   *Component  = FindComponent(Id);            Cim_Assert(Component);

    ui_button_theme Theme = GetButtonTheme(ThemeId, Component->ThemeId);
    if (Theme.ThemeId.Value)
    {
        LayoutNode->Width  = Theme.Size.x;
        LayoutNode->Height = Theme.Size.y;
    }

    if (Theme.BorderWidth > 0)
    {
        ui_draw_command *Command = AllocateDrawCommand(UIP_LAYOUT.Tree.DrawList);
        Command->Type         = UICommand_Border;
        Command->Pipeline     = UIPipeline_Default;
        Command->ClippingRect = {};
        Command->LayoutNodeId = LayoutNode->Id;

        // We now set extra data the layout doesn't care about.
        Command->ExtraData.Border.Color = Theme.BorderColor;
        Command->ExtraData.Border.Width = Theme.BorderWidth;
    }

    {
        ui_draw_command *Command = AllocateDrawCommand(UIP_LAYOUT.Tree.DrawList);
        if (Command)
        {
            Command->Type         = UICommand_Quad;
            Command->Pipeline     = UIPipeline_Default;
            Command->ClippingRect = {};
            Command->LayoutNodeId = LayoutNode->Id;

            // We now set extra data the layout doesn't care about.
            Command->ExtraData.Quad.Color = Theme.Color;
        }
    }
}


//// NOTE: We probably don't want to take this as an argument. Something like UISetFont.
//static void
//UITextInternal(const char *TextToRender, UIPass_Type Pass)
//{
//    Cim_Assert(CimCurrent && CimCurrent->CurrentFont.IsValid && TextToRender);
//
//    ui_font Font = CimCurrent->CurrentFont;
//
//    // TODO: Figure this out, because this limits us to one component.
//    static ui_component Component;
//
//    if (Pass == UIPass_Layout)
//    {
//        cim_text *Text = &Component.For.Text;
//
//        // BUG: This is still wrong. Can't we wait on the layout? But the layout on the dimensions.
//        // Weird circular dependency.
//
//        ui_layout_node *Node = PushLayoutNode(false, &Component.LayoutNodeIndex);
//        Node->ContentWidth  = 100;
//        Node->ContentHeight = 50;
//
//        // WARN: I don't know about this.
//        if (!Text->LayoutInfo.BackendLayout)
//        {
//            Text->LayoutInfo = CreateTextLayout((char*)TextToRender, Node->ContentWidth, Node->ContentHeight, Font);
//        }
//
//        // NOTE: Let's do the most naive thing and iterate the string without checking dirty states.
//        for (cim_u32 Idx = 0; Idx < Text->LayoutInfo.GlyphCount; Idx++)
//        {
//            glyph_hash Hash  = ComputeGlyphHash(Text->LayoutInfo.GlyphLayoutInfo[Idx].GlyphId);
//            glyph_info Glyph = FindGlyphEntryByHash(Font.Table, Hash);
//
//            if (!Glyph.IsInAtlas)
//            {
//                glyph_size GlyphSize = GetGlyphExtent((char*) & TextToRender[Idx], 1, Font);
//
//                stbrp_rect Rect;
//                Rect.id = 0;
//                Rect.w  = GlyphSize.Width;
//                Rect.h  = GlyphSize.Height;
//                stbrp_pack_rects(&Font.AtlasContext, &Rect, 1);
//                if (Rect.was_packed)
//                {
//                    cim_f32 U0 = (cim_f32) Rect.x           / Font.AtlasWidth;
//                    cim_f32 V0 = (cim_f32) Rect.y           / Font.AtlasHeight;
//                    cim_f32 U1 = (cim_f32)(Rect.x + Rect.w) / Font.AtlasWidth;
//                    cim_f32 V1 = (cim_f32)(Rect.y + Rect.h) / Font.AtlasHeight;
//
//                    RasterizeGlyph(TextToRender[Idx], Rect, Font);
//                    UpdateGlyphCacheEntry(Font.Table, Glyph.MapId, true, U0, V0, U1, V1, GlyphSize);
//                }
//                else
//                {
//                    // TODO: This either means there is a bug or there is no more
//                    // place in the atlas. Note that we don't have a way to free
//                    // things from the atlas at the moment. I think as things get
//                    // evicted from the cache, we should also free their rects in
//                    // the allocator.
//                }
//            }
//
//            Text->LayoutInfo.GlyphLayoutInfo[Idx].MapId = Glyph.MapId;
//        }
//
//    }
//    else if (Pass == UIPass_User)
//    {
//        // WARN: Then most of this code, has nothing to do on this pass. Which means we now
//        // need 3 pass. Sounds fine to me.
//
//        typedef struct local_vertex
//        {
//            cim_f32 PosX, PosY;
//            cim_f32 U, V;
//        } local_vertex;
//
//        cim_text        *Text = &Component.For.Text;
//        ui_layout_node *Node = GetNodeFromIndex(Component.LayoutNodeIndex); // Can't we just call get next node since it's the same order? Same for hashmap?
//
//        // WARN: Shouldn't be done here.
//        cim_cmd_buffer   *Buffer  = UIP_COMMANDS;
//        cim_draw_command *Command = GetDrawCommand(Buffer, {}, UIPipeline_Text);
//        Command->Font = Font;
//
//        cim_f32 PenX = Node->X;
//        cim_f32 PenY = Node->Y;
//
//        // NOTE: There are two problems currently with this loop:
//        // 1) It's trying to do too much. Move the hashing/checking for atlas to the other phase?
//        // 2) We currently have no way to compute the hash.
//
//        // TODO: Make a draw text routine?
//        for (cim_u32 Idx = 0; Idx < Text->LayoutInfo.GlyphCount; Idx++)
//        {
//            glyph_layout_info *Layout = &Text->LayoutInfo.GlyphLayoutInfo[Idx];
//            glyph_entry       *Glyph  = GetGlyphEntry(Font.Table, Layout->MapId);
//
//            if (PenX + Layout->AdvanceX >= Node->X + Node->Width)
//            {
//                PenX  = Node->X;
//                PenY += Font.LineHeight;
//            }
//
//            cim_f32 MinX = PenX + Layout->OffsetX;
//            cim_f32 MinY = PenY + Layout->OffsetY;
//            cim_f32 MaxX = PenX + Layout->OffsetX + Glyph->Size.Width;
//            cim_f32 MaxY = PenY + Layout->OffsetY + Glyph->Size.Height;
//
//            local_vertex Vtx[4];
//            Vtx[0] = (local_vertex){MinX, MinY, Glyph->U0, Glyph->V0}; // Top-left
//            Vtx[1] = (local_vertex){MinX, MaxY, Glyph->U0, Glyph->V1}; // Bottom-left
//            Vtx[2] = (local_vertex){MaxX, MinY, Glyph->U1, Glyph->V0}; // Top-right
//            Vtx[3] = (local_vertex){MaxX, MaxY, Glyph->U1, Glyph->V1}; // Bottom-right
//
//            cim_u32 Indices[6];
//            Indices[0] = Command->VtxCount + 0;
//            Indices[1] = Command->VtxCount + 2;
//            Indices[2] = Command->VtxCount + 1;
//            Indices[3] = Command->VtxCount + 2;
//            Indices[4] = Command->VtxCount + 3;
//            Indices[5] = Command->VtxCount + 1;
//
//            WriteToArena(Vtx    , sizeof(Vtx)    , &Buffer->FrameVtx);
//            WriteToArena(Indices, sizeof(Indices), &Buffer->FrameIdx);;
//
//            Command->VtxCount += 4;
//            Command->IdxCount += 6;
//
//            PenX += Layout->AdvanceX;
//        }
//
//    }
//}