// [CORE TYPES]

typedef struct ui_font
{
    // Backend
    render_handle GPUObjects;
    os_handle     OSObjects;

    // Info
    vec2_i32 TextureSize;
} ui_font;


// [API]

internal ui_font UILoadFont(byte_string Name, f32 Size, render_handle BackendHandle, memory_arena *Arena);
internal b32     UIFontIsValid(ui_font *Font);