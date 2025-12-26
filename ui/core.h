#pragma once

#include <immintrin.h>

namespace layout
{
    struct layout_tree;

    enum class NodeFlags : uint32_t;
}


struct font;
struct cached_style;

namespace core
{

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

static resource_key     MakeNodeResourceKey         (ResourceType Type, uint32_t NodeIndex, layout::layout_tree *Tree);
static resource_key     MakeFontResourceKey         (byte_string Name, float Size);

static resource_state   FindResourceByKey           (resource_key Key, FindResourceFlag Flags, resource_table *Table);
static void             UpdateResourceTable         (uint32_t Id, resource_key Key, void *Memory, resource_table *Table);

static void           * QueryNodeResource           (ResourceType Type, uint32_t NodeIndex, layout::layout_tree *Tree, resource_table *Table);

// =============================================================================
// DOMAIN:  Node
// =============================================================================

// =============================================================================
// DOMAIN: Pipeline
// =============================================================================

enum class Pipeline
{
    Default = 0,
    Count   = 1,
};

constexpr uint32_t PipelineCount = static_cast<uint32_t>(Pipeline::Count);

struct pipeline_params
{
    uint64_t      NodeCount;
    uint64_t      FrameBudget;
    Pipeline      Pipeline;
    cached_style *StyleArray;
    uint32_t      StyleIndexMin;
    uint32_t      StyleIndexMax;
};

struct meta_tree;

struct pipeline
{
    layout::layout_tree    *Tree;
    Pipeline                Type;

    memory_arena           *StateArena;
    memory_arena           *FrameArena;

    bool                    Bound;
    uint64_t                NodeCount;
};

// --- Public API ---

static void       BeginFrame      (vec2_int WindowSize);
static void       EndFrame        ();

static void       CreatePipeline  (const pipeline_params &Params);
static pipeline & BindPipeline    (Pipeline Pipeline);
static void       UnbindPipeline  (Pipeline Pipeline);

// =============================================================================
// DOMAIN: Fonts & Context
// =============================================================================

struct font_list
{
    font     *First;
    font     *Last;
    uint32_t Count;
};

struct void_context
{
    memory_arena   *StateArena;
    resource_table *ResourceTable;
    pipeline        PipelineArray[PipelineCount];
    uint32_t        PipelineCount;
    font_list       Fonts;
    vec2_int        WindowSize;
};

static void_context GlobalVoidContext;

// --- Public API ---

static void_context & GetVoidContext     ();
static void           CreateVoidContext  ();

// =============================================================================
// DOMAIN: Meta Tree
// =============================================================================

struct component
{
    uint32_t              LayoutIndex;
    layout::layout_tree * LayoutTree;

    // Attributes

    void SetStyle  (const cached_style *Style);

    // Layout

    bool Push      (memory_arena *Arena);
    bool Pop       ();

    // Helpers

    component(const char *Name, layout::NodeFlags Flags, const cached_style *Style, layout::layout_tree *Tree);
    component(byte_string Name, layout::NodeFlags Flags, const cached_style *Style, layout::layout_tree *Tree);
};

} // namespace core
