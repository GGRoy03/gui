#pragma once

// -----------------------------------------------------------------------------
// @Public Layout API
// Functions intended for public consumption.
// -----------------------------------------------------------------------------


constexpr uint32_t InvalidLayoutNodeIndex = 0xFFFFFFFFu;


//
// Flags describing layout node behavior.
//
// These flags control how a node participates in layout, input handling,
// and interaction. Flags may be combined using bitwise operators.
// 


enum class LayoutNodeFlags : uint32_t
{
    None = 0,

    IsDraggable    = 1 << 0, // Node can be dragged by pointer input
    IsResizable    = 1 << 1, // Node can be resized by pointer input

    IsImmediate    = 1 << 2, // Node is only valid for _this_ frame

    ClipContent    = 1 << 3, // Child rendering is clipped to this node's bounds
    Indestructible = 1 << 4, // Node cannot be destroyed by normal cleanup paths
};


//
// Create a layout node.
//
// Allocates a new node in the layout tree and returns its index.
// The returned index is used to build hierarchy, assign styles, nd participate in layout and input.
//
// Typical usage:
//
//     static ui_node
//     UIWindow(uint32_t StyleIndex, ui_pipeline &Pipeline)
//     {
//         ui_node Node = {};
//
//         LayoutNodeFlags Flags =
//             LayoutNodeFlags::ClipContent |
//             LayoutNodeFlags::IsDraggable |
//             LayoutNodeFlags::IsResizable |
//             LayoutNodeFlags::Indestructible;
//
//         uint32_t NodeIndex = UICreateNode(Flags, Pipeline.Tree);
//         if (UIPushLayoutParent(Node.Index, Pipeline.Tree, Pipeline.FrameArena))
//         {
//            Node.Index = NodeIndex;
//            Node.SetStyle(StyleIndex, Pipeline);
//            Node.SetXXX(...);
//            Node.SetYYY(...);
//         }
//         else
//         {
//            // Log/Assert/Recover (This is not done internally, you must handle it)
//         }
//
//         return Node;
//     }
//
// Notes:
// - The node is created even if no parent is pushed; it will exist as a root-level node.
//
// Failure cases -> returns InvalidLayoutNodeIndex:
// - The tree is in an invalid state.
//


static uint32_t UICreateNode    (LayoutNodeFlags Flags, ui_layout_tree *Tree);


//
// Push / Pop a layout parent.
//
// These functions manage the layout parent stack. While a parent is pushed,
// any newly-created nodes become its children. Note that a parent stack is only valid
// for the CURRENT frame and NO OTHER frames.
//
// Typical usage:
//
//     uint32_t Panel = UICreateNode(PanelFlags, Tree);
//
//     if (UIPushLayoutParent(Panel, Tree, Arena))
//     {
//         UICreateNode(ChildFlags, Tree);
//         UICreateNode(ChildFlags, Tree);
//
//         UIPopLayoutParent(Panel, Tree);
//     }
//     else
//     {
//         // Log/Assert/Recover (This is not handled internally)
//     }
//
// Failure cases -> returns false:
// - Push fails if temporary memory cannot be allocated.
// - Pop  fails if Index is not the current parent (mismatched push/pop) or if there is nothing to pop.
// - Both fail  if the tree or the node index are invalid.
//


static bool UIPushLayoutParent  (uint32_t Index, ui_layout_tree *Tree, memory_arena *Arena);
static bool UIPopLayoutParent   (uint32_t Index, ui_layout_tree *Tree);


//
// Find the ChildIndex-th child of ParentIndex.
//
// Example:
//
//     uint32_t Button = UIFindChild(PanelIndex, 0, Tree);
//     if (Button != InvalidLayoutNodeIndex)
//     {
//         // Access or modify the child node
//     }
//     else
//     {
//         // Recover/Log/Assert (This is not handled internally)
//     }
//
//      // You may also use a ui_node for simplicity.
//      ui_node Panel  = UICreateNode(Flags, Tree);
//      ui_node Button = Panel.FindChild(0, Tree);
//      if (ui_node::IsValid(Button))
//      {
//          //...
//      }
//      else
//      {
//          //...
//      }
//
// Failure cases -> returns InvalidLayoutNodeIndex:
// - If the child or the parent does not exist.
// - If the tree is in an invalid state.


static uint32_t UIFindChild(uint32_t ParentIndex, uint32_t ChildIndex, ui_layout_tree *Tree);


//
// Append ChildIndex as a child of ParentIndex.
//
// Potentially expensive checks are done in debug builds to ensure that appending does not
// produce circular layouts.
//
// While it is possible to append the same node to multiple subtrees
// in the same pipeline, it may lead to undefined behavior and is not directly supported.
//
// Consider using Push/Pop if appending a lot of nodes, this is mostly used for rare updates to the tree.
//
// Example:
//
//     uint32_t Panel  = UICreateNode(PanelFlags, Tree);
//     uint32_t Button = UICreateNode(ButtonFlags, Tree);
//
//     if (Panel != UIInvalidLayoutNodeIndex && Button != UIInvalidLayoutNodeIndex)
//     {
//         UIAppendChild(Panel, Button, Tree);
//     }
//     else
//     {
//         // Recover/Log/Assert (This is not handled internally)
//     }
//
//     // You may also use a ui_node for simplicity.
//     ui_node Panel  = UICreateNode(PanelFlags , Tree);
//     ui_node Button = UICreateNode(ButtonFlags, Tree);
//     if(ui_node::IsValid(Panel) && ui_node::IsValid(Button))
//     {
//         Panel.Append(Button, Tree);
//     }
//     else
//     {
//         // Recover/Log/Assert (This is not handled internally)
//     }
//
// Failure cases -> returns false:
// - If the ParentIndex is the same as the ChildIndex.
// - If the tree is in an invalid state.


static bool UIAppendChild(uint32_t ParentIndex, uint32_t ChildIndex, ui_layout_tree *Tree);

// 
// TODO
//

static void UISetOffset(uint32_t NodeIndex, float XOffset, float YOffset, ui_layout_tree *Tree);

// 
// TODO
//

static bool UIIsNodeClicked(uint32_t NodeIndex, ui_layout_tree *Tree);

// 
// See ui/Inspector.cpp to see how these functions are used in practice.
// Note that in most cases we prefer using the ui_node type to make the code more intuitive.
//


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
uint64_t         GetLayoutTreeAlignment  (void);
uint64_t         GetLayoutTreeFootprint  (uint64_t NodeCount);
ui_layout_tree * PlaceLayoutTreeInMemory (uint64_t NodeCount, void *Memory);

// Compute layout for the entire tree.
void             ComputeTreeLayout       (ui_layout_tree *Tree);

// Pointer / input handling helpers used by the UI pipeline.
bool             HandlePointerClick      (vec2_float Position, uint32_t ClickMask, uint32_t NodeIndex, ui_layout_tree *Tree);
bool             HandlePointerRelease    (vec2_float Position, uint32_t ClickMask, uint32_t NodeIndex, ui_layout_tree *Tree);
bool             HandlePointerHover      (vec2_float Position, uint32_t NodeIndex, ui_layout_tree *Tree);
void             HandlePointerMove       (vec2_float Delta, ui_layout_tree *Tree);

// Rendering and styling helpers.
ui_paint_buffer  GeneratePaintBuffer     (ui_layout_tree *Tree, ui_cached_style *Cached, memory_arena *Arena);
void             SetNodeProperties       (uint32_t NodeIndex, uint32_t StyleIndex, const ui_cached_style &Cached, ui_layout_tree *Tree);

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
