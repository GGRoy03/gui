// [Constants]

typedef enum RenderPass_Type
{
    RenderPass_UI    = 0,
    RenderPass_Count = 1,
} RenderPass_Type;

// [Core Types]

typedef struct render_batch
{
    u8 *Memory;
    u64 ByteCount;
    u64 ByteCapacity;
} render_batch;

typedef struct render_batch_node render_batch_node;
struct render_batch_node
{
    render_batch_node *Next;
    render_batch       Batch;
};

typedef struct render_batch_list
{
    render_batch_node *First;
    render_batch_node *Last;

    u64 BatchCount;
    u64 ByteCount;
    u64 BytesPerInstance;
} render_batch_list;

typedef struct render_rect_group_node render_rect_group_node;
struct render_rect_group_node
{
    render_rect_group_node *Next;
    render_batch_list       Batch;

    mat3x3_f32 Transform;
};

// Params Types
// Used to batch groups of data within a pass.

typedef struct render_pass_params_ui
{
    render_rect_group_node *First;
    render_rect_group_node *Last;
} render_pass_params_ui;

// Stats Types
// Used to inferer the size of the allocated arena at the beginning of every frame.
// This structure must be filled correctly throughout the frame to achieve better
// allocations.

typedef struct render_pass_ui_stats
{
    u32 BatchCount;
    u32 GroupCount;
    u32 PassCount;
    u64 RenderedDataSize;
} render_pass_ui_stats;

// Render Pass Types
// The game contains multiple render passes (UI, 3D Geometry, ...). Each is batched
// and has a set of allocated memory (which they can overflow). Implemented as a linked
// list.

typedef struct render_pass
{
    RenderPass_Type Type;
    union
    {
        void                  *Params;
        render_pass_params_ui *UI;
    } Params;
} render_pass;

typedef struct render_pass_node render_pass_node;
struct render_pass_node
{
    render_pass_node *Next;
    render_pass       Pass;
};

typedef struct render_context
{
    // UI
    render_pass_ui_stats UIStats;
    memory_arena        *UIPassArena;
    render_pass_node    *UIPassNode;
} render_context;

// [Globals]

global u32 UIPassDefaultBatchCount       = 10;
global u32 UIPassDefaultGroupCount       = 20;
global u32 UIPassDefaultPassCount        = 5;
global u32 UIPassDefaultRenderedDataSize = Kilobyte(50);
global u32 UIPassPaddingSize             = Kilobyte(25);

// [CORE API]

internal b32 BeginRendererFrame(render_context *Context);
