constexpr uint32_t InvalidLayoutNodeIndex = 0xFFFFFFFF;

enum class LayoutNodeFlags
{
    None = 0,

    IsDraggable = 1 << 0,
    IsResizable = 1 << 1,

    IsImmediate = 1 << 2, 

    ClipContent = 1 << 3,
};

template<>
struct enable_bitmask_operators<LayoutNodeFlags> : std::true_type {};

static uint64_t         GetLayoutTreeAlignment   (void);
static uint64_t         GetLayoutTreeFootprint   (uint64_t NodeCount);
static ui_layout_tree * PlaceLayoutTreeInMemory  (uint64_t NodeCount, void *Memory);
static uint32_t         AllocateLayoutNode       (LayoutNodeFlags Flags, ui_layout_tree *Tree);
static bool             PushLayoutParent         (uint32_t Index, ui_layout_tree *Tree, memory_arena *Arena);
static bool             PopLayoutParent          (uint32_t Index, ui_layout_tree *Tree);
static void             ComputeTreeLayout        (ui_layout_tree *Tree);

static bool             HandlePointerClick       (vec2_float Position, uint32_t ClickMask, uint32_t NodeIndex, ui_layout_tree *Tree);
static bool             HandlePointerRelease     (vec2_float Position, uint32_t ClickMask, uint32_t NodeIndex, ui_layout_tree *Tree);
static bool             HandlePointerHover       (vec2_float Position, uint32_t NodeIndex, ui_layout_tree *Tree);
static void             HandlePointerMove        (vec2_float Delta, ui_layout_tree *Tree);
static ui_paint_buffer  GeneratePaintBuffer      (ui_layout_tree *Tree, ui_cached_style *Cached, memory_arena *Arena);
static void             SetNodeProperties        (uint32_t NodeIndex, uint32_t StyleIndex, const ui_cached_style &Cached, ui_layout_tree *Tree);

// ------------------------------------------------------------------------------------
// @internal: Tree Queries

static uint32_t UITreeFindChild    (uint32_t ParentIndex, uint32_t ChildIndex, ui_layout_tree *Tree);
static void     UITreeAppendChild  (uint32_t ParentIndex, uint32_t ChildIndex, ui_layout_tree *Tree);
static void     UITreeReserve      (uint32_t NodeIndex  , uint32_t Amount    , ui_layout_tree *Tree);

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
//   Note that you may re-use the same memory with different parameters to modify the behavior with new parameters.
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
    float       PixelPerLine;
    UIAxis_Type Axis;
};

static uint64_t           GetScrollRegionFootprint   (void);
static ui_scroll_region * PlaceScrollRegionInMemory  (scroll_region_params Params, void *Memory);

// ------------------------------------------------------------------------------------

static void ComputeSubtreeLayout  (void *Subtree);
static void UpdateSubtreeState    (void *Subtree);

static void PaintSubtree           (void *Subtree);
