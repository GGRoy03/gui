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


struct sizing
{
    float        Value;
    LayoutSizing Type;
};


struct size
{
    sizing Width;
    sizing Height;
};

// TODO: Can we be smarter about what a command really is?
// Right now it's mostly tied to a node in the tree so we need some fat struct.

struct paint_command
{
    rect_float             Rectangle;
    rect_float             RectangleClip;

    core::resource_key  TextKey;
    core::resource_key  ImageKey;

    core::color         Color;
    core::color         BorderColor;
    core::color         TextColor;
    core::corner_radius CornerRadius;
    float                  Softness;
    float                  BorderWidth;
};


struct paint_buffer
{
    paint_command *Commands;
    uint32_t          Size;
};


template<typename T>
struct style_property
{
    T    Value;
    bool IsSet;

    style_property() = default;
    constexpr style_property(const T& Value_)              : Value(Value_), IsSet(true) {}
    constexpr style_property(const T& Value_, bool IsSet_) : Value(Value_), IsSet(IsSet_) {}

    style_property& operator=(const T& Value_) { Value = Value_; IsSet = true; return *this; }
};

struct paint_properties
{
    style_property<core::color>         Color;
    style_property<core::color>         BorderColor;
    style_property<core::color>         TextColor;
    style_property<core::color>         CaretColor;

    style_property<float>                  BorderWidth;
    style_property<float>                  Softness;
    style_property<core::corner_radius> CornerRadius;

    style_property<float>            CaretWidth;
};

struct default_properties
{
    style_property<size>                Size;
    style_property<size>                MinSize;
    style_property<size>                MaxSize;
    style_property<LayoutDirection>        Direction;
    style_property<Alignment>              XAlign;
    style_property<Alignment>              YAlign;

    style_property<core::padding>       Padding;
    style_property<float>                  Spacing;
    style_property<float>                  Grow;
    style_property<float>                  Shrink;

    style_property<core::color>         Color;
    style_property<core::color>         BorderColor;
    style_property<core::color>         TextColor;

    style_property<float>                  BorderWidth;
    style_property<float>                  Softness;
    style_property<core::corner_radius> CornerRadius;

    paint_properties MakePaintProperties(void) const
    {
        paint_properties Result = {};

        Result.Color        = Color;
        Result.BorderColor  = BorderColor;
        Result.TextColor    = TextColor;
        Result.BorderWidth  = BorderWidth;
        Result.Softness     = Softness;
        Result.CornerRadius = CornerRadius;

        return Result;
    }
};

struct hovered_properties
{
    style_property<core::color>         Color;
    style_property<core::color>         BorderColor;
    style_property<float>                  Softness;
    style_property<core::corner_radius> CornerRadius;

    paint_properties InheritPaintProperties(const paint_properties &Default) const
    {
        paint_properties Result = Default;

        if (Color.IsSet)        Result.Color        = Color;
        if (BorderColor.IsSet)  Result.BorderColor  = BorderColor;
        if (Softness.IsSet)     Result.Softness     = Softness;
        if (CornerRadius.IsSet) Result.CornerRadius = CornerRadius;

        return Result;
    }
};

struct focused_properties
{
    style_property<core::color>         Color;
    style_property<core::color>         BorderColor;
    style_property<core::color>         TextColor;
    style_property<core::color>         CaretColor;
    style_property<float>               CaretWidth;
    style_property<float>               Softness;
    style_property<core::corner_radius> CornerRadius;

    paint_properties InheritPaintProperties(const paint_properties &Default) const
    {
        paint_properties Result = Default;

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

struct cached_style
{
    default_properties Default;
    hovered_properties Hovered;
    focused_properties Focused;
};

// static void ExecutePaintCommands(paint_buffer Buffer, memory_arena *Arena);
