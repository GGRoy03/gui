static ui_node
UIWindow(uint32_t StyleIndex, ui_pipeline &Pipeline)
{
    ui_node Node = {};

    Layout::NodeFlags Flags     = Layout::NodeFlags::ClipContent | Layout::NodeFlags::IsDraggable | Layout::NodeFlags::IsResizable;
    uint32_t        NodeIndex = UICreateNode2(NodeType::Container, Flags, Pipeline.GetActiveTree());

    if(UIPushLayoutParent2(NodeIndex, Pipeline.GetActiveTree(), Pipeline.FrameArena))
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
    UIPopLayoutParent2(Node.Index, Pipeline.GetActiveTree());
}

static ui_node
UIDummy(uint32_t StyleIndex, ui_pipeline &Pipeline)
{
    ui_node Node = {};

    uint32_t NodeIndex = UICreateNode2(NodeType::None, Layout::NodeFlags::None, Pipeline.GetActiveTree());

    if(UIPushLayoutParent2(NodeIndex, Pipeline.GetActiveTree(), Pipeline.FrameArena))
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
    UIPopLayoutParent2(Node.Index, Pipeline.GetActiveTree());
}
