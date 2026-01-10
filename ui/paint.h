#pragma once

#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// Style Property
// =============================================================================


typedef struct gui_style_property_float
{
    float Value;
    bool  IsSet;
} gui_style_property_float;


typedef struct gui_style_property_size
{
    gui_size Value;
    bool     IsSet;
} gui_style_property_size;


typedef struct gui_style_property_color
{
    gui_color Value;
    bool      IsSet;
} gui_style_property_color;


typedef struct gui_style_property_corner_radius
{
    gui_corner_radius Value;
    bool              IsSet;
} gui_style_property_corner_radius;


typedef struct gui_style_property_padding
{
    gui_padding Value;
    bool        IsSet;
} gui_style_property_padding;


typedef struct gui_style_property_alignment
{
    Gui_Alignment Value;
    bool          IsSet;
} gui_style_property_alignment;


typedef struct gui_style_property_layout_direction
{
    Gui_LayoutDirection Value;
    bool                IsSet;
} gui_style_property_layout_direction;


// =============================================================================
// Layout Properties
// =============================================================================


typedef struct gui_layout_properties
{
    gui_style_property_size             Size;
    gui_style_property_size             MinSize;
    gui_style_property_size             MaxSize;

    gui_style_property_layout_direction Direction;
    gui_style_property_alignment        XAlign;
    gui_style_property_alignment        YAlign;

    gui_style_property_padding          Padding;
    gui_style_property_float            Spacing;

    gui_style_property_float            Grow;
    gui_style_property_float            Shrink;
} gui_layout_properties;


// =============================================================================
// Paint Properties
// =============================================================================


typedef struct gui_paint_default_properties
{
    gui_style_property_color         Color;
    gui_style_property_color         BorderColor;
    gui_style_property_color         TextColor;

    gui_style_property_float         BorderWidth;
    gui_style_property_float         Softness;
    gui_style_property_corner_radius CornerRadius;
} gui_paint_default_properties;


typedef struct gui_paint_hovered_properties
{
    gui_style_property_color          Color;
    gui_style_property_color          BorderColor;
    gui_style_property_color          TextColor;

    gui_style_property_float          BorderWidth;
    gui_style_property_float          Softness;
    gui_style_property_corner_radius  CornerRadius;
} gui_paint_hovered_properties;


typedef struct gui_paint_focused_properties
{
    gui_style_property_color          Color;
    gui_style_property_color          BorderColor;
    gui_style_property_float          BorderWidth;
    gui_style_property_color          TextColor;
    gui_style_property_color          CaretColor;
    gui_style_property_float          CaretWidth;
    gui_style_property_float          Softness;
    gui_style_property_corner_radius  CornerRadius;
} gui_paint_focused_properties;


typedef struct gui_paint_properties
{
    gui_color         Color;
    gui_color         HoveredColor;
    gui_color         FocusedColor;

    gui_color         BorderColor;
    gui_color         HoveredBorderColor;
    gui_color         FocusedBorderColor;

    float             BorderWidth;
    float             HoveredBorderWidth;
    float             FocusedBorderWidth;

    gui_color         TextColor;
    gui_color         HoveredTextColor;
    gui_color         FocusedTextColor;

    gui_color         CaretColor;
    float             CaretWidth;

    gui_corner_radius CornerRadius;
    gui_corner_radius HoveredCornerRadius;
    gui_corner_radius FocusedCornerRadius;

    float             Softness;
    float             HoveredSoftness;
    float             FocusedSoftness;
} gui_paint_properties;


typedef struct gui_cached_style
{
    gui_layout_properties        Layout;
    gui_paint_default_properties Default;
    gui_paint_hovered_properties Hovered;
    gui_paint_focused_properties Focused;
} gui_cached_style;


// =============================================================================
// Render Commands
// =============================================================================


typedef enum Gui_RenderCommandType
{
    Gui_RenderCommandType_None      = 0,
    Gui_RenderCommandType_Rectangle = 1,
    Gui_RenderCommandType_Border    = 2,
    Gui_RenderCommandType_Text      = 3,
} GuiRenderCommandType;


typedef struct gui_render_command
{
    GuiRenderCommandType Type;
    gui_bounding_box     Box;

    union
    {
        struct
        {
            gui_color         Color;
            gui_corner_radius CornerRadius;
        } Rect;

        struct
        {
            gui_corner_radius CornerRadius;
            gui_color         Color;
            float             Width;
        } Border;

        struct
        {
            void             *Texture;
            gui_color         Color;
            gui_bounding_box  Source;
        } Text;
    };
} gui_render_command;


typedef struct gui_render_command_list
{
    gui_render_command *Commands;
    uint32_t            Count;
} gui_render_command_list;


static gui_memory_footprint    GuiGetRenderCommandsFootprint  (gui_layout_tree *Tree);
static gui_render_command_list GuiComputeRenderCommands       (gui_layout_tree *Tree, void *Memory);
