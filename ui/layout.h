#pragma once

namespace layout
{

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

// --- Node Manipulation ---

static uint32_t CreateNode     (NodeFlags Flags, ui_layout_tree *Tree);
static bool     AppendChild    (uint32_t ParentIndex, uint32_t ChildIndex, ui_layout_tree *Tree);

// --- Node Queries ---

static uint32_t FindChild      (uint32_t ParentIndex, uint32_t ChildIndex, ui_layout_tree *Tree);

// --- Node Properties ---

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

// Memory and construction helpers for layout trees.
static uint64_t         GetLayoutTreeAlignment  (void);
static uint64_t         GetLayoutTreeFootprint  (uint64_t NodeCount);
static ui_layout_tree * PlaceLayoutTreeInMemory (uint64_t NodeCount, void *Memory);
static void             PrepareTreeForFrame     (ui_layout_tree *Tree);

// Compute layout for the entire tree.
static void             ComputeTreeLayout       (ui_layout_tree *Tree);

// Pointer / input handling helpers used by the UI pipeline.
static bool             HandlePointerClick      (vec2_float Position, uint32_t ClickMask, uint32_t NodeIndex, ui_layout_tree *Tree);
static bool             HandlePointerRelease    (vec2_float Position, uint32_t ClickMask, uint32_t NodeIndex, ui_layout_tree *Tree);
static bool             HandlePointerHover      (vec2_float Position, uint32_t NodeIndex, ui_layout_tree *Tree);
static void             HandlePointerMove       (vec2_float Delta, ui_layout_tree *Tree);

// Rendering and styling helpers.
static ui_paint_buffer  GeneratePaintBuffer     (ui_layout_tree *Tree, ui_cached_style *Cached, memory_arena *Arena);
static void             UpdateLayoutInput       (uint32_t NodeIndex, uint32_t StyleIndex, const ui_cached_style &Cached, ui_layout_tree *Tree);

// ------------------------------------------------------------------------------------
// @internal: Layout Resources
//
// ui_scroll_region:
//  Opaque pointers the user shouldn't care about.
//
// scroll_region_params:
//  Parameters structure used when calling PlaceScrollRegionInMemory.
//  PixelPerLine: Specifies the speed at which the content will scroll.
//  Axis:         Specifies the axis along which the content scrolls.
//
// GetScrollRegionFootprint & PlaceScrollRegionInMemory
//   Used to initilialize in memory a scroll region. You may specify parameters to modify the behavior of the scroll region.
//   Note that you may re-use the same memory with different parameters to modify the scroll-region.
//
//   Example Usage:
//   uint64_t   Size   = GetScrollRegionFootprint(); -> Get the size needed to allocate
//   void *Memory = malloc(Size);               -> Allocate (Do not check for null yet!)
//
//   scroll_region_params Params = {.PixelPerLine = ScrollSpeed, .Axis = Axis};  -> Prepare the params
//   ui_scroll_region *ScrollRegion = PlaceScrollRegionInMemory(Params, Memory); -> Initialize
//   if(ScrollRegion)                                                            -> Now check if it succeeded

struct ui_scroll_region;

struct scroll_region_params
{
    float          PixelPerLine;
    core::AxisType Axis;
};

static uint64_t           GetScrollRegionFootprint   (void);
static ui_scroll_region * PlaceScrollRegionInMemory  (scroll_region_params Params, void *Memory);


} // namespace Layout
