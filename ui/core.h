#pragma once

#include <immintrin.h>

struct ui_layout_tree;
struct parent_node;
enum class NodeFlags : uint32_t;
struct cached_style;
struct font;
struct cached_style;

// =============================================================================
// DOMAIN: Basic Types
// =============================================================================

enum class AxisType
{
    None = 0,
    X    = 1,
    Y    = 2,
    XY   = 3,
};

struct color
{
    float R;
    float G;
    float B;
    float A;
};

struct corner_radius
{
    float TL;
    float TR;
    float BR;
    float BL;
};

struct padding
{
    float Left;
    float Top;
    float Right;
    float Bot;
};

struct rect
{
    rect_float    RectBounds;
    rect_float    TextureSource;
    color         ColorTL;
    color         ColorBL;
    color         ColorTR;
    color         ColorBR;
    corner_radius CornerRadii;
    float         BorderWidth;
    float         Softness;
    float         SampleTexture;
    float        _Padding0;
};

// =============================================================================
// DOMAIN: Resources
// =============================================================================

enum class ResourceType
{
    None         = 0,
    Text         = 1,
    TextInput    = 2,
    ScrollRegion = 3,
    Image        = 4,
    ImageGroup   = 5,
    Font         = 6,
};

enum class FindResourceFlag
{
    None          = 0,
    AddIfNotFound = 1 << 0,
};

inline FindResourceFlag operator|(FindResourceFlag A, FindResourceFlag B)
{
    return static_cast<FindResourceFlag>(static_cast<int>(A) | static_cast<int>(B));
}

inline FindResourceFlag operator&(FindResourceFlag A, FindResourceFlag B)
{
    return static_cast<FindResourceFlag>(static_cast<int>(A) & static_cast<int>(B));
}

struct resource_key
{
    __m128i Value;
};

struct resource_stats
{
    uint64_t CacheHitCount;
    uint64_t CacheMissCount;
};

struct resource_table_params
{
    uint32_t HashSlotCount;
    uint32_t EntryCount;
};

struct resource_state
{
    uint32_t     Id;
    ResourceType ResourceType;
    void        *Resource;
};

struct resource_table;

// --- Public API ---

static uint64_t         GetResourceTableFootprint   (resource_table_params Params);
static resource_table * PlaceResourceTableInMemory  (resource_table_params Params, void *Memory);

static resource_key     MakeNodeResourceKey         (ResourceType Type, uint32_t NodeIndex, ui_layout_tree *Tree);
static resource_key     MakeFontResourceKey         (byte_string Name, float Size);

static resource_state   FindResourceByKey           (resource_key Key, FindResourceFlag Flags, resource_table *Table);
static void             UpdateResourceTable         (uint32_t Id, resource_key Key, void *Memory, resource_table *Table);

static void           * QueryNodeResource           (ResourceType Type, uint32_t NodeIndex, ui_layout_tree *Tree, resource_table *Table);

// 

// 

enum class PointerSource
{
    None       = 0,
    Mouse      = 1,
    Touch      = 2,
    Pen        = 3,
    Controller = 4,
};

enum class PointerEvent
{
    None    = 0,
    Move    = 1,
    Click   = 2,
    Release = 3,
};

constexpr uint32_t BUTTON_NONE      = 0;
constexpr uint32_t BUTTON_PRIMARY   = 1u << 0;
constexpr uint32_t BUTTON_SECONDARY = 1u << 1;

struct input_pointer
{
    uint32_t      Id;
    PointerSource Source;
    vec2_float    Position;
    vec2_float    Delta;
    uint32_t      ButtonMask;
    uint32_t      LastButtonMask;
};

struct pointer_event
{
    PointerEvent Type;
    uint32_t     PointerId;
    vec2_float   Position;
    vec2_float   Delta;
    uint32_t     ButtonMask;
};

struct pointer_event_node
{
    pointer_event_node *Prev;
    pointer_event_node *Next;
    pointer_event       Value;
};

struct pointer_event_list
{
    pointer_event_node *First;
    pointer_event_node *Last;
    uint32_t            Count;
};

static void
PushPointerMoveEvent(pointer_event_list *List, pointer_event_node *Node, vec2_float Position, vec2_float Delta);
static void
PushPointerClickEvent(pointer_event_list *List, pointer_event_node *Node, uint32_t Button, vec2_float Position);
static void
PushPointerReleaseEvent(pointer_event_list *List, pointer_event_node *Node, uint32_t Button, vec2_float Position);


// =============================================================================
// DOMAIN: Context & Memory
// =============================================================================

struct font_list
{
    font     *First;
    font     *Last;
    uint32_t Count;
};

struct void_context
{
    resource_table *ResourceTable;
    font_list       Fonts;

    // Frame State

    float Width;
    float Height;
};

static void_context GlobalVoidContext;


struct memory_footprint
{
    uint64_t SizeInBytes;
    uint64_t Alignment;
};


struct memory_block
{
    uint64_t SizeInBytes;
    void    *Base;
};


struct memory_region
{
    void    *Base;
    uint64_t Size;
    uint64_t At;
};


static bool
IsValidMemoryRegion(const memory_region &Region)
{
    bool Result = Region.Base && Region.Size && Region.At <= Region.Size;
    return Result;
}

static memory_region
EnterMemoryRegion(memory_block Block)
{
    memory_region Region =
    {
        .Base = Block.Base,
        .Size = Block.SizeInBytes,
        .At   = 0,
    };

    return Region;
}

static void *
PushMemoryRegion(memory_region &Region, uint64_t Size, uint64_t Alignment)
{
    void *Result = 0;

    uint64_t Before = AlignPow2(Region.At, Alignment); // Like.. Wouldn't it already be aligned?
    uint64_t After  = Before + Size;

    if(After <= Region.Size)
    {
        Result = (uint8_t *)Region.Base + Before;
        Region.At = After;
    }

    return Result;
}

template <typename T>
constexpr T* PushArrayNoZeroAligned(memory_region &Region, uint64_t Count, uint64_t Align)
{
    return static_cast<T*>(PushMemoryRegion(Region, sizeof(T) * Count, Align));
}

template <typename T>
constexpr T* PushArrayAligned(memory_region &Region, uint64_t Count, uint64_t Align)
{
    return PushArrayNoZeroAligned<T>(Region, Count, Align);
}

template <typename T>
constexpr T* PushArray(memory_region &Region, uint64_t Count)
{
    return PushArrayAligned<T>(Region, Count, max(8, alignof(T)));
}

template <typename T>
constexpr T* PushStruct(memory_region &Region)
{
    return PushArray<T>(Region, 1);
}


static void BeginFrame  (float Width, float Height, const pointer_event_list &EventList, ui_layout_tree *Tree);
static void EndFrame    (void);

static void_context & GetVoidContext     ();
static void           CreateVoidContext  ();

// =============================================================================
// DOMAIN: Meta Tree
// =============================================================================

struct component
{
    uint32_t      LayoutIndex;
    ui_layout_tree * LayoutTree;

    // Attributes

    void SetStyle  (cached_style *Style);

    // Layout

    bool Push      (parent_node *ParentNode);
    bool Pop       ();

    // Helpers

    component(const char *Name, NodeFlags Flags, cached_style *Style, ui_layout_tree *Tree);
    component(byte_string Name, NodeFlags Flags, cached_style *Style, ui_layout_tree *Tree);
};
