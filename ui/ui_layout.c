// [HELPERS IMPLEMENTATION]



// [API IMPLEMENTATION]

internal ui_hit_test_result 
UILayout_HitTest(vec2_f32 MousePosition, bit_field Flags, ui_node *LayoutRoot, ui_pipeline *Pipeline)
{
    ui_hit_test_result Result    = {0};
    ui_node           *StyleRoot = UITree_GetStyleNode(LayoutRoot, Pipeline);

    ui_layout_box *Box   = &LayoutRoot->Layout;
    ui_style      *Style = &StyleRoot->Style;


    f32      Radius        = 0.f;
    vec2_f32 FullHalfSize  = Vec2F32(Box->FinalWidth * 0.5f, Box->FinalHeight * 0.5f);
    vec2_f32 Origin        = Vec2F32Add(Vec2F32(Box->FinalX, Box->FinalY), FullHalfSize);
    vec2_f32 LocalPosition = Vec2F32Sub(MousePosition, Origin);

    f32 TargetSDF = RoundedRectSDF(LocalPosition, FullHalfSize, Radius);
    if(TargetSDF <= 0.f)
    {
        // Recurse into all of the children. Respects draw order.
        for(ui_node *Child = LayoutRoot->Last; Child != 0; Child = Child->Prev)
        {
            Result = UILayout_HitTest(MousePosition, Flags, Child, Pipeline);
            if(Result.Success)
            {
                return Result;
            }
        }

        Result.LayoutNode = LayoutRoot;
        Result.HoverState = UIHover_Target;
        Result.Success    = 1;

        if(HasFlag(Flags, UIHitTest_CheckForResize))
        {
            f32      BorderWidth        = Style->BorderWidth;
            vec2_f32 BorderWidthVector  = Vec2F32(BorderWidth, BorderWidth);
            vec2_f32 HalfSizeWithBorder = Vec2F32Sub(FullHalfSize, BorderWidthVector);

            f32 BorderSDF = RoundedRectSDF(LocalPosition, HalfSizeWithBorder, Radius);
            if(BorderSDF >= 0.f)
            {
                f32 CornerTolerance = 5.f;                            // The higher this is, the greedier corner detection is.
                f32 ResizeBorderX   = Box->FinalX + Box->FinalWidth;  // Right  Border
                f32 ResizeBorderY   = Box->FinalY + Box->FinalHeight; // Bottom Border

                if(MousePosition.X >= (ResizeBorderX - BorderWidth))
                {
                    if(MousePosition.Y >= (Origin.Y + FullHalfSize.Y - CornerTolerance))
                    {
                        Result.HoverState = UIHover_ResizeCorner;
                    }
                    else
                    {
                        Result.HoverState = UIHover_ResizeX;
                    }
                }
                else if(MousePosition.Y >= (ResizeBorderY - BorderWidth))
                {
                    if(MousePosition.X >= (Origin.X + FullHalfSize.X - CornerTolerance))
                    {
                        Result.HoverState = UIHover_ResizeCorner;
                    }
                    else
                    {
                        Result.HoverState = UIHover_ResizeY;
                    }
                 }
            }
         }
    }

    return Result;
}

internal void
UILayout_DragSubtree(vec2_f32 Delta, ui_node *LayoutRoot, ui_pipeline *Pipeline)
{   Assert(Delta.X != 0.f || Delta.Y != 0.f);

    LayoutRoot->Layout.FinalX += Delta.X;
    LayoutRoot->Layout.FinalY += Delta.Y;

    for (ui_node *Child = LayoutRoot->First; Child != 0; Child = Child->Next)
    {
        UILayout_DragSubtree(Delta, Child, Pipeline);
    }
}

internal void
UILayout_ResizeSubtree(vec2_f32 Delta, ui_node *LayoutNode, ui_pipeline *Pipeline)
{
    ui_node *StyleNode = UITree_GetStyleNode(LayoutNode, Pipeline);
    Assert(StyleNode->Style.Size.X.Type != UIUnit_Percent);
    Assert(StyleNode->Style.Size.Y.Type != UIUnit_Percent);

    StyleNode->Style.Size.X.Float32 += Delta.X;
    StyleNode->Style.Size.Y.Float32 += Delta.Y;

    UILayout_ComputeParentToChildren(LayoutNode, Pipeline);
}

internal void
UILayout_ComputeParentToChildren(ui_node *LayoutRoot, ui_pipeline *Pipeline)
{
    if (Pipeline->LayoutTree)
    {
        node_queue Queue = { 0 };
        {
            typed_queue_params Params = { 0 };
            Params.QueueSize = Pipeline->LayoutTree->NodeCount;

            Queue = BeginNodeQueue(Params, Pipeline->LayoutTree->Arena);
        }

        if (Queue.Data)
        {
            ui_layout_box *RootBox = &LayoutRoot->Layout;

            if (RootBox->Width.Type != UIUnit_Float32)
            {
                return; // TODO: Error.
            }

            RootBox->FinalWidth  = RootBox->Width.Float32;
            RootBox->FinalHeight = RootBox->Height.Float32;

            PushNodeQueue(&Queue, LayoutRoot);

            while (!IsNodeQueueEmpty(&Queue))
            {
                ui_node *Current = PopNodeQueue(&Queue);
                ui_layout_box *Box = &Current->Layout;

                f32 AvWidth    = Box->FinalWidth - (Box->Padding.Left + Box->Padding.Right);
                f32 AvHeight   = Box->FinalHeight - (Box->Padding.Top + Box->Padding.Bot);
                f32 UsedWidth  = 0;
                f32 UsedHeight = 0;
                f32 BasePosX   = Box->FinalX + Box->Padding.Left;
                f32 BasePosY   = Box->FinalY + Box->Padding.Top;

                {
                    for (ui_node *ChildNode = Current->First; (ChildNode != 0 && UsedWidth <= AvWidth); ChildNode = ChildNode->Next)
                    {
                        ChildNode->Layout.FinalX = BasePosX + UsedWidth;
                        ChildNode->Layout.FinalY = BasePosY + UsedHeight;

                        ui_layout_box *ChildBox = &ChildNode->Layout;

                        f32 Width = 0;
                        f32 Height = 0;
                        {
                            if (ChildBox->Width.Type == UIUnit_Percent)
                            {
                                Width = (ChildBox->Width.Percent / 100.f) * AvWidth;
                            }
                            else if (ChildBox->Width.Type == UIUnit_Float32)
                            {
                                Width = ChildBox->Width.Float32 <= AvWidth ? ChildBox->Width.Float32 : 0.f;
                            }
                            else
                            {
                                return;
                            }

                            if (ChildBox->Height.Type == UIUnit_Percent)
                            {
                                Height = (ChildBox->Height.Percent / 100.f) * AvHeight;
                            }
                            else if (ChildBox->Height.Type == UIUnit_Float32)
                            {
                                Height = ChildBox->Height.Float32 <= AvHeight ? ChildBox->Height.Float32 : 0.f;
                            }
                            else
                            {
                                return;
                            }

                            ChildBox->FinalWidth  = Width;
                            ChildBox->FinalHeight = Height;
                        }

                        if (Box->Flags & UILayoutNode_PlaceChildrenVertically)
                        {
                            UsedHeight += Height + Box->Spacing.Vertical;
                        }
                        else
                        {
                            UsedWidth += Width + Box->Spacing.Horizontal;
                        }

                        PushNodeQueue(&Queue, ChildNode);
                    }
                }

                // Text Position
                // TODO: Text wrapping and stuff.
                // TODO: Text Alignment.
                {
                    if (Box->Flags & UILayoutNode_DrawText)
                    {
                        ui_text *Text    = Box->Text;
                        vec2_f32 TextPos = Vec2F32(BasePosX, BasePosY);

                        for (u32 Idx = 0; Idx < Text->Size; Idx++)
                        {
                            ui_character *Character = &Text->Characters[Idx];
                            Character->Position.Min.X = roundf(TextPos.X + Character->Layout.Offset.X);
                            Character->Position.Min.Y = roundf(TextPos.Y + Character->Layout.Offset.Y);
                            Character->Position.Max.X = roundf(Character->Position.Min.X + Character->Layout.AdvanceX);
                            Character->Position.Max.Y = roundf(Character->Position.Min.Y + Text->LineHeight);

                            TextPos.X += (Character->Position.Max.X) - (Character->Position.Min.X);
                        }
                    }
                }

            }

            // NOTE: Not really clear that we are clearing the queue...
            PopArena(Pipeline->LayoutTree->Arena, Pipeline->LayoutTree->NodeCount * sizeof(ui_node *));
        }
        else
        {
            OSLogMessage(byte_string_literal("Failed to allocate queue for (Top Down Layout) ."), OSMessage_Error);
        }
    }
    else
    {
        OSLogMessage(byte_string_literal("Unable to compute (Top Down Layout), because one of the trees is invalid."), OSMessage_Error);
    }
}