#pragma once


enum class Alignment
{
    None   = 0,
    Start  = 1,
    Center = 2,
    End    = 3,
};


enum class LayoutDirection
{
    None       = 0,
    Horizontal = 1,
    Vertical   = 2,
};


enum class LayoutSizing
{
    None    = 0,
    Fixed   = 1,
    Percent = 2,
    Fit     = 3,
};


struct ui_sizing
{
    float        Value;
    LayoutSizing Type;
};


struct ui_size
{
    ui_sizing Width;
    ui_sizing Height;
};

// TODO: Can we be smarter about what a command really is?
// Right now it's mostly tied to a node in the tree so we need some fat struct.

struct ui_paint_command
{
    rect_float             Rectangle;
    rect_float             RectangleClip;

    core::ui_resource_key  TextKey;
    core::ui_resource_key  ImageKey;

    core::ui_color         Color;
    core::ui_color         BorderColor;
    core::ui_color         TextColor;
    core::ui_corner_radius CornerRadius;
    float                  Softness;
    float                  BorderWidth;
};


struct ui_paint_buffer
{
    ui_paint_command *Commands;
    uint32_t          Size;
};


template<typename T>
struct ui_property
{
    T    Value;
    bool IsSet;

    ui_property() = default;
    constexpr ui_property(const T& Value_)              : Value(Value_), IsSet(true) {}
    constexpr ui_property(const T& Value_, bool IsSet_) : Value(Value_), IsSet(IsSet_) {}

    ui_property& operator=(const T& Value_) { Value = Value_; IsSet = true; return *this; }
};

struct ui_paint_properties
{
    ui_property<core::ui_color>         Color;
    ui_property<core::ui_color>         BorderColor;
    ui_property<core::ui_color>         TextColor;
    ui_property<core::ui_color>         CaretColor;

    ui_property<float>                  BorderWidth;
    ui_property<float>                  Softness;
    ui_property<core::ui_corner_radius> CornerRadius;

    ui_property<float>            CaretWidth;
};

struct ui_default_properties
{
    ui_property<ui_size>                Size;
    ui_property<ui_size>                MinSize;
    ui_property<ui_size>                MaxSize;
    ui_property<LayoutDirection>        Direction;
    ui_property<Alignment>              XAlign;
    ui_property<Alignment>              YAlign;

    ui_property<core::ui_padding>       Padding;
    ui_property<float>                  Spacing;
    ui_property<float>                  Grow;
    ui_property<float>                  Shrink;

    ui_property<core::ui_color>         Color;
    ui_property<core::ui_color>         BorderColor;
    ui_property<core::ui_color>         TextColor;

    ui_property<float>                  BorderWidth;
    ui_property<float>                  Softness;
    ui_property<core::ui_corner_radius> CornerRadius;

    ui_paint_properties MakePaintProperties(void)
    {
        ui_paint_properties Result = {};

        Result.Color        = Color;
        Result.BorderColor  = BorderColor;
        Result.TextColor    = TextColor;
        Result.BorderWidth  = BorderWidth;
        Result.Softness     = Softness;
        Result.CornerRadius = CornerRadius;

        return Result;
    }
};

struct ui_hovered_properties
{
    ui_property<core::ui_color>         Color;
    ui_property<core::ui_color>         BorderColor;
    ui_property<float>                  Softness;
    ui_property<core::ui_corner_radius> CornerRadius;

    ui_paint_properties InheritPaintProperties(const ui_paint_properties &Default)
    {
        ui_paint_properties Result = Default;

        if (Color.IsSet)        Result.Color        = Color;
        if (BorderColor.IsSet)  Result.BorderColor  = BorderColor;
        if (Softness.IsSet)     Result.Softness     = Softness;
        if (CornerRadius.IsSet) Result.CornerRadius = CornerRadius;

        return Result;
    }
};

struct ui_focused_properties
{
    ui_property<core::ui_color>         Color;
    ui_property<core::ui_color>         BorderColor;
    ui_property<core::ui_color>         TextColor;
    ui_property<core::ui_color>         CaretColor;
    ui_property<float>                  CaretWidth;
    ui_property<float>                  Softness;
    ui_property<core::ui_corner_radius> CornerRadius;

    ui_paint_properties InheritPaintProperties(const ui_paint_properties &Default)
    {
        ui_paint_properties Result = Default;

        if (Color.IsSet)        Result.Color        = Color;
        if (BorderColor.IsSet)  Result.BorderColor  = BorderColor;
        if (TextColor.IsSet)    Result.TextColor    = TextColor;
        if (CaretColor.IsSet)   Result.CaretColor   = CaretColor;
        if (CaretWidth.IsSet)   Result.CaretWidth   = CaretWidth;
        if (Softness.IsSet)     Result.Softness     = Softness;
        if (CornerRadius.IsSet) Result.CornerRadius = CornerRadius;

        return Result;
    }
};

struct ui_cached_style
{
    ui_default_properties Default;
    ui_hovered_properties Hovered;
    ui_focused_properties Focused;
};

static void ExecutePaintCommands(ui_paint_buffer Buffer, memory_arena *Arena);
