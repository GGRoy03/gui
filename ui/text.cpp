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
        ui_font  *Font      = static_cast<ui_font *>(malloc(Footprint));

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

            Font->Generator   = ntext::CreateGlyphGenerator(GeneratorParams);
            Font->Texture     = CreateRenderTexture(CacheSizeX, CacheSizeY, RenderTexture::RGBA);
            Font->TextureView = CreateRenderTextureView(Font->Texture, RenderTexture::RGBA);
            Font->TextureSize = vec2_uint16(CacheSizeX, CacheSizeY);
            Font->Size        = Size;
            Font->Name        = Name;

            UpdateResourceTable(State.Id, Key, Font, Context.ResourceTable);
        }
    }

    return Key;
}

// =================================================================
// @Internal: Static Text Implementation

static uint64_t
GetTextFootprint(ntext::analysed_text Analysed, ntext::shaped_glyph_run Run)
{
    uint64_t ShapedBufferSize = Analysed.CodepointCount * sizeof(ui_shaped_glyph);
    uint64_t WordBufferSize   = Analysed.Words.Count    * sizeof(ui_text_word);
    uint64_t Result           = ShapedBufferSize + WordBufferSize + sizeof(ui_text);

    return Result;
}

// This is a mess. The problem is the allocation. It changes behavior depending
// on whether or not we have found words in the string. What do I actually need?

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
            Words  = reinterpret_cast<ui_text_word *>(Shaped + Analysed.CodepointCount);
            Result = reinterpret_cast<ui_text      *>(Words  + Analysed.Words.Count);
        }
        else
        {
            Result = reinterpret_cast<ui_text      *>(Shaped + Analysed.CodepointCount);
        }

        VOID_ASSERT(Result);

        Result->Shaped      = Shaped;
        Result->Words       = Words;
        Result->FontKey     = MakeFontResourceKey(Font->Name, Font->Size);
        Result->ShapedCount = Run.ShapedCount;

        for(uint32_t Idx = 0; Idx < Run.ShapedCount; ++Idx)
        {
            Result->Shaped[Idx].OffsetX = Run.Shaped[Idx].Layout.OffsetX;
            Result->Shaped[Idx].OffsetY = Run.Shaped[Idx].Layout.OffsetY;
            Result->Shaped[Idx].Advance = Run.Shaped[Idx].Layout.Advance;

            Result->Shaped[Idx].Source = rect_float(Run.Shaped[Idx].Source.Left , Run.Shaped[Idx].Source.Top,
                                                    Run.Shaped[Idx].Source.Right, Run.Shaped[Idx].Source.Bottom);
        }

        UpdateGlyphCache(Font->Texture, Run.UpdateList);
    }

    return Result;
}
