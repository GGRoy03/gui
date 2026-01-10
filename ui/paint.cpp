#pragma once

// -----------------------------------------------------------------------------
// Render Commands
// -----------------------------------------------------------------------------

#define GUI_MAX_COMMAND_PER_NODE 2u


static gui_memory_footprint
GuiGetRenderCommandsFootprint(gui_layout_tree *Tree)
{
    gui_memory_footprint Result;
    Result.SizeInBytes = 0;
    Result.Alignment  = 0;

    if (GuiIsValidLayoutTree(Tree))
    {
        uint64_t CommandSize = Tree->NodeBuffer.Count * GUI_MAX_COMMAND_PER_NODE * sizeof(gui_render_command);
        uint64_t QueueSize   = Tree->NodeBuffer.Count * sizeof(uint32_t);

        Result.SizeInBytes = CommandSize + QueueSize;
        Result.Alignment   = AlignOf(gui_render_command);
    }

    return Result;
}


static gui_render_command_list
GuiComputeRenderCommands(gui_layout_tree *Tree, gui_memory_block Block)
{
    gui_render_command_list Result;
    Result.Commands = NULL;
    Result.Count    = 0;

    gui_memory_region Local = GuiEnterMemoryRegion(Block);

    if (GuiIsValidLayoutTree(Tree) && GuiIsValidMemoryRegion(&Local))
    {
        Result.Commands = GuiPushArray(&Local, gui_render_command, Tree->NodeBuffer.Count * GUI_MAX_COMMAND_PER_NODE);
        Result.Count    = 0;

        uint32_t *NodeQueue = GuiPushArray(&Local, uint32_t, Tree->NodeBuffer.Count);
        uint32_t  Head      = 0;
        uint32_t  Tail      = 0;

        NodeQueue[Tail++] = (uint32_t)Tree->RootIndex;

        while (Head < Tail)
        {
            uint32_t         NodeIndex = NodeQueue[Head++];
            gui_layout_node *Node      = GuiGetLayoutNode(Tree->NodeBuffer, NodeIndex);
            GUI_ASSERT(Node);

            for (uint32_t Child = Node->First; Child != GuiInvalidIndex; Child = GuiGetLayoutNode(Tree->NodeBuffer, Child)->Next)
            {
                NodeQueue[Tail++] = Child;
            }

            gui_paint_properties *Paint = &Tree->PaintBuffer[Node->Index];

            gui_color         Color        = Paint->Color;
            gui_color         BorderColor  = Paint->BorderColor;
            float             BorderWidth  = Paint->BorderWidth;
            gui_corner_radius CornerRadius = Paint->CornerRadius;

            if (Node->State & Gui_NodeState_UseFocusedStyle)
            {
                Color        = Paint->FocusedColor;
                BorderColor  = Paint->FocusedBorderColor;
                BorderWidth  = Paint->FocusedBorderWidth;
                CornerRadius = Paint->FocusedCornerRadius;
            }
            else if (Node->State & Gui_NodeState_UseHoveredStyle)
            {
                Color        = Paint->HoveredColor;
                BorderColor  = Paint->HoveredBorderColor;
                BorderWidth  = Paint->HoveredBorderWidth;
                CornerRadius = Paint->HoveredCornerRadius;

                Node->State = (Node->State & ~Gui_NodeState_UseHoveredStyle);
            }

            if (Color.A > 0.0f)
            {
                gui_render_command *Command = &Result.Commands[Result.Count++];

                Command->Type = Gui_RenderCommandType_Rectangle;
                Command->Box  = GuiGetLayoutNodeBoundingBox(Node);

                Command->Rect.Color        = Color;
                Command->Rect.CornerRadius = CornerRadius;
            }

            if (BorderWidth > 0.0f)
            {
                gui_render_command *Command = &Result.Commands[Result.Count++];

                Command->Type = Gui_RenderCommandType_Border;
                Command->Box  = GuiGetLayoutNodeBoundingBox(Node);

                Command->Border.Color        = BorderColor;
                Command->Border.CornerRadius = CornerRadius;
                Command->Border.Width        = BorderWidth;
            }
        }
    }

    return Result;
}
