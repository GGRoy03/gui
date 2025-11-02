// [Fonts]

internal render_handle
GetFontAtlas(ui_font *Font)
{
    render_handle Result = RenderHandle((u64)Font->GPUContext.GlyphCacheView);
    return Result;
}

internal ui_font *
UILoadFont(byte_string Name, f32 Size)
{
    ui_font *Result = 0;

    // Global Access
    render_handle Renderer = RenderState.Renderer;
    memory_arena *Arena    = UIState.StaticData;
    ui_font_list *FontList = &UIState.Fonts;

    if(IsValidByteString(Name) && Size && IsValidRenderHandle(Renderer))
    {
        vec2_i32 TextureSize = Vec2I32(1024, 1024);
        size_t   Footprint   = (TextureSize.X * sizeof(stbrp_node)) + sizeof(ui_font);

        Result = PushArena(Arena, Footprint, AlignOf(ui_font));

        if(Result)
        {
            Result->Name        = ByteStringCopy(Name, Arena);
            Result->Size        = Size;
            Result->AtlasNodes  = (stbrp_node*)(Result + 1);
            Result->TextureSize = Vec2I32ToVec2F32(TextureSize);

            stbrp_init_target(&Result->AtlasContext, TextureSize.X, TextureSize.Y, Result->AtlasNodes, TextureSize.X);

            if(CreateGlyphCache(Renderer, Result->TextureSize, &Result->GPUContext))
            {
                if(CreateGlyphTransfer(Renderer, Result->TextureSize, &Result->GPUContext))
                {
                    if(OSAcquireFontContext(Name, Size, &Result->GPUContext, &Result->OSContext))
                    {
                        Result->LineHeight = OSGetLineHeight(Result->Size, &Result->OSContext);
                        AppendToLinkedList(FontList, Result, FontList->Count);
                    }
                }
            }

            // WARN: Wonky
            if(!Result->LineHeight)
            {
                ReleaseGlyphCache(&Result->GPUContext);
                ReleaseGlyphTransfer(&Result->GPUContext);
                OSReleaseFontContext(&Result->OSContext);
            }
        }
        else
        {
            ConsoleWriteMessage(error_message("Failed to allocate memory when calling UILoadFont."));
        }
    }
    else
    {
        ConsoleWriteMessage(error_message("One or more arguments given to UILoadFont is invalid."));
    }

    return Result;
}

internal ui_font *
UIQueryFont(byte_string FontName, f32 FontSize)
{
    ui_font *Result = 0;

    ui_font_list *FontList = &UIState.Fonts;
    IterateLinkedList(FontList, ui_font *, Font)
    {
        if (Font->Size == FontSize && ByteStringMatches(Font->Name, FontName, NoFlag))
        {
            Result = Font;
            break;
        }
    }

    return Result;
}

// [Glyphs]

internal glyph_entry *
GetGlyphEntry(glyph_table *Table, u32 Index)
{
    Assert(Index < Table->EntryCount);
    glyph_entry *Result = Table->Entries + Index;
    return Result;
}

internal u32 *
GetSlotPointer(glyph_table *Table, glyph_hash Hash)
{
    u32 HashIndex = Hash.Value;
    u32 HashSlot  = (HashIndex & Table->HashMask);

    Assert(HashSlot < Table->HashCount);
    u32 *Result = &Table->HashTable[HashSlot];

    return Result;
}

internal glyph_entry *
GetSentinel(glyph_table *Table)
{
    glyph_entry *Result = Table->Entries;
    return Result;
}

internal void
UpdateGlyphCacheEntry(glyph_table *Table, glyph_state New)
{
    glyph_entry *Entry = GetGlyphEntry(Table, New.Id);
    if(Entry)
    {
        Entry->IsRasterized = New.IsRasterized;
        Entry->Source       = New.Source;
        Entry->Offset       = New.Offset;
        Entry->AdvanceX     = New.AdvanceX;
        Entry->Position     = New.Position;
        Entry->Size         = New.Size;
    }
}

internal b32
IsValidGlyphRun(ui_glyph_run *Run)
{
    b32 Result = (Run) && (IsValidRenderHandle(Run->Atlas)) && (Run->LineHeight) &&
                 (Run->Shaped) && (Run->ShapedLimit);
    return Result;
}

// -----------------------------------------------------------------------------------
// Glyph Run Public API Implementation

internal void
AlignShapedGlyph(vec2_f32 Position, ui_shaped_glyph *Shaped)
{
    Shaped->Position.Min.X = roundf(Position.X + Shaped->Offset.X);
    Shaped->Position.Min.Y = roundf(Position.Y + Shaped->Offset.Y);
    Shaped->Position.Max.X = roundf(Shaped->Position.Min.X + Shaped->Size.X);
    Shaped->Position.Max.Y = roundf(Shaped->Position.Min.Y + Shaped->Size.Y);
}

internal u64
GetGlyphRunFootprint(u64 BufferSize)
{
    u64 GlyphBufferSize = BufferSize * sizeof(ui_shaped_glyph);
    u64 Result          = sizeof(ui_glyph_run) + GlyphBufferSize;
    return Result;
}

internal ui_glyph_run *
BeginGlyphRun(byte_string Text, u64 BufferSize, ui_font *Font, void *Memory)
{
    Assert(Text.Size <= BufferSize);

    ui_glyph_run *Result   = 0;
    render_handle Renderer = RenderState.Renderer;
    Assert(IsValidRenderHandle(Renderer));

    if(Memory)
    {
        ui_shaped_glyph *Shaped = (ui_shaped_glyph *)Memory;

        Result = (ui_glyph_run *)(Shaped + BufferSize);
        Result->Shaped      = Shaped;
        Result->LineHeight  = Font->LineHeight;
        Result->ShapedCount = 0;
        Result->ShapedLimit = BufferSize;
        Result->Atlas       = GetFontAtlas(Font);
        Result->AtlasSize   = Font->TextureSize;

        while(Result->ShapedCount < Result->ShapedLimit && Text.String[Result->ShapedCount])
        {
            byte_string UTF8Stream = ByteString(&Text.String[Result->ShapedCount], 1);

            glyph_state *State = 0;
            if(IsAsciiString(UTF8Stream))
            {
                State = &Font->Direct[Text.String[Result->ShapedCount]];
            }
            else
            {
                // TODO: Call the cache..
                return Result;
            }

            if(!State->IsRasterized)
            {
                os_glyph_info Info = OSGetGlyphInfo(UTF8Stream, Font->Size, &Font->OSContext);

                f32 Padding     = 2.f;
                f32 HalfPadding = 1.f;

                stbrp_rect STBRect = {0};
                STBRect.w = (u16)(Info.Size.X + Padding);
                STBRect.h = (u16)(Info.Size.Y + Padding);
                stbrp_pack_rects(&Font->AtlasContext, &STBRect, 1);

                if (STBRect.was_packed)
                {
                    rect_f32 PaddedRect;
                    PaddedRect.Min.X = (f32)STBRect.x + HalfPadding;
                    PaddedRect.Min.Y = (f32)STBRect.y + HalfPadding;
                    PaddedRect.Max.X = (f32)(STBRect.x + Info.Size.X + HalfPadding);
                    PaddedRect.Max.Y = (f32)(STBRect.y + Info.Size.Y + HalfPadding);

                    State->IsRasterized = OSRasterizeGlyph(UTF8Stream, PaddedRect, &Font->OSContext);
                    State->Source       = PaddedRect;
                    State->Offset       = Info.Offset;
                    State->AdvanceX     = Info.AdvanceX;
                    State->Size         = Info.Size;

                    TransferGlyph(PaddedRect, Renderer, &Font->GPUContext);
                }
                else
                {
                    ConsoleWriteMessage(warn_message("Could not pack glyph inside the texture in CreateGlyphRun"));
                }
            }

            Result->Shaped[Result->ShapedCount].Source   = State->Source;
            Result->Shaped[Result->ShapedCount].Offset   = State->Offset;
            Result->Shaped[Result->ShapedCount].AdvanceX = State->AdvanceX;
            Result->Shaped[Result->ShapedCount].Size     = State->Size;

            ++Result->ShapedCount;
        }
    }
    else
    {
        ConsoleWriteMessage(warn_message("One or more arguments passed to CreateGlyphRun is invalid"));
    }

    return Result;
}
