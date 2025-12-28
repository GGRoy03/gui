constexpr uint32_t MAX_COMMAND_PER_NODE = 2;

static memory_footprint
GetRenderCommandsFootprint(ui_layout_tree *Tree)
{
    memory_footprint Result = {};

    if(IsValidLayoutTree(Tree))
    {
        uint64_t CommandSize = Tree->NodeBuffer.Count * MAX_COMMAND_PER_NODE * sizeof(render_command);
        uint64_t QueueSize   = Tree->NodeBuffer.Count * sizeof(uint32_t);

        Result.SizeInBytes = CommandSize + QueueSize;
        Result.Alignment   = AlignOf(render_command);
    }

    return Result;
}

static render_command_list
ComputeRenderCommands(ui_layout_tree *Tree, memory_block Block)
{
    render_command_list Result = {};
    memory_region       Local  = EnterMemoryRegion(Block);

    if (IsValidLayoutTree(Tree) && IsValidMemoryRegion(Local))
    {
        Result.Commands = PushArray<render_command>(Local, Tree->NodeBuffer.Count * MAX_COMMAND_PER_NODE);
        Result.Count = 0;

        uint32_t *NodeQueue = PushArray<uint32_t>(Local, Tree->NodeBuffer.Count);
        uint32_t  Head      = 0;
        uint32_t  Tail      = 0;

        NodeQueue[Tail++] = Tree->RootIndex;

        while (Head < Tail)
        {
            uint32_t        NodeIndex = NodeQueue[Head++];
            ui_layout_node *Node      = GetLayoutNode(Tree->NodeBuffer, NodeIndex);
            VOID_ASSERT(Node);

            for (uint32_t Child = Node->First; Child != InvalidIndex; Child = GetLayoutNode(Tree->NodeBuffer, Child)->Next)
            {
                NodeQueue[Tail++] = Child;
            }

            // This part is still a bit messy.

            const paint_properties &Paint = Tree->PaintBuffer[Node->Index];
            color         Color        = Paint.Color;
            color         BorderColor  = Paint.BorderColor;
            float         BorderWidth  = Paint.BorderWidth;
            corner_radius CornerRadius = Paint.CornerRadius;

            if ((Node->State & NodeState::UseFocusedStyle) != NodeState::None)
            {
                Color        = Paint.FocusedColor;
                BorderColor  = Paint.FocusedBorderColor;
                BorderWidth  = Paint.FocusedBorderWidth;
                CornerRadius = Paint.FocusedCornerRadius;
            } else
            if ((Node->State & NodeState::UseHoveredStyle) != NodeState::None)
            {
                Color        = Paint.HoveredColor;
                BorderColor  = Paint.HoveredBorderColor;
                BorderWidth  = Paint.HoveredBorderWidth;
                CornerRadius = Paint.HoveredCornerRadius;
    
                Node->State &= ~NodeState::UseHoveredStyle;
            }

            if (Color.A > 0.0f)
            {
                render_command &Command = Result.Commands[Result.Count++];

                Command.Type = RenderCommandType::Rectangle;
                Command.Box  = GetLayoutNodeOuterRect(Node);

                Command.Rect.Color        = Color;
                Command.Rect.CornerRadius = CornerRadius;
            }

            if (BorderWidth > 0.0f)
            {
                render_command &Command = Result.Commands[Result.Count++];

                Command.Type = RenderCommandType::Border;
                Command.Box  = GetLayoutNodeOuterRect(Node);

                Command.Border.Color        = BorderColor;
                Command.Border.CornerRadius = CornerRadius;
                Command.Border.Width        = BorderWidth;
            }
        }
    }

    return Result;
}
