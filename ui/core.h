// Think this should not be here honestly.
namespace layout
{
    struct ui_layout_tree;
}

// Idk...
struct ui_font;
struct ui_cached_style;

namespace core
{

enum class AxisType
{
    None = 0,

    X    = 1,
    Y    = 2,
    XY   = 3,
};

// [CORE TYPES]

struct ui_color
{
    float R;
    float G;
    float B;
    float A;
};

struct ui_corner_radius
{
    float TL;
    float TR;
    float BR;
    float BL;
};

struct ui_padding
{
    float Left;
    float Top;
    float Right;
    float Bot;
};


// NOTE: Must be padded to 16 bytes alignment.
typedef struct ui_rect
{
    rect_float       RectBounds;
    rect_float       TextureSource;
    ui_color         ColorTL;
    ui_color         ColorBL;
    ui_color         ColorTR;
    ui_color         ColorBR;
    ui_corner_radius CornerRadii;
    float            BorderWidth, Softness, SampleTexture, _P0; // Style Params
} ui_rect;

// ------------------------------------------------------------------------------------

#include <immintrin.h>

typedef enum UIResource_Type
{
    UIResource_None         = 0,
    UIResource_Text         = 1,
    UIResource_TextInput    = 2,
    UIResource_ScrollRegion = 3,
    UIResource_Image        = 4,
    UIResource_ImageGroup   = 5,
    UIResource_Font         = 6,
} UIResource_Type;

struct ui_resource_key
{
    __m128i Value;
};

struct ui_resource_stats
{
    uint64_t CacheHitCount;
    uint64_t CacheMissCount;
};

struct ui_resource_table_params
{
    uint32_t HashSlotCount;
    uint32_t EntryCount;
};

struct ui_resource_state
{
    uint32_t        Id;
    UIResource_Type ResourceType;
    void           *Resource;
};


struct ui_resource_table;

static uint64_t            GetResourceTableFootprint   (ui_resource_table_params Params);
static ui_resource_table * PlaceResourceTableInMemory  (ui_resource_table_params Params, void *Memory);

// Keys:
//   Opaque handles to resources. Use a resource table to retrieve the associated data
//   MakeNodeResourceKey is used for general node-based resources
//   MakeFontResourceKey is used for global  font       resources




static ui_resource_key MakeNodeResourceKey   (UIResource_Type Type, uint32_t NodeIndex, layout::ui_layout_tree *Tree);
static ui_resource_key MakeFontResourceKey   (byte_string Name, float Size);

// Resources:
//   Use FindResourceByKey to retrieve some resource with a key created from MakeResourceKey.
//   If the resource doesn't exist yet, the returned state will contain: .ResourceType = UIResource_None AND .Resource = NULL.
//   You may update the table using UpdateResourceTable by passing the relevant updated data. The id is retrieved in State.Id.

enum class FindResourceFlag
{
    None          = 0,
    AddIfNotFound = 1 << 0,
};

// Template this for convenience.

inline FindResourceFlag operator|(FindResourceFlag A, FindResourceFlag B) {return static_cast<FindResourceFlag>(static_cast<int>(A) | static_cast<int>(B));}
inline FindResourceFlag operator&(FindResourceFlag A, FindResourceFlag B) {return static_cast<FindResourceFlag>(static_cast<int>(A) & static_cast<int>(B));}

static ui_resource_state FindResourceByKey     (ui_resource_key Key, FindResourceFlag Flags,  ui_resource_table *Table);
static void              UpdateResourceTable   (uint32_t Id, ui_resource_key Key, void *Memory, ui_resource_table *Table);

static void * QueryNodeResource  (UIResource_Type Type, uint32_t NodeIndex, layout::ui_layout_tree *Tree, ui_resource_table *Table);


struct ui_pipeline;

// I do wonder if we want to add the pipeline as a member to simplify?
// What do we want to do with this anyways?

struct ui_node
{
    uint32_t Index;

    // Layout
    ui_node  FindChild        (uint32_t Index , ui_pipeline &Pipeline);
    void     Append           (ui_node  Child , ui_pipeline &Pipeline);
    void     SetOffset        (float XOffset, float YOffset, ui_pipeline &Pipeline);

    // Queries
    bool     IsClicked        (ui_pipeline &Pipeline);

    // Resource
    void     SetText          (byte_string Text, ui_resource_key FontKey, ui_pipeline &Pipeline);
    void     SetTextInput     (uint8_t *Buffer, uint64_t BufferSize, ui_pipeline &Pipeline);
    void     SetScroll        (float ScrollSpeed, AxisType Axis, ui_pipeline &Pipeline);
    void     SetImage         (byte_string Path, byte_string Group, ui_pipeline &Pipeline);

    // Debug
    void     DebugBox         (uint32_t Flag, bool Draw, ui_pipeline &Pipeline);

    // Helpers
    bool     IsValid          ();
};

// -----------------------------------------------------------------------------------

static void UIBeginFrame  (vec2_int WindowSize);
static void UIEndFrame    (void);

// -----------------------------------------------------------------------------------

enum class UIPipeline : uint32_t
{
    Default = 0,
    Count   = 1,
};

constexpr uint32_t PipelineCount = static_cast<uint32_t>(UIPipeline::Count);

struct ui_pipeline_params
{
    uint64_t             NodeCount;
    uint64_t             FrameBudget;

    UIPipeline           Pipeline;
    ui_cached_style     *StyleArray;
    uint32_t             StyleIndexMin;
    uint32_t             StyleIndexMax;
};

struct ui_meta_tree;
struct ui_pipeline
{
    // UI State

    layout::ui_layout_tree *Tree;
    ui_meta_tree           *MetaTrees[2];
    uint32_t                ActiveMetaTree;

    // User State

    UIPipeline              Type;
    ui_cached_style        *StyleArray;
    uint32_t                StyleIndexMin;
    uint32_t                StyleIndexMax;

    // Memory

    memory_arena           *StateArena;
    memory_arena           *FrameArena;

    // Misc

    bool                    Bound;
    uint64_t                NodeCount; // Why?
    
    // Helpers

    ui_meta_tree * GetActiveTree()
    {
        ui_meta_tree *Result = MetaTrees[ActiveMetaTree];
        return Result;
    }
};


static void               UICreatePipeline            (const ui_pipeline_params &Params);
static ui_pipeline&       UIBindPipeline              (UIPipeline Pipeline);
static void               UIUnbindPipeline            (UIPipeline Pipeline);

// ----------------------------------------

// NOTE:
// Only here, because of the D3D11 debug layer bug? Really?
// Because it could fit in the global resource pattern as far as I understand?

struct ui_font_list
{
    ui_font *First;
    ui_font *Last;
    uint32_t Count;
};

struct void_context
{
    // Memory
    memory_arena      *StateArena;

    // State
    ui_resource_table *ResourceTable;
    ui_pipeline        PipelineArray[PipelineCount];
    uint32_t           PipelineCount;

    ui_font_list     Fonts; // TODO: Find a solution such that this is a global resource.

    // State
    vec2_int   WindowSize;
};

static void_context GlobalVoidContext;

static void_context & GetVoidContext     (void);
static void           CreateVoidContext  (void);


// -----------------------------------------------------------------------------
// @Private Meta-Layout API
// Functions intended for internal use.
// -----------------------------------------------------------------------------

enum class NodeType : uint32_t
{
    None = 0,

    Container = 1,
};

static uint64_t GetMetaTreeFootprint   (uint64_t NodeCount);
static uint64_t PlaceMetaTreeInMemory  (uint64_t NodeCount);

} // namespace core
