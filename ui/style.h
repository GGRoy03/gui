#pragma once

enum class StyleProperty : uint32_t
{
    // Layout Properties
    Size        = 0,
    MinSize     = 1,
    MaxSize     = 2,
    Padding     = 3,
    Spacing     = 4,
    BorderWidth = 5,
    Grow        = 6,
    Shrink      = 7,

    // Style Properties
    Color        = 8,
    BorderColor  = 9,
    Softness     = 10,
    CornerRadius = 11,

    // Text Properties
    Font       = 12,
    FontSize   = 13,
    XTextAlign = 14,
    YTextAlign = 15,
    TextColor  = 16,
    CaretColor = 17,
    CaretWidth = 18,

    // Misc
    Count = 19,
    None  = 666,
};

constexpr uint32_t StylePropertyCount = static_cast<uint32_t>(StyleProperty::Count);

typedef enum StyleState_Type
{
    StyleState_Default = 0,
    StyleState_Hovered = 1,
    StyleState_Focused = 2,

    StyleState_None  = 3,
    StyleState_Count = 3,
} StyleState_Type;

// [CORE TYPES]

#define InvalidCachedStyleIndex 0

struct ui_style_properties
{
    // TODO:
    // Size, MinSize, MaxSize, TextAlign

    ui_padding       Padding;
    float            Spacing;
    float            BorderWidth;
    float            Grow;
    float            Shrink;

    ui_color         Color;
    ui_color         BorderColor;
    float            Softness;
    ui_corner_radius CornerRadius;

    ui_font          *Font;
    float             FontSize;
    ui_color          TextColor;
    ui_color          CaretColor;
    float             CaretWidth;
};

struct ui_cached_style
{
    ui_style_properties Properties[StyleState_Count];
};

struct ui_cached_style_node
{
    ui_cached_style_node *Next;
    ui_cached_style       Value;
};

struct ui_cached_style_list
{
    ui_cached_style_node *First;
    ui_cached_style_node *Last;
    uint32_t              Count;
};

struct ui_paint_properties
{
    ui_style_properties Properties;
    uint32_t            CachedIndex;
};

static ui_paint_properties *
GetPaintProperties(uint32_t NodeIndex, ui_subtree *Subtree);

static void
SetNodeStyle(uint32_t NodeIndex, uint32_t StyleIndex, ui_subtree *Subtree);

static void
ClearPaintProperties(uint32_t NodeIndex, ui_subtree *Subtree);

// ===================================================================================
// @Internal: Small Helpers

static bool     IsVisibleColor  (ui_color Color);
static ui_color NormalizeColor  (ui_color Color);
