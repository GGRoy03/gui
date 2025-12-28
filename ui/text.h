// =================================================================
// @Internal: Fonts Implementation

struct font
{
    // render_handle          CacheTransfer;
    // render_handle          Cache;
    // render_handle          CacheView;
    vec2_uint16            TextureSize;
    ntext::glyph_generator Generator;
    uint16_t               Size;
    byte_string            Name;
};

static resource_key UILoadSystemFont  (byte_string Name, float Size, uint16_t CacheSizeX, uint16_t CacheSizeY);

// =================================================================
// @Internal: Static Text Implementation

struct shaped_glyph
{
    float      OffsetX;
    float      OffsetY;
    float      Advance;
    rect_float Source;
    rect_float Position;
    bool       BreakLine;
    bool       Skip;
};

struct text_word
{
    float    LeadingWhitespaceAdvance;
    float    Advance;
    uint64_t FirstGlyph;
    uint64_t LastGlyph;
};

// Do we need to store Texture/TextureSize, since we can always get them from the font?
// And it would also fix the problem of who owns what (the font owns the texture)

struct text
{
    resource_key   FontKey;
    shaped_glyph  *Shaped;
    uint32_t       ShapedCount;
    text_word     *Words;
    uint32_t       WordCount;
};

static uint64_t GetTextFootprint   (ntext::analysed_text Analysed, ntext::shaped_glyph_run Run);
static text *   PlaceTextInMemory  (ntext::analysed_text Analysed, ntext::shaped_glyph_run Run, font *Font, void *Memory);
