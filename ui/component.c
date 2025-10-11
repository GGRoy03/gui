internal void
UIWindow(u32 StyleIndex, ui_pipeline *Pipeline)
{
    ui_cached_style *Style = GetCachedStyle(Pipeline->Registry, StyleIndex);
    bit_field        Flags = 0;

    // Constants Flags
    {
        SetFlag(Flags, UILayoutNode_PlaceChildrenY);
        SetFlag(Flags, UILayoutNode_IsDraggable);
        SetFlag(Flags, UILayoutNode_IsResizable);
        SetFlag(Flags, UILayoutNode_IsParent);
    }

    InitializeLayoutNode(Style, UILayoutNode_Window, Flags, Pipeline->LayoutTree);
}

internal void
UIButton(u32 StyleIndex, ui_click_callback *Callback, ui_pipeline *Pipeline)
{
    ui_cached_style *Style = GetCachedStyle(Pipeline->Registry, StyleIndex);
    bit_field        Flags = 0;

    // TODO: Figure out what we want to do with callbacks.
    Useless(Callback);

    InitializeLayoutNode(Style, UILayoutNode_Button, Flags, Pipeline->LayoutTree);
}

internal void
UIHeader(u32 StyleIndex, ui_pipeline *Pipeline)
{
    ui_cached_style *Style = GetCachedStyle(Pipeline->Registry, StyleIndex);
    bit_field        Flags = 0;

    // Constants Flags
    {
        SetFlag(Flags, UILayoutNode_PlaceChildrenX);
    }

    InitializeLayoutNode(Style, UILayoutNode_Header, Flags, Pipeline->LayoutTree);
}

internal void
UIScrollView(u32 StyleIndex, ui_pipeline *Pipeline)
{
    ui_cached_style *Style = GetCachedStyle(Pipeline->Registry, StyleIndex);
    bit_field        Flags = 0;

    // Constants Flags
    {
        SetFlag(Flags, UILayoutNode_PlaceChildrenY);
        SetFlag(Flags, UILayoutNode_IsParent);
    }

    InitializeLayoutNode(Style, UILayoutNode_ScrollView, Flags, Pipeline->LayoutTree);
}

internal void
UILabel(u32 StyleIndex, byte_string Text, ui_pipeline *Pipeline)
{
    ui_cached_style *Style = GetCachedStyle(Pipeline->Registry, StyleIndex);
    bit_field        Flags = 0;

    // Constants Flags
    {
        if(IsValidByteString(Text))
        {
            SetFlag(Flags, UILayoutNode_TextIsBound);
        }
    }

    ui_layout_node *Node = InitializeLayoutNode(Style, UILayoutNode_Label, Flags, Pipeline->LayoutTree);
    if(IsValidLayoutNode(Node))
    {
        BindText(Node, Text, UIQueryFont(Style), Pipeline->StaticArena);
    }
}
