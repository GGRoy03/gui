namespace Inspector
{

enum class InspectorStyle : uint32_t
{
    Window    = 0,
    TreePanel = 1,
    TreeNode  = 2,
};

constexpr ui_color MainOrange        = ui_color(0.9765f, 0.4510f, 0.0863f, 1.0f);
constexpr ui_color WhiteOrange       = ui_color(1.0000f, 0.9686f, 0.9294f, 1.0f);
constexpr ui_color SurfaceOrange     = ui_color(0.9961f, 0.8431f, 0.6667f, 1.0f);
constexpr ui_color HoverOrange       = ui_color(0.9176f, 0.3451f, 0.0471f, 1.0f);
constexpr ui_color SubtleOrange      = ui_color(0.1686f, 0.1020f, 0.0627f, 1.0f);

constexpr ui_color Background        = ui_color(0.0588f, 0.0588f, 0.0627f, 1.0f);
constexpr ui_color SurfaceBackground = ui_color(0.1020f, 0.1098f, 0.1176f, 1.0f);
constexpr ui_color BorderOrDivider   = ui_color(0.1804f, 0.1961f, 0.2118f, 1.0f);

constexpr ui_color TextPrimary       = ui_color(0.9765f, 0.9804f, 0.9843f, 1.0f);
constexpr ui_color TextSecondary     = ui_color(0.6118f, 0.6392f, 0.6863f, 1.0f);

constexpr ui_color Success           = ui_color(0.1333f, 0.7725f, 0.3686f, 1.0f);
constexpr ui_color Error             = ui_color(0.9373f, 0.2667f, 0.2667f, 1.0f);
constexpr ui_color Warning           = ui_color(0.9608f, 0.6196f, 0.0431f, 1.0f);

static ui_cached_style InspectorStyleArray[] =
{
    // Window
    {
        .Default =
        {
            .Size         = ui_size({1000.f, LayoutSizing::Fixed}, {1000.f, LayoutSizing::Fixed}),
            .Direction    = LayoutDirection::Horizontal,
            .XAlign       = Alignment::Start,
            .YAlign       = Alignment::Center,

            .Padding      = ui_padding(20.f, 20.f, 20.f, 20.f),
            .Spacing      = 10.f,

            .Color        = Background,
            .BorderColor  = BorderOrDivider,

            .BorderWidth  = 2.f,
            .Softness     = 2.f,
            .CornerRadius = ui_corner_radius(4.f, 4.f, 4.f, 4.f),
        },

        .Hovered =
        {
            .BorderColor = HoverOrange,
        },

        .Focused =
        {
            .BorderColor = Success,
        },
    },

    // Tree Panel
    {
        .Default =
        {
            .Size         = ui_size({50.f, LayoutSizing::Percent}, {100.f, LayoutSizing::Percent}),

            .Padding      = ui_padding(20.f, 20.f, 20.f, 20.f),

            .Color        = SurfaceBackground,
            .BorderColor  = BorderOrDivider,

            .BorderWidth  = 2.f,
            .Softness     = 2.f,
            .CornerRadius = ui_corner_radius(4.f, 4.f, 4.f, 4.f),
        },
    },

    // Tree Node
    {
        .Default =
        {
            .Size         = ui_size({50.f, LayoutSizing::Fixed}, {50.f, LayoutSizing::Fixed}),

            .Color        = SurfaceBackground,
            .BorderColor  = HoverOrange,

            .BorderWidth  = 2.f,
            .Softness     = 2.f,
            .CornerRadius = ui_corner_radius(4.f, 4.f, 4.f, 4.f),
        },
    },
};

struct node_state
{
    uint32_t Index   = InvalidLayoutNodeIndex;
    uint32_t Depth   = 0;
    bool     Visible = false;
};

struct inspector_ui
{
    // UI Static State
    bool          IsInitialized = false;
    memory_arena *FrameArena    = 0;

    // UI Transient State
    uint32_t        SelectedNodeIndex = InvalidLayoutNodeIndex;
    uint32_t        HoveredNodeIndex  = InvalidLayoutNodeIndex;
    node_state     *Nodes             = 0;
    uint32_t        NodeCount         = 0;
    ui_layout_tree *CurrentTree       = 0;

    // UI Resources
    ui_resource_key Font;
};

// ------------------------------------------------------------------------------------
// Rendering A Hierarchical Tree View
// ------------------------------------------------------------------------------------

static void
BuildVisibleListEx(ui_layout_node *Node, ui_layout_tree *Tree, uint32_t Depth, inspector_ui &Inspector)
{
    node_state &State = Inspector.Nodes[Inspector.NodeCount++];
    State.Index = Node->Index;
    State.Depth = Depth;

    // If the node is not expanded, stop collection. Where do I store that?
    // I need some sort of persistent state?


    for (auto *Child = Tree->GetNode(Node->First); ui_layout_node::IsValid(Child); Child = Tree->GetNode(Child->Next))
    {
        BuildVisibleListEx(Child, Tree, Depth + 1, Inspector);
    }
}


static void
BuildVisibleList(ui_layout_node *Root, ui_layout_tree *Tree, inspector_ui &Inspector)
{
    Inspector.Nodes     = PushArray(Inspector.FrameArena, node_state, Tree->NodeCount);
    Inspector.NodeCount = 0;

    if(ui_layout_node::IsValid(Root) && ui_layout_tree::IsValid(Tree))
    {
        BuildVisibleListEx(Root, Tree, 0, Inspector);
    }
}


static ui_node
TreeNode(ui_pipeline &Pipeline)
{
    LayoutNodeFlags Flags = LayoutNodeFlags::IsImmediate;

    ui_node Node = {.Index = UICreateNode(Flags, Pipeline.Tree)};

    if(Node.Index != InvalidLayoutNodeIndex)
    {
        Node.SetStyle(static_cast<uint32_t>(InspectorStyle::TreeNode), Pipeline);
    }

    return Node;
}


static void
RenderLayoutNodes(ui_pipeline &Pipeline, inspector_ui &Inspector)
{
    for(uint32_t Idx = 0; Idx < Inspector.NodeCount; ++Idx)
    {
        uint32_t LayoutIndex = Inspector.Nodes[Idx].Index;
        uint32_t LayoutDepth = Inspector.Nodes[Idx].Depth;

        ui_node Row = TreeNode(Pipeline);

        if(!Row.IsValid())
        {
            continue;
        }

        // This simply cannot work with the current architecture. Can we find something else?

        if(Row.IsClicked(Pipeline))
        {
            VOID_ASSERT(!"Crash");
        }

        Row.SetOffset(LayoutDepth * 10.f, 0.f, Pipeline);
    }
}

// ------------------------------------------------------------------------------------
// @Private : Inspector Helpers

static bool
InitializeInspector(inspector_ui &Inspector, ui_pipeline &Pipeline)
{
    // UI Pipeline
    {
        ui_pipeline_params Params = UIGetDefaultPipelineParams();
        Params.NodeCount     = 128;
        Params.Pipeline      = UIPipeline::Default;
        Params.StyleArray    = InspectorStyleArray;
        Params.StyleIndexMin = static_cast<uint32_t>(InspectorStyle::Window  );
        Params.StyleIndexMax = static_cast<uint32_t>(InspectorStyle::TreeNode);
        Params.FrameBudget   = VOID_KILOBYTE(128);

        UICreatePipeline(Params);
    }

    // UI State
    {
        Inspector.FrameArena = AllocateArena({.ReserveSize = VOID_KILOBYTE(128)});
    }

    // UI Resource
    {
        Inspector.Font = UILoadSystemFont(str8_comp("Consolas"), 16.f, 1024, 1024);
    }

    // Base Layout
    {
        ui_node Window = UIWindow(static_cast<uint32_t>(InspectorStyle::Window), Pipeline);
        {
            ui_node TreePanel  = UIDummy(static_cast<uint32_t>(InspectorStyle::TreePanel), Pipeline);
            {
            }
            UIEndDummy(TreePanel, Pipeline);

            ui_node OtherPanel = UIDummy(static_cast<uint32_t>(InspectorStyle::TreePanel), Pipeline);
            {
            }
            UIEndDummy(OtherPanel, Pipeline);

        }
        UIEndWindow(Window, Pipeline);
    }

    return true;
}


// ------------------------------------------------------------------------------------
// @Public : Inspector API

static void
ShowUI(void)
{
    static inspector_ui Inspector;

    // It is weird that we allow binding before it's even created. Perhaps we rename these functions?
    
    ui_pipeline &Pipeline = UIBindPipeline(UIPipeline::Default);

    if(!Inspector.IsInitialized)
    {
        Inspector.IsInitialized = InitializeInspector(Inspector, Pipeline);

        Inspector.CurrentTree = Pipeline.Tree; // Lazy!
    }

    if(Inspector.IsInitialized)
    {
        ClearArena(Inspector.FrameArena);

        ui_layout_tree *Tree = Inspector.CurrentTree;
        if(ui_layout_tree::IsValid(Tree))
        {
            BuildVisibleList(Tree->GetNode(0), Tree, Inspector);

            if(UIPushLayoutParent(1, Tree, Pipeline.FrameArena))
            {
                RenderLayoutNodes(Pipeline, Inspector);
                UIPopLayoutParent(1, Tree);
            }
        }
    }

    UIUnbindPipeline(UIPipeline::Default);
}

}
