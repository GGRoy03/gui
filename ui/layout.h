#pragma once

struct ui_layout_tree;


constexpr uint32_t InvalidIndex = 0xFFFFFFFFu; // TODO: HIDE


enum class NodeFlags : uint32_t
{
    None = 0,

    IsDraggable    = 1 << 0, // Node can be dragged by pointer input
    IsResizable    = 1 << 1, // Node can be resized by pointer input

    ClipContent    = 1 << 3, // Child rendering is clipped to this node's bounds
    Indestructible = 1 << 4, // Node cannot be destroyed by normal cleanup paths
};


enum class Alignment : uint32_t
{
    None   = 0,
    Start  = 1,
    Center = 2,
    End    = 3,
};


enum class LayoutDirection : uint32_t
{
    None       = 0,
    Horizontal = 1,
    Vertical   = 2,
};


enum class LayoutSizing : uint32_t
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

// --- Tree Manipulation ---

static memory_footprint GetLayoutTreeFootprint  (uint64_t NodeCount);
static ui_layout_tree * PlaceLayoutTreeInMemory (uint64_t NodeCount, memory_block Block);

// --- Node Manipulation ---

struct parent_node;

static uint32_t CreateNode     (uint64_t Key, NodeFlags Flags, cached_style *Style, ui_layout_tree *Tree);
static bool     AppendChild    (uint32_t ParentIndex, uint32_t ChildIndex, ui_layout_tree *Tree);
static void     PushParent     (uint32_t NodeIndex, ui_layout_tree *Tree, parent_node *Node);
static void     PopParent      (uint32_t NodeIndex, ui_layout_tree *Tree);


// --- Node Queries ---


static uint32_t FindChild      (uint32_t ParentIndex, uint32_t ChildIndex, ui_layout_tree *Tree);


// --- Node Properties ---


static void     UpdateInput    (uint32_t NodeIndex, cached_style *Cached, ui_layout_tree *Tree);
static void     SetNodeOffset  (uint32_t NodeIndex, float X, float Y, ui_layout_tree *Tree);


// -----------------------------------------------------------------------------
// @Private Layout API
//
// These functions form the internal interface used by the UI implementation.
// They are exposed to allow shared access between internal systems, but are
// not considered stable and may change without notice.
//
// Most users should not call these directly.
// -----------------------------------------------------------------------------

// Compute layout for the entire tree.
static void             ComputeTreeLayout       (ui_layout_tree *Tree);

// Pointer / input handling helpers used by the UI pipeline.
static bool             HandlePointerClick      (vec2_float Position, uint32_t ClickMask, uint32_t NodeIndex, ui_layout_tree *Tree);
static bool             HandlePointerRelease    (vec2_float Position, uint32_t ClickMask, uint32_t NodeIndex, ui_layout_tree *Tree);
static bool             HandlePointerHover      (vec2_float Position, uint32_t NodeIndex, ui_layout_tree *Tree);
static void             HandlePointerMove       (vec2_float Delta, ui_layout_tree *Tree);

// Rendering and styling helpers.
// static paint_buffer  GeneratePaintBuffer     (ui_layout_tree *Tree, memory_arena *Arena);
// ------------------------------------------------------------------------------------
// @internal: Layout Resources


struct scroll_region;

struct scroll_region_params
{
    float    PixelPerLine;
    AxisType Axis;
};

static uint64_t        GetScrollRegionFootprint   (void);
static scroll_region * PlaceScrollRegionInMemory  (scroll_region_params Params, void *Memory);
