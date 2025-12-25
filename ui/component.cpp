static core::node
UIWindow(uint32_t StyleIndex, core::pipeline &Pipeline)
{
    core::node Node = {};

    layout::NodeFlags Flags     = layout::NodeFlags::ClipContent | layout::NodeFlags::IsDraggable | layout::NodeFlags::IsResizable;
    uint32_t        NodeIndex = UICreateNode2(StyleIndex, core::NodeType::Container, Flags, Pipeline.GetActiveTree());

    if(UIPushLayoutParent2(NodeIndex, Pipeline.GetActiveTree(), Pipeline.FrameArena))
    {
        Node.Index = NodeIndex;
    }

    return Node;
}

static void
UIEndWindow(core::node Node, core::pipeline &Pipeline)
{
    // NOTE: Should we check anything? I don't yet, but this pattern is nice.
    UIPopLayoutParent2(Node.Index, Pipeline.GetActiveTree());
}

static core::node
UIDummy(uint32_t StyleIndex, core::pipeline &Pipeline)
{
    core::node Node = {};

    uint32_t NodeIndex = UICreateNode2(StyleIndex, core::NodeType::None, layout::NodeFlags::None, Pipeline.GetActiveTree());

    if(UIPushLayoutParent2(NodeIndex, Pipeline.GetActiveTree(), Pipeline.FrameArena))
    {
        Node.Index = NodeIndex;
    }

    return Node;
}

static void
UIEndDummy(core::node Node, core::pipeline &Pipeline)
{
    // NOTE: Should we check anything? I don't yet, but this pattern is nice.
    UIPopLayoutParent2(Node.Index, Pipeline.GetActiveTree());
}
