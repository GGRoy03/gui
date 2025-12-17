// =================================================================
// @Internal: Fonts Implementation

struct ui_font
{
    render_handle          Texture;
    render_handle          TextureView;
    vec2_uint16            TextureSize;
    ntext::glyph_generator Generator;
    uint16_t               Size;
    byte_string            Name;
};

static ui_resource_key UILoadSystemFont  (byte_string Name, float Size, uint16_t CacheSizeX, uint16_t CacheSizeY);

// =================================================================
// @Internal: Static Text Implementation

struct ui_shaped_glyph
{
    float      OffsetX;
    float      OffsetY;
    float      Advance;
    rect_float Source;
    rect_float Position;
    bool       BreakLine;
};

struct ui_text_word
{
    float Advance;
};

// Do we need to store Texture/TextureSize, since we can always get them from the font?
// And it would also fix the problem of who owns what (the font owns the texture)

struct ui_text
{
    ui_resource_key  FontKey;
    ui_shaped_glyph *Shaped;
    uint32_t         ShapedCount;
    ui_text_word    *Words;
    uint32_t         WordCount;
};

static uint64_t GetTextFootprint    (ntext::analysed_text Analysed, ntext::shaped_glyph_run Run);
static ui_text * PlaceTextInMemory  (ntext::analysed_text Analysed, ntext::shaped_glyph_run Run, ui_font *Font, void *Memory);
