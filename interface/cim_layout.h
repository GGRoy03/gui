#pragma once

#define CimLayout_MaxNestDepth 32
#define CimLayout_InvalidNode 0xFFFFFFFF
#define CimLayout_MaxShapesForDrag 4
#define CimLayout_MaxDragPerBatch 16

typedef enum CimComponent_Flag
{
    CimComponent_Invalid = 0,
    CimComponent_Window  = 1 << 0,
    CimComponent_Button  = 1 << 1,
    CimComponent_Text    = 1 << 2,
} CimComponent_Flag;

typedef enum UIContainer_Type
{
    UIContainer_None   = 0,
    UIContainer_Row    = 1,
    UIContainer_Column = 2,
} UIContainer_Type;

typedef struct cim_window
{
    cim_u32 Nothing;
} cim_window;

typedef struct ui_button
{
    void *Nothing;
} ui_button;

typedef struct cim_text
{
    void *Nothing;
} cim_text;

typedef struct ui_component
{
    theme_id ThemeId;
    
    CimComponent_Flag Type;
    union
    {
        cim_window Window;
        ui_button  Button;
        cim_text   Text;
    } Extra;
} ui_component;

typedef struct ui_component_entry
{
    ui_component Value;
    char         Key[64];
} ui_component_entry;

typedef struct ui_component_hashmap
{
    cim_u8             *Metadata;
    ui_component_entry *Buckets;
    cim_u32             GroupCount;
    bool                IsInitialized;
} ui_component_hashmap;

// NOTE: We could have leaner types? If this grows too much.
typedef struct ui_component_state
{
    bool    Clicked;
    bool    Hovered;
    cim_u32 ActiveIndex;
} ui_component_state;

typedef struct ui_layout_node
{
    // Hierarchy
    cim_u32 Id;
    cim_u32 Parent;
    cim_u32 FirstChild;
    cim_u32 LastChild;
    cim_u32 NextSibling;
    cim_u32 ChildCount;

    // Layout
    UIContainer_Type ContainerType;
    cim_vector2      Spacing;
    cim_vector4      Padding;

    // This is weird. It's used by both the drawer and the layout code.
    cim_f32 X;
    cim_f32 Y;
    cim_f32 Width;
    cim_f32 Height;

    // We mutate the state when hit-testing.
    ui_component_state *State;
} ui_layout_node;

typedef struct ui_layout_node_state
{
    cim_f32 X;
    cim_f32 Y;
    cim_f32 Width;
    cim_f32 Height;
} ui_layout_node_state;

typedef struct ui_layout_tree
{
    // Memory pool
    ui_layout_node *Nodes;
    cim_u32         NodeCount;
    cim_u32         NodeSize;

    // Tree-Logic
    cim_u32 ParentStack[CimLayout_MaxNestDepth];
    cim_u32 AtParent;

    // Tree-State
    ui_draw_list DrawList;
} ui_layout_tree;

typedef struct ui_tree_stats
{
    cim_u32 VisitedNodes;
    cim_u32 LeafCount;
    cim_u32 Depth;
} ui_tree_stats;

typedef struct ui_tree_sizing_result
{
    cim_u32 Stack[256];
    cim_u32 Top;
} ui_tree_sizing_result;

// TODO: I don't know about this. It depends on if we cache trees or not.
typedef struct cim_layout
{
    ui_layout_tree       Tree;       // WARN: Forced to 1 tree for now.
    ui_component_hashmap Components;
} cim_layout;