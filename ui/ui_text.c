// [Fonts]

// TODO: Allocate the font itself and the sub-components inside the provided arena.

internal ui_font 
UILoadFont(byte_string Name, f32 Size, render_handle BackendHandle, memory_arena *Arena)
{
    ui_font Result = {0};

    gpu_font_objects *GPUFontObjects = PushArena(Arena, sizeof(gpu_font_objects), AlignOf(gpu_font_objects));
    os_font_objects  *OSFontObjects  = PushArena(Arena, sizeof(os_font_objects) , AlignOf(os_font_objects ));
    vec2_i32          TextureSize    = OSGetClientSize(); // WARN: Is that really what we want? Doubt it...

    b32 CreatedGlyphCache = CreateGlyphCache(BackendHandle, TextureSize, GPUFontObjects);
    if (CreatedGlyphCache)
    {
        b32 CreatedGlyphTransfer = CreateGlyphTransfer(BackendHandle, TextureSize, GPUFontObjects);
        if (CreatedGlyphTransfer)
        {
            b32 AcquiredOSFontObjects = OSAcquireFontObjects(Name, Size, GPUFontObjects, OSFontObjects);
            if (AcquiredOSFontObjects)
            {
                Result.GPUObjects.u64[0] = (u64)GPUFontObjects;
                Result.OSObjects.u64[0]  = (u64)OSFontObjects;
                Result.TextureSize       = TextureSize;
            }
            else
            {
                ReleaseGlyphCache(GPUFontObjects);
                ReleaseGlyphTransfer(GPUFontObjects);
                OSLogMessage(byte_string_literal("Failed to acquire OS font objects: See error(s) above."), OSMessage_Warn);
            }
        }
        else
        {
            ReleaseGlyphCache(GPUFontObjects);
            OSLogMessage(byte_string_literal("Failed to create glyph transfer: See error(s) above."), OSMessage_Warn);
        }
    }
    else
    {
        OSLogMessage(byte_string_literal("Failed to create glyph cache: See error(s) above."), OSMessage_Warn);
    }

    return Result;
}

internal b32
UIFontIsValid(ui_font *Font)
{
    b32 Result = 0;

    if (Font)
    {
        Result = (Font->GPUObjects.u64[0] != 0) && (Font->OSObjects.u64[0] != 0) && (!Vec2I32IsEmpty(Font->TextureSize));
    }

    return Result;
}