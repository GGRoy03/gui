#pragma once

namespace layout
{

struct layout_tree;


constexpr uint32_t InvalidIndex = 0xFFFFFFFFu; // TODO: HIDE


enum class NodeFlags : uint32_t
{
    None = 0,

    IsDraggable    = 1 << 0, // Node can be dragged by pointer input
    IsResizable    = 1 << 1, // Node can be resized by pointer input

    ClipContent    = 1 << 3, // Child rendering is clipped to this node's bounds
    Indestructible = 1 << 4, // Node cannot be destroyed by normal cleanup paths
};

// --- Tree Manipulation ---

static uint64_t         GetLayoutTreeFootprint  (uint64_t NodeCount);
static layout_tree *    PlaceLayoutTreeInMemory (uint64_t NodeCount, void *Memory);

// --- Node Manipulation ---

struct parent_node;

static uint32_t CreateNode     (uint64_t Key, NodeFlags Flags, const cached_style *Style, layout_tree *Tree);
static bool     AppendChild    (uint32_t ParentIndex, uint32_t ChildIndex, layout_tree *Tree);
static void     PushParent     (uint32_t NodeIndex, layout_tree *Tree, parent_node *Node);
static void     PopParent      (uint32_t NodeIndex, layout_tree *Tree);


// --- Node Queries ---


static uint32_t FindChild      (uint32_t ParentIndex, uint32_t ChildIndex, layout_tree *Tree);


// --- Node Properties ---


static void     UpdateInput    (uint32_t NodeIndex, const cached_style *Cached, layout_tree *Tree);
static void     SetNodeOffset  (uint32_t NodeIndex, float X, float Y, layout_tree *Tree);


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
static void             ComputeTreeLayout       (layout_tree *Tree);

// Pointer / input handling helpers used by the UI pipeline.
static bool             HandlePointerClick      (vec2_float Position, uint32_t ClickMask, uint32_t NodeIndex, layout_tree *Tree);
static bool             HandlePointerRelease    (vec2_float Position, uint32_t ClickMask, uint32_t NodeIndex, layout_tree *Tree);
static bool             HandlePointerHover      (vec2_float Position, uint32_t NodeIndex, layout_tree *Tree);
static void             HandlePointerMove       (vec2_float Delta, layout_tree *Tree);

// Rendering and styling helpers.
// static paint_buffer  GeneratePaintBuffer     (layout_tree *Tree, memory_arena *Arena);
// ------------------------------------------------------------------------------------
// @internal: Layout Resources


struct scroll_region;

struct scroll_region_params
{
    float          PixelPerLine;
    core::AxisType Axis;
};

static uint64_t        GetScrollRegionFootprint   (void);
static scroll_region * PlaceScrollRegionInMemory  (scroll_region_params Params, void *Memory);


} // namespace Layout
