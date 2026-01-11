#pragma once


#include <stdint.h>
#include <immintrin.h>


// =============================================================================
// Forward Declarations
// =============================================================================


typedef struct gui_layout_tree    gui_layout_tree;
typedef struct gui_parent_node    gui_parent_node;
typedef struct gui_cached_style   gui_cached_style;
typedef struct gui_font           gui_font;
typedef struct gui_resource_table gui_resource_table;


// =============================================================================
// DOMAIN: Basic Types
// =============================================================================


typedef enum Gui_AxisType
{
    Gui_AxisType_None = 0,
    Gui_AxisType_X    = 1,
    Gui_AxisType_Y    = 2,
    Gui_AxisType_XY   = 3,
} Gui_AxisType;


typedef struct gui_color
{
    float R;
    float G;
    float B;
    float A;
} gui_color;


typedef struct gui_corner_radius
{
    float TL;
    float TR;
    float BR;
    float BL;
} gui_corner_radius;


typedef struct gui_padding
{
    float Left;
    float Top;
    float Right;
    float Bot;
} gui_padding;


// =============================================================================
// DOMAIN: Strings
// =============================================================================


typedef struct gui_byte_string
{
    char    *String;
    uint64_t Size;
} gui_byte_string;


#define gui_str8_lit(String)   GuiByteString((char *)(String), sizeof(String) - 1)
#define gui_str8_comp(String)  ((gui_byte_string){ (char *)(String), sizeof(String) - 1 })


static gui_byte_string GuiByteString(char *String, uint64_t Size);
static gui_bool        GuiIsValidByteString(gui_byte_string Input);
static uint64_t        GuiHashByteString(gui_byte_string Input);


// =============================================================================
// DOMAIN: Memory
// =============================================================================


typedef struct gui_memory_footprint
{
    uint64_t SizeInBytes;
    uint64_t Alignment;
} gui_memory_footprint;


typedef struct gui_memory_block
{
    uint64_t SizeInBytes;
    void    *Base;
} gui_memory_block;


typedef struct gui_memory_region
{
    void    *Base;
    uint64_t Size;
    uint64_t At;
} gui_memory_region;





#define GuiPushArrayNoZeroAligned(Region, Type, Count, Align) ((Type *)GuiPushMemoryRegion((Region), sizeof(Type) * (Count), (Align)))
#define GuiPushArrayAligned(Region, Type, Count, Align)                GuiPushArrayNoZeroAligned((Region), Type, (Count), (Align))
#define GuiPushArray(Region, Type, Count)                              GuiPushArrayAligned((Region), Type, (Count), _Alignof(Type))
#define GuiPushStruct(Region, Type)                                    GuiPushArray((Region), Type, 1)

// =============================================================================
// DOMAIN: Resources
// =============================================================================


typedef enum Gui_ResourceType
{
    Gui_ResourceType_None         = 0,
    Gui_ResourceType_Text         = 1,
    Gui_ResourceType_TextInput    = 2,
    Gui_ResourceType_ScrollRegion = 3,
    Gui_ResourceType_Image        = 4,
    Gui_ResourceType_ImageGroup   = 5,
    Gui_ResourceType_Font         = 6,
} Gui_ResourceType;


typedef enum Gui_FindResourceFlag
{
    Gui_FindResourceFlag_None          = 0,
    Gui_FindResourceFlag_AddIfNotFound = 1 << 0,
} Gui_FindResourceFlag;


typedef struct gui_resource_key
{
    __m128i Value;
} gui_resource_key;


typedef struct gui_resource_stats
{
    uint64_t CacheHitCount;
    uint64_t CacheMissCount;
} gui_resource_stats;


typedef struct gui_resource_table_params
{
    uint32_t HashSlotCount;
    uint32_t EntryCount;
} gui_resource_table_params;


typedef struct gui_resource_state
{
    uint32_t          Id;
    Gui_ResourceType  ResourceType;
    void             *Resource;
} gui_resource_state;


static gui_memory_footprint   GuiGetResourceTableFootprint   (gui_resource_table_params Params);
static gui_resource_table   * GuiPlaceResourceTableInMemory  (gui_resource_table_params Params, gui_memory_block Block);

static gui_resource_key       GuiMakeNodeResourceKey         (Gui_ResourceType Type, uint32_t NodeIndex, gui_layout_tree *Tree);
static gui_resource_state     GuiFindResourceByKey           (gui_resource_key Key, Gui_FindResourceFlag Flags, gui_resource_table *Table);
static void                   GuiUpdateResourceTable         (uint32_t Id, gui_resource_key Key, void *Resource, gui_resource_table *Table);


// =============================================================================
// DOMAIN: Pointer Input
// =============================================================================


typedef enum Gui_PointerSource
{
    Gui_PointerSource_None       = 0,
    Gui_PointerSource_Mouse      = 1,
    Gui_PointerSource_Touch      = 2,
    Gui_PointerSource_Pen        = 3,
    Gui_PointerSource_Controller = 4,
} Gui_PointerSource;


typedef enum Gui_PointerButton
{
    Gui_PointerButton_None      = 0,
    Gui_PointerButton_Primary   = 1,
    Gui_PointerButton_Secondary = 2,
} Gui_PointerButton;


typedef enum Gui_PointerEvent
{
    Gui_PointerEvent_None    = 0,
    Gui_PointerEvent_Move    = 1,
    Gui_PointerEvent_Click   = 2,
    Gui_PointerEvent_Release = 3,
} Gui_PointerEvent;


typedef struct gui_pointer_event
{
    Gui_PointerEvent  Type;
    uint32_t          PointerId;
    gui_point         Position;
    gui_translation   Delta;
    Gui_PointerButton ButtonMask;
} gui_pointer_event;


typedef struct gui_pointer_event_node gui_pointer_event_node;
struct gui_pointer_event_node
{
    gui_pointer_event_node *Prev;
    gui_pointer_event_node *Next;
    gui_pointer_event       Value;
};


typedef struct gui_pointer_event_list
{
    gui_pointer_event_node *First;
    gui_pointer_event_node *Last;
    uint32_t                Count;
} gui_pointer_event_list;


static void     GuiClearPointerEvents       (gui_pointer_event_list *List);
static gui_bool GuiPushPointerMoveEvent     (gui_point Position, gui_point LastPosition, gui_pointer_event_node *Node, gui_pointer_event_list *List);
static gui_bool GuiPushPointerClickEvent    (Gui_PointerButton Button, gui_point Position, gui_pointer_event_node *Node, gui_pointer_event_list *List);
static gui_bool GuiPushPointerReleaseEvent  (Gui_PointerButton Button, gui_point Position, gui_pointer_event_node *Node, gui_pointer_event_list *List);


// =============================================================================
// DOMAIN: Context & Memory
// =============================================================================


typedef struct gui_font_list
{
    gui_font *First;
    gui_font *Last;
    uint32_t  Count;
} gui_font_list;


typedef struct gui_context
{
    gui_resource_table *ResourceTable;
    gui_font_list       Fonts;
    float               Width;
    float               Height;
} gui_context;


static gui_context GlobalVoidContext;


static void          GuiBeginFrame         (float Width, float Height, gui_pointer_event_list *EventList, gui_layout_tree *Tree);
static void          GuiEndFrame           (void);

static gui_context * GuiGetContext         (void);


// =============================================================================
// DOMAIN: Components
// =============================================================================


typedef struct gui_component
{
    uint32_t         LayoutIndex;
    gui_layout_tree *LayoutTree;
} gui_component;


static gui_component GuiCreateComponent  (gui_byte_string Name, uint32_t Flags, gui_cached_style *Style, gui_layout_tree *Tree);

static void     GuiSetStyle              (gui_component *Component, gui_cached_style *Style);
static gui_bool GuiPushComponent         (gui_component *Component, gui_parent_node *ParentNode);
static gui_bool GuiPopComponent          (gui_component *Component);
