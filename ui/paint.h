#pragma once

namespace gui
{

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


struct layout_properties
{
    style_property<size>            Size;
    style_property<size>            MinSize;
    style_property<size>            MaxSize;

    style_property<LayoutDirection> Direction;
    style_property<Alignment>       XAlign;
    style_property<Alignment>       YAlign;

    style_property<padding>         Padding;
    style_property<float>           Spacing;

    style_property<float>           Grow;
    style_property<float>           Shrink;
};


struct paint_default_properties
{
    style_property<color>         Color;
    style_property<color>         BorderColor;
    style_property<color>         TextColor;

    style_property<float>         BorderWidth;
    style_property<float>         Softness;
    style_property<corner_radius> CornerRadius;
};


struct paint_hovered_properties
{
    style_property<color>         Color;
    style_property<color>         BorderColor;
    style_property<color>         TextColor;

    style_property<float>         BorderWidth;
    style_property<float>         Softness;
    style_property<corner_radius> CornerRadius;
};


struct paint_focused_properties
{   
    style_property<color>           Color;
    style_property<color>           BorderColor;
    style_property<float>           BorderWidth;
    style_property<color>           TextColor;
    style_property<color>           CaretColor;
    style_property<float>           CaretWidth;
    style_property<float>           Softness;
    style_property<corner_radius>   CornerRadius;
};


struct cached_style
{
    layout_properties        Layout;
    paint_default_properties Default;
    paint_hovered_properties Hovered;
    paint_focused_properties Focused;
};


struct paint_properties
{
    color         Color;
    color         HoveredColor;
    color         FocusedColor;

    color         BorderColor;
    color         HoveredBorderColor;
    color         FocusedBorderColor;

    float         BorderWidth;
    float         HoveredBorderWidth;
    float         FocusedBorderWidth;

    color         TextColor;
    color         HoveredTextColor;
    color         FocusedTextColor;

    color         CaretColor;
    float         CaretWidth;

    corner_radius CornerRadius;
    corner_radius HoveredCornerRadius;
    corner_radius FocusedCornerRadius;

    float         Softness;
    float         HoveredSoftness;
    float         FocusedSoftness;
};


enum class RenderCommandType
{
    None = 0,

    Rectangle = 1,
    Border    = 2,
    Text      = 3,
};

struct render_command
{
    RenderCommandType Type;
    bounding_box      Box;

    union
    {
        struct
        {
            color         Color;
            corner_radius CornerRadius;
        } Rect;

        struct
        {
            corner_radius CornerRadius;
            color         Color;
            float         Width;
        } Border;

        struct
        {
            void        *Texture;
            color        Color;
            bounding_box Source;
        } Text;
    };
};


struct render_command_list
{
    render_command *Commands;
    uint32_t        Count;
};


static memory_footprint    GetRenderCommandsFootprint  (ui_layout_tree *Tree);
static render_command_list ComputeRenderCommands       (ui_layout_tree *Tree, void *Memory);

} // namespace gui
