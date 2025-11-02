// WARN: This is really garbage? Yeah this is complete shit and it kinda makes no sense...

internal ui_node_chain * 
UIComponentAll(u32 StyleIndex, bit_field Flags, byte_string Text)
{
    ui_pipeline *Pipeline = GetCurrentPipeline();
    Assert(Pipeline);

    ui_subtree *Subtree = Pipeline->CurrentSubtree;
    Assert(Subtree);

    ui_node Node = AllocateUINode(Flags, Subtree);
    Assert(Node.CanUse);

    ui_node_style *Style = GetNodeStyle(Node.IndexInTree, Subtree);
    Style->CachedStyleIndex = StyleIndex;

    // NOTE: This needs to go!
    if(HasFlag(Flags, UILayoutNode_IsScrollable))
    {
        BindScrollContext(Node, ScrollAxis_Y, Subtree->LayoutTree, Pipeline->StaticArena);
    }

    ui_node_chain *Chain = UIChain(Node);
    return Chain;
}

internal ui_node_chain *
UIComponent(u32 StyleIndex, bit_field Flags)
{
    ui_node_chain *Chain = UIComponentAll(StyleIndex, Flags, ByteString(0, 0));
    Assert(Chain);

    return Chain;
}

// ------------------------------------------------------------------------

internal ui_node_chain *
UIWindow(u32 StyleIndex)
{
    bit_field Flags = 0;
    {
        SetFlag(Flags, UILayoutNode_IsDraggable);
        SetFlag(Flags, UILayoutNode_IsResizable);
        SetFlag(Flags, UILayoutNode_IsParent);
    }

    ui_node_chain *Chain = UIComponent(StyleIndex, Flags);
    return Chain;
}

internal ui_node_chain *
UIScrollView(u32 StyleIndex)
{
    bit_field Flags = 0;
    {
        SetFlag(Flags, UILayoutNode_IsParent);
        SetFlag(Flags, UILayoutNode_PlaceChildrenY);
        SetFlag(Flags, UILayoutNode_IsScrollable);
        SetFlag(Flags, UILayoutNode_HasClip);
    }

    ui_node_chain *Chain = UIComponent(StyleIndex, Flags);
    return Chain;
}

// NOTE:
// We for sure ask the user for a text buffer.
// But how do we manage our memory?
// We need some array of shaped glyphs.
// And the access to the user buffer.

internal ui_node_chain *
UITextInput(u8 *TextBuffer, u64 TextBufferSize, u32 StyleIndex)
{
    bit_field Flags = 0;
    {
        SetFlag(Flags, UILayoutNode_HasTextInput);
    }

    ui_node_chain *Chain = UIComponent(StyleIndex, Flags);
    return Chain;
}
