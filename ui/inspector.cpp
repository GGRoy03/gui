namespace Inspector
{

enum class InspectorStyle : uint32_t
{
    Window    = 0,
    TreePanel = 1,
    TreeNode  = 2,
};

constexpr core::color MainOrange        = core::color(0.9765f, 0.4510f, 0.0863f, 1.0f);
constexpr core::color WhiteOrange       = core::color(1.0000f, 0.9686f, 0.9294f, 1.0f);
constexpr core::color SurfaceOrange     = core::color(0.9961f, 0.8431f, 0.6667f, 1.0f);
constexpr core::color HoverOrange       = core::color(0.9176f, 0.3451f, 0.0471f, 1.0f);
constexpr core::color SubtleOrange      = core::color(0.1686f, 0.1020f, 0.0627f, 1.0f);

constexpr core::color Background        = core::color(0.0588f, 0.0588f, 0.0627f, 1.0f);
constexpr core::color SurfaceBackground = core::color(0.1020f, 0.1098f, 0.1176f, 1.0f);
constexpr core::color BorderOrDivider   = core::color(0.1804f, 0.1961f, 0.2118f, 1.0f);

constexpr core::color TextPrimary       = core::color(0.9765f, 0.9804f, 0.9843f, 1.0f);
constexpr core::color TextSecondary     = core::color(0.6118f, 0.6392f, 0.6863f, 1.0f);

constexpr core::color Success           = core::color(0.1333f, 0.7725f, 0.3686f, 1.0f);
constexpr core::color Error             = core::color(0.9373f, 0.2667f, 0.2667f, 1.0f);
constexpr core::color Warning           = core::color(0.9608f, 0.6196f, 0.0431f, 1.0f);

static cached_style InspectorStyleArray[] =
{
    // Window
    {
        .Default =
        {
            .Size         = size({1000.f, LayoutSizing::Fixed}, {1000.f, LayoutSizing::Fixed}),
            .Direction    = LayoutDirection::Horizontal,
            .XAlign       = Alignment::Start,
            .YAlign       = Alignment::Center,

            .Padding      = core::padding(20.f, 20.f, 20.f, 20.f),
            .Spacing      = 10.f,

            .Color        = Background,
            .BorderColor  = BorderOrDivider,

            .BorderWidth  = 2.f,
            .Softness     = 2.f,
            .CornerRadius = core::corner_radius(4.f, 4.f, 4.f, 4.f),
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
            .Size         = size({50.f, LayoutSizing::Percent}, {100.f, LayoutSizing::Percent}),

            .Padding      = core::padding(20.f, 20.f, 20.f, 20.f),

            .Color        = SurfaceBackground,
            .BorderColor  = BorderOrDivider,

            .BorderWidth  = 2.f,
            .Softness     = 2.f,
            .CornerRadius = core::corner_radius(4.f, 4.f, 4.f, 4.f),
        },
    },

    // Tree Node
    {
        .Default =
        {
            .Size         = size({50.f, LayoutSizing::Fixed}, {50.f, LayoutSizing::Fixed}),

            .Color        = SurfaceBackground,
            .BorderColor  = HoverOrange,

            .BorderWidth  = 2.f,
            .Softness     = 2.f,
            .CornerRadius = core::corner_radius(4.f, 4.f, 4.f, 4.f),
        },
    },
};

struct node_state
{
    uint32_t Index   = layout::InvalidIndex;
    uint32_t Depth   = 0;
    bool     Visible = false;
};

struct visible_list
{
    node_state *Nodes;
    uint32_t    Count;
};

struct inspector_ui
{
    // UI Static State
    bool          IsInitialized = false;
    memory_arena *FrameArena    = 0;

    // UI Transient State
    uint32_t        SelectedNodeIndex = layout::InvalidIndex;
    uint32_t        HoveredNodeIndex  = layout::InvalidIndex;
    node_state     *Nodes             = 0;
    uint32_t        NodeCount         = 0;
    layout::layout_tree *CurrentTree       = 0;

    // UI Resources
    core::resource_key Font;
};

// ------------------------------------------------------------------------------------
// Rendering A Hierarchical Tree View
// ------------------------------------------------------------------------------------

static void
BuildVisibleListEx(layout::layout_node *Node, layout::layout_tree *Tree, uint32_t Depth, visible_list &List)
{
    node_state &State = List.Nodes[List.Count++];
    State.Index = Node->Index;
    State.Depth = Depth;

    // If the node is not expanded, stop collection. Where do I store that?
    // I need some sort of persistent state?


    for (auto *Child = Tree->GetNode(Node->First); layout::layout_node::IsValid(Child); Child = Tree->GetNode(Child->Next))
    {
        BuildVisibleListEx(Child, Tree, Depth + 1, List);
    }
}


static visible_list
BuildVisibleList(layout::layout_node *Root, layout::layout_tree *Tree, memory_arena *Arena)
{
    visible_list List =
    {
        .Nodes = PushArray(Arena, node_state, Tree->NodeCount),
        .Count = 0,
    };

    if(layout::layout_node::IsValid(Root) && layout::layout_tree::IsValid(Tree))
    {
        BuildVisibleListEx(Root, Tree, 0, List);
    }

    return List;
}


static core::node
TreeNode(core::pipeline &Pipeline)
{
    core::node Node = {.Index = UICreateNode2(static_cast<uint32_t>(InspectorStyle::TreeNode), core::NodeType::None, layout::NodeFlags::None, Pipeline.GetActiveTree())};
    return Node;
}


static void
RenderLayoutNodes(core::pipeline &Pipeline, const visible_list &List)
{
    for(uint32_t Idx = 0; Idx < List.Count; ++Idx)
    {
        uint32_t LayoutIndex = List.Nodes[Idx].Index;
        uint32_t LayoutDepth = List.Nodes[Idx].Depth;

        core::node Row = TreeNode(Pipeline);

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
InitializeInspector(inspector_ui &Inspector, core::pipeline &Pipeline)
{
    // UI Pipeline
    {
        core::pipeline_params Params =
        {
            .NodeCount     = 128,
            .FrameBudget   = VOID_KILOBYTE(128),
            .Pipeline      = core::Pipeline::Default,
            .StyleArray    = InspectorStyleArray,
            .StyleIndexMin = static_cast<uint32_t>(InspectorStyle::Window  ),
            .StyleIndexMax = static_cast<uint32_t>(InspectorStyle::TreeNode),
        };

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

    return true;
}


// ------------------------------------------------------------------------------------
// @Public : Inspector API

static void
ShowUI(void)
{
    static inspector_ui Inspector;

    // It is weird that we allow binding before it's even created. Perhaps we rename these functions?
    
    core::pipeline &Pipeline = UIBindPipeline(core::Pipeline::Default);

    if(!Inspector.IsInitialized)
    {
        Inspector.IsInitialized = InitializeInspector(Inspector, Pipeline);

        Inspector.CurrentTree = Pipeline.Tree; // Lazy! Might cause some weird recursive issues.
    }

    if(Inspector.IsInitialized)
    {
        ClearArena(Inspector.FrameArena);

        layout::layout_tree *Tree = Inspector.CurrentTree;
        if(layout::layout_tree::IsValid(Tree))
        {
            core::node Window = UIWindow(static_cast<uint32_t>(InspectorStyle::Window), Pipeline);
            {
                core::node TreePanel = UIDummy(static_cast<uint32_t>(InspectorStyle::TreePanel), Pipeline);
                {
                }
                UIEndDummy(TreePanel, Pipeline);

                core::node OtherPanel = UIDummy(static_cast<uint32_t>(InspectorStyle::TreePanel), Pipeline);
                {
                }
                UIEndDummy(OtherPanel, Pipeline);
            }
            UIEndWindow(Window, Pipeline);
        }
    }

    UIUnbindPipeline(core::Pipeline::Default);
}


}
