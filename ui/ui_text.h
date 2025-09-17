// [CONSTANTS]

typedef enum UIFontCoverage_Type
{
    UIFontCoverage_None      = 0,
    UIFontCoverage_ASCIIOnly = 1,
} UIFontCoverage_Type;

// [CORE TYPES]

typedef struct glyph_rasterize_info
{
    b32           IsRasterized;
    texture_coord TextureCoord;
} glyph_rasterize_info;

typedef struct glyph_state
{
    os_glyph_layout      Layout;
    glyph_rasterize_info RasterizeInfo;
} glyph_state;

typedef struct glyph_table_params
{
    u32 EntryCount;
} glyph_table_params;

typedef struct glyph_table_stats
{
    u64 HitCount;
    u64 MissCount;
} glyph_table_stats;

typedef struct direct_glyph_table
{
    glyph_table_stats Stats;

    glyph_state *Entries;
    u32          EntryCount;
} direct_glyph_table;

typedef struct ui_font
{
    // Backend
    gpu_font_objects GPUFontObjects;
    os_font_objects  OSFontObjects;
    memory_arena    *Arena;

    // Info
    vec2_i32 TextureSize;
    f32      LineHeight;
    f32      Size;

    // Tables
    direct_glyph_table *GlyphTable;
} ui_font;

// [API]

// [Fonts]

internal ui_font * UILoadFont  (byte_string Name, f32 Size, render_handle BackendHandle, UIFontCoverage_Type Coverage);

// [Glyphs]

internal size_t               GetDirectGlyphTableFootprint   (glyph_table_params Params);
internal glyph_state          FindGlyphEntryByDirectAccess   (u32 CodePoint, direct_glyph_table *Table);
internal direct_glyph_table * PlaceDirectGlyphTableInMemory  (glyph_table_params Params, void *Memory);