// [CONSTANTS]

typedef enum UILayoutNode_Flag
{
    UILayoutNode_NoFlag                  = 0,
    UILayoutNode_PlaceChildrenVertically = 1 << 0,
    UILayoutNode_DrawBackground          = 1 << 1,
    UILayoutNode_DrawBorders             = 1 << 2,
    UILayoutNode_DrawText                = 1 << 3,
    UILayoutNode_HasClip                 = 1 << 4,
    UILayoutNode_IsHovered               = 1 << 5,
    UILayoutNode_IsClicked               = 1 << 6,
    UILayoutNode_IsDraggable             = 1 << 7,
    UILayoutNode_IsResizable             = 1 << 8,
} UILayoutNode_Flag;

typedef enum UIHover_State
{
    UIHover_None         = 0,
    UIHover_Target       = 1,
    UIHover_ResizeX      = 2,
    UIHover_ResizeY      = 3,
    UIHover_ResizeCorner = 4,
} UIHover_State;

typedef enum UIHitTest_Flag
{
    UIHitTest_NoFlag         = 0,
    UIHitTest_CheckForResize = 1 << 0,
} UIHitTest_Flag;

// [CORE TYPES]

DEFINE_TYPED_QUEUE(Node, node, ui_node *);

typedef struct ui_hit_test_result
{
    ui_node      *LayoutNode;
    UIHover_State HoverState;
    b32           Success;
} ui_hit_test_result;

typedef struct ui_layout_box
{
    // Output
    f32 FinalX;
    f32 FinalY;
    f32 FinalWidth;
    f32 FinalHeight;

    // Layout Info
    ui_unit    Width;
    ui_unit    Height;
    ui_spacing Spacing;
    ui_padding Padding;

    // Params
    rect_f32   Clip;
    matrix_3x3 Transform;

    // Misc
    bit_field Flags;

    // Misc (Should text be callback based?)
    ui_text           *Text;
    ui_click_callback *ClickCallback;
} ui_layout_box;

// [CORE API]

internal ui_hit_test_result UILayout_HitTest  (vec2_f32 MousePosition, bit_field Flags, ui_node *LayoutRoot, ui_pipeline *Pipeline);

internal void UILayout_DragSubtree    (vec2_f32 Delta, ui_node *LayoutRoot, ui_pipeline *Pipeline);
internal void UILayout_ResizeSubtree  (vec2_f32 Delta, ui_node *LayoutNode, ui_pipeline *Pipeline);

internal void UILayout_ComputeParentToChildren  (ui_node *LayoutRoot, ui_pipeline *Pipeline);
