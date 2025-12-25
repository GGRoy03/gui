static core::ui_node
UIWindow(uint32_t StyleIndex, core::ui_pipeline &Pipeline)
{
    core::ui_node Node = {};

    layout::NodeFlags Flags     = layout::NodeFlags::ClipContent | layout::NodeFlags::IsDraggable | layout::NodeFlags::IsResizable;
    uint32_t        NodeIndex = UICreateNode2(StyleIndex, core::NodeType::Container, Flags, Pipeline.GetActiveTree());

    if(UIPushLayoutParent2(NodeIndex, Pipeline.GetActiveTree(), Pipeline.FrameArena))
    {
        Node.Index = NodeIndex;
    }

    return Node;
}

static void
UIEndWindow(core::ui_node Node, core::ui_pipeline &Pipeline)
{
    // NOTE: Should we check anything? I don't yet, but this pattern is nice.
    UIPopLayoutParent2(Node.Index, Pipeline.GetActiveTree());
}

static core::ui_node
UIDummy(uint32_t StyleIndex, core::ui_pipeline &Pipeline)
{
    core::ui_node Node = {};

    uint32_t NodeIndex = UICreateNode2(StyleIndex, core::NodeType::None, layout::NodeFlags::None, Pipeline.GetActiveTree());

    if(UIPushLayoutParent2(NodeIndex, Pipeline.GetActiveTree(), Pipeline.FrameArena))
    {
        Node.Index = NodeIndex;
    }

    return Node;
}

static void
UIEndDummy(core::ui_node Node, core::ui_pipeline &Pipeline)
{
    // NOTE: Should we check anything? I don't yet, but this pattern is nice.
    UIPopLayoutParent2(Node.Index, Pipeline.GetActiveTree());
}
