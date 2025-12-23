// =================================================================
// @Internal: Fonts Implementation

static ui_resource_key
UILoadSystemFont(byte_string Name, float Size, uint16_t CacheSizeX, uint16_t CacheSizeY)
{
    void_context &Context = GetVoidContext();

    ui_resource_key   Key   = MakeFontResourceKey(Name, Size);
    ui_resource_state State = FindResourceByKey(Key, FindResourceFlag::AddIfNotFound, Context.ResourceTable);

    if(!State.Resource)
    {
        uint64_t  Footprint = sizeof(ui_font);
        ui_font  *Font      = static_cast<ui_font *>(malloc(Footprint)); // Do we really malloc?

        if(Font)
        {
            ntext::glyph_generator_params GeneratorParams =
            {
                .TextStorage       = ntext::TextStorage::LazyAtlas,
                .FrameMemoryBudget = VOID_MEGABYTE(1),
                .FrameMemory       = malloc(VOID_MEGABYTE(1)),         // Do we really malloc?
                .CacheSizeX        = CacheSizeX,
                .CacheSizeY        = CacheSizeY,
            };

            render_texture_params CacheParams =
            {
                .Format = RenderTextureFormat::RGBA,
                .Type   = RenderTextureType::Default,
                .SizeX  = CacheSizeX,
                .SizeY  = CacheSizeY,
            };

            render_texture_params CacheTransferParams = 
            {
                .Format = RenderTextureFormat::RGBA,
                .Type   = RenderTextureType::Staging,
                .SizeX  = CacheSizeX,
                .SizeY  = CacheSizeY,
            };

            Font->Generator     = ntext::CreateGlyphGenerator(GeneratorParams);
            Font->Cache         = CreateRenderTexture(CacheParams);
            Font->CacheTransfer = CreateRenderTexture(CacheTransferParams);
            Font->CacheView     = CreateRenderTextureView(Font->Cache, RenderTextureFormat::RGBA);
            Font->TextureSize   = vec2_uint16(CacheSizeX, CacheSizeY);
            Font->Size          = Size;
            Font->Name          = Name;

            UpdateResourceTable(State.Id, Key, Font, Context.ResourceTable);
        }
    }

    return Key;
}

// =================================================================
// @Internal: Static Text Implementation

// There might be an alignment issue. Need to understand more.

static uint64_t
GetTextFootprint(ntext::analysed_text Analysed, ntext::shaped_glyph_run Run)
{
    uint64_t ShapedBufferSize = Run.ShapedCount      * sizeof(ui_shaped_glyph);
    uint64_t WordBufferSize   = Analysed.Words.Count * sizeof(ui_text_word);
    uint64_t Result           = ShapedBufferSize + WordBufferSize + sizeof(ui_text);

    return Result;
}

// This is very bad. It's quite complex for something so simple.
// How can I reduce complexity here?

static ui_text *
PlaceTextInMemory(ntext::analysed_text Analysed, ntext::shaped_glyph_run Run, ui_font *Font, void *Memory)
{
    VOID_ASSERT(Font);
    VOID_ASSERT(Memory);

    ui_text      *Result  = 0;
    void_context &Context = GetVoidContext();

    if(Memory && Context.ResourceTable)
    {
        auto         *Shaped = static_cast<ui_shaped_glyph *>(Memory);
        ui_text_word *Words  = 0;

        if(Analysed.Words.Count)
        {
            Words  = reinterpret_cast<ui_text_word *>(Shaped + Run.ShapedCount);
            Result = reinterpret_cast<ui_text      *>(Words  + Analysed.Words.Count);
        }
        else
        {
            Result = reinterpret_cast<ui_text      *>(Shaped + Run.ShapedCount);
        }

        VOID_ASSERT(Result);

        Result->Shaped      = Shaped;
        Result->Words       = Words;
        Result->FontKey     = MakeFontResourceKey(Font->Name, Font->Size);
        Result->ShapedCount = Run.ShapedCount;

        for(uint32_t Idx = 0; Idx < Run.ShapedCount; ++Idx)
        {
            Result->Shaped[Idx].OffsetX   = Run.Shaped[Idx].Layout.OffsetX;
            Result->Shaped[Idx].OffsetY   = Run.Shaped[Idx].Layout.OffsetY;
            Result->Shaped[Idx].Advance   = Run.Shaped[Idx].Layout.Advance;
            Result->Shaped[Idx].BreakLine = false;

            Result->Shaped[Idx].Source = rect_float(Run.Shaped[Idx].Source.Left , Run.Shaped[Idx].Source.Top,
                                                    Run.Shaped[Idx].Source.Right, Run.Shaped[Idx].Source.Bottom);
        }

        ntext::word_glyph_cursor Cursor  = {.Glyphs = Run.Shaped, .GlyphCount = Run.ShapedCount, .GlyphAt = 0};
        uint32_t                 WordIdx = 0;

        for(ntext::word_slice_node *Node = Analysed.Words.First; Node != 0; Node = Node->Next)
        {
            ntext::word_advance WordAdvance = ntext::AdvanceWord(Cursor, Node->Value);

            Result->Words[WordIdx].Advance                  = WordAdvance.Advance;
            Result->Words[WordIdx].LeadingWhitespaceAdvance = WordAdvance.LeadingWhitespaceAdvance;
            Result->Words[WordIdx].LastGlyph                = Node->Value.Start + Node->Value.Length - 1; // Kind of awkard.

            ++WordIdx;
        }
        Result->WordCount = Analysed.Words.Count;

        UpdateGlyphCache(Font->CacheTransfer, Font->Cache, Run.UpdateList);
    }

    return Result;
}
