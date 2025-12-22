static ui_node
UIWindow(uint32_t StyleIndex, ui_pipeline &Pipeline)
{
    ui_node Node = {};

    LayoutNodeFlags Flags     = LayoutNodeFlags::ClipContent | LayoutNodeFlags::IsDraggable | LayoutNodeFlags::IsResizable;
    uint32_t        NodeIndex = UICreateNode(Flags, Pipeline.Tree);

    if(UIPushLayoutParent(NodeIndex, Pipeline.Tree, Pipeline.FrameArena))
    {
        Node.Index = NodeIndex;
        Node.SetStyle(StyleIndex, Pipeline);
    }

    return Node;
}

static void
UIEndWindow(ui_node Node, ui_pipeline &Pipeline)
{
    // NOTE: Should we check anything? I don't yet, but this pattern is nice.
    UIPopLayoutParent(Node.Index, Pipeline.Tree);
}

static ui_node
UIDummy(uint32_t StyleIndex, ui_pipeline &Pipeline)
{
    ui_node Node = {};

    uint32_t NodeIndex = UICreateNode(LayoutNodeFlags::None, Pipeline.Tree);

    if(UIPushLayoutParent(NodeIndex, Pipeline.Tree, Pipeline.FrameArena))
    {
        Node.Index = NodeIndex;
        Node.SetStyle(StyleIndex, Pipeline);
    }

    return Node;
}

static void
UIEndDummy(ui_node Node, ui_pipeline &Pipeline)
{
    // NOTE: Should we check anything? I don't yet, but this pattern is nice.
    UIPopLayoutParent(Node.Index, Pipeline.Tree);
}
