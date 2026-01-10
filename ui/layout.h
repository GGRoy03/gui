#pragma once

#include <stdint.h>

// =============================================================================
// Forward Declarations
// =============================================================================


typedef struct gui_layout_tree   gui_layout_tree;
typedef struct gui_parent_node   gui_parent_node;
typedef struct gui_cached_style  gui_cached_style;
typedef struct gui_scroll_region gui_scroll_region;


// =============================================================================
// Constants
// =============================================================================


static const uint32_t GuiInvalidIndex = 0xFFFFFFFFu; // TODO: HIDE


// =============================================================================
// Enums
// =============================================================================


typedef enum Gui_NodeFlags
{
    Gui_NodeFlags_None            = 0,
    Gui_NodeFlags_IsDraggable     = 1 << 0,
    Gui_NodeFlags_IsResizable     = 1 << 1,
    Gui_NodeFlags_ClipContent     = 1 << 3,
    Gui_NodeFlags_Indestructible  = 1 << 4,
} Gui_NodeFlags;


typedef enum Gui_Alignment
{
    Gui_Alignment_None   = 0,
    Gui_Alignment_Start  = 1,
    Gui_Alignment_Center = 2,
    Gui_Alignment_End    = 3,
} Gui_Alignment;


typedef enum Gui_LayoutDirection
{
    Gui_LayoutDirection_None       = 0,
    Gui_LayoutDirection_Horizontal = 1,
    Gui_LayoutDirection_Vertical   = 2,
} Gui_LayoutDirection;


typedef enum Gui_LayoutSizing
{
    Gui_LayoutSizing_None    = 0,
    Gui_LayoutSizing_Fixed   = 1,
    Gui_LayoutSizing_Percent = 2,
    Gui_LayoutSizing_Fit     = 3,
} Gui_LayoutSizing;


// =============================================================================
// Structs
// =============================================================================


typedef struct gui_sizing
{
    float             Value;
    Gui_LayoutSizing  Type;
} gui_sizing;


typedef struct gui_size
{
    gui_sizing  Width;
    gui_sizing  Height;
} gui_size;


// =============================================================================
// Tree Manipulation
// =============================================================================

static gui_memory_footprint GuiGetLayoutTreeFootprint   (uint64_t NodeCount);
static gui_layout_tree    * GuiPlaceLayoutTreeInMemory  (uint64_t NodeCount, gui_memory_block Block);
static void                 GuiComputeTreeLayout        (gui_layout_tree *Tree);

// =============================================================================
// Node Manipulation
// =============================================================================

static uint32_t GuiCreateNode   (uint64_t Key, uint32_t Flags, gui_layout_tree *Tree);
static gui_bool GuiAppendChild  (uint32_t ParentIndex, uint32_t ChildIndex, gui_layout_tree *Tree);
static void     GuiPushParent   (uint32_t NodeIndex, gui_layout_tree *Tree, gui_parent_node *Node);
static void     GuiPopParent    (uint32_t NodeIndex, gui_layout_tree *Tree);

// =============================================================================
// Node Queries
// =============================================================================

static uint32_t GuiFindChild  (uint32_t ParentIndex, uint32_t ChildIndex, gui_layout_tree *Tree);

// =============================================================================
// Tree Queries
// =============================================================================

static gui_bool GuiHandlePointerClick    (gui_point Position, uint32_t ClickMask, uint32_t NodeIndex, gui_layout_tree *Tree);
static gui_bool GuiHandlePointerRelease  (gui_point Position, uint32_t ClickMask, uint32_t NodeIndex, gui_layout_tree *Tree);
static gui_bool GuiHandlePointerHover    (gui_point Position, uint32_t NodeIndex, gui_layout_tree *Tree);
static void     GuiHandlePointerMove     (float DeltaX, float DeltaY, gui_layout_tree *Tree);

// =============================================================================
// Node Properties
// =============================================================================

static void GuiUpdateInput    (uint32_t NodeIndex, gui_cached_style *Cached, gui_layout_tree *Tree);
static void GuiSetNodeOffset  (uint32_t NodeIndex, gui_dimensions Offset, gui_layout_tree *Tree);

// =============================================================================
// @internal: Layout Resources
// =============================================================================

typedef struct gui_scroll_region_params
{
    float        PixelPerLine;
    Gui_AxisType Axis;
} gui_scroll_region_params;


static uint64_t            GuiGetScrollRegionFootprint   (void);
static gui_scroll_region * GuiPlaceScrollRegionInMemory  (gui_scroll_region_params Params, void *Memory);
