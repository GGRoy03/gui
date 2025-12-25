#pragma once

// =============================================================================
// GLOBAL: Template Specializations
// =============================================================================

namespace layout
{

enum class NodeState : uint32_t;

}

template<>
struct enable_bitmask_operators<layout::NodeState> : std::true_type {};

template<>
struct enable_bitmask_operators<layout::NodeFlags> : std::true_type {};

namespace layout
{

// =============================================================================
// DOMAIN: Layout Tree & Nodes
// =============================================================================


enum class NodeState : uint32_t
{
    None = 0,
    UseHoveredStyle    = 1 << 0,
    UseFocusedStyle    = 1 << 1,
    HasCapturedPointer = 1 << 2,
    IsClicked          = 1 << 3,
};

struct axes
{
    float Width;
    float Height;
};


struct layout_node
{
    uint32_t Parent;
    uint32_t First;
    uint32_t Last;
    uint32_t Next;
    uint32_t Prev;
    uint32_t ChildCount;
    uint32_t Index;

    vec2_float      OutputPosition;
    axes         OutputSize;
    axes         OutputChildSize;

    size          Size;
    size          MinSize;
    size          MaxSize;
    Alignment        XAlign;
    Alignment        YAlign;
    LayoutDirection  Direction;
    core::padding Padding;
    float            Spacing;

    vec2_float VisualOffset;
    vec2_float DragOffset;
    vec2_float ScrollOffset;

    NodeState  State;
    NodeFlags  Flags;
    uint32_t   StyleIndex;

    static bool IsValid(const layout_node *Node)
    {
        return Node && Node->Index != InvalidIndex;
    }
};

struct layout_tree
{
    uint64_t          NodeCapacity;
    uint64_t          NodeCount;
    layout_node   *Nodes;
    uint32_t          RootIndex;
    uint32_t          CapturedNodeIndex;
    paint_command *PaintBuffer;

    static bool IsValid(const layout_tree *Tree)
    {
        return Tree && Tree->Nodes && Tree->NodeCount <= Tree->NodeCapacity && Tree->PaintBuffer;
    }

    layout_node * GetSentinel()
    {
        layout_node *Result = Nodes + NodeCapacity;
        return Result;
    }

    layout_node * GetRootNode()
    {
        layout_node *Result = Nodes + RootIndex;
        return Result;
    }

    layout_node * GetNode(uint32_t Index)
    {
        layout_node *Result = 0;
        if(Index < NodeCapacity)
        {
            Result = Nodes + Index;
        }
        return Result;
    }

    layout_node * GetFreeNode()
    {
        layout_node *Sentinel = GetSentinel();
        layout_node *Result   = 0;

        if (Sentinel->Next != InvalidIndex)
        {
            VOID_ASSERT(NodeCount < NodeCapacity);

            Result = GetNode(Sentinel->Next);
            Result->Index  = Sentinel->Next;
            Sentinel->Next = Result->Next;

            Result->First      = InvalidIndex;
            Result->Last       = InvalidIndex;
            Result->Prev       = InvalidIndex;
            Result->Next       = InvalidIndex;
            Result->Parent     = InvalidIndex;
            Result->ChildCount = 0;

            if(NodeCount == 0)
            {
                RootIndex = Result->Index;
            }

            ++NodeCount;
        }

        return Result;
    }

    void FreeNode(layout_node *Node)
    {
        VOID_ASSERT(layout_node::IsValid(Node));

        if(layout_node::IsValid(Node))
        {
            layout_node *Sentinel = GetSentinel();
            VOID_ASSERT(Sentinel);

            layout_node *Prev = GetNode(Node->Prev);
            if(layout_node::IsValid(Prev))
            {
                Prev->Next = Node->Next;
            }

            layout_node *Next = GetNode(Node->Next);
            if(layout_node::IsValid(Next))
            {
                Next->Prev = Node->Prev;
            }

            layout_node *Parent = GetNode(Node->Parent);
            if(layout_node::IsValid(Parent))
            {
                if(Parent->First == Node->Index)
                {
                    Parent->First = Node->Next;
                }

                if(Parent->Last == Node->Index)
                {
                    Parent->Last = Node->Prev;
                }

                --Parent->ChildCount;
            }

            Node->Next     = Sentinel->Next;
            Sentinel->Next = Node->Index;

            Node->Index      = InvalidIndex;
            Node->First      = InvalidIndex;
            Node->Last       = InvalidIndex;
            Node->Prev       = InvalidIndex;
            Node->Parent     = InvalidIndex;
            Node->ChildCount = 0;

            --NodeCount;
        }
    }

    void Append(layout_node *Parent, layout_node *Child)
    {
        if(layout_node::IsValid(Parent) && layout_node::IsValid(Child))
        {
            Child->Parent = Parent->Index;

            layout_node *First = GetNode(Parent->First);
            if(!layout_node::IsValid(First))
            {
                Parent->First = Child->Index;
            }

            layout_node *Last = GetNode(Parent->Last);
            if(layout_node::IsValid(Last))
            {
                Last->Next  = Child->Index;
            }

            Child->Prev         = Parent->Last;
            Parent->Last        = Child->Index;
            Parent->ChildCount += 1;
        }
    }
};

// --- Private Helpers ---

static uint64_t GetLayoutTreeAlignment(void)
{
    uint64_t Result = AlignOf(layout_node);
    return Result;
}

static uint64_t GetLayoutTreeFootprint(uint64_t NodeCount)
{
    uint64_t ArraySize = (NodeCount + 1) * sizeof(layout_node);
    uint64_t PaintSize = NodeCount * sizeof(paint_command);
    uint64_t Result    = sizeof(layout_tree) + ArraySize + PaintSize;
    return Result;
}

static layout_tree * PlaceLayoutTreeInMemory(uint64_t NodeCount, void *Memory)
{
    layout_tree *Result = 0;

    if (Memory)
    {
        layout_node   *Nodes       = static_cast<layout_node*>(Memory);
        paint_command *PaintBuffer = reinterpret_cast<paint_command *>(Nodes + NodeCount + 1);

        Result = reinterpret_cast<layout_tree *>(PaintBuffer + NodeCount);
        Result->Nodes        = Nodes;
        Result->PaintBuffer  = PaintBuffer;
        Result->NodeCount    = 0;
        Result->NodeCapacity = NodeCount;

        for (uint64_t Idx = 0; Idx < Result->NodeCapacity; Idx++)
        {
            layout_node *Node = Result->GetNode(Idx);
            VOID_ASSERT(Node);

            Node->Index      = InvalidIndex;
            Node->First      = InvalidIndex;
            Node->Last       = InvalidIndex;
            Node->Parent     = InvalidIndex;
            Node->Prev       = InvalidIndex;
            Node->ChildCount = 0;

            Node->Next = Idx + 1;
        }

        layout_node *Sentinel = Result->GetSentinel();
        Sentinel->Next  = 0;
        Sentinel->Index = InvalidIndex;
    }

    return Result;
}

static void UpdateLayoutInput(uint32_t NodeIndex, uint32_t StyleIndex, const cached_style &Cached, layout_tree *Tree)
{
    if(layout_tree::IsValid(Tree))
    {
        layout_node *Node = Tree->GetNode(NodeIndex);
        if(layout_node::IsValid(Node))
        {
            Node->StyleIndex  = StyleIndex;
    
            Node->Size      = Cached.Default.Size.Value;
            Node->MinSize   = Cached.Default.MinSize.Value;
            Node->MaxSize   = Cached.Default.MaxSize.Value;
            Node->Direction = Cached.Default.Direction.Value;
            Node->XAlign    = Cached.Default.XAlign.Value;
            Node->YAlign    = Cached.Default.YAlign.Value;
            Node->Padding   = Cached.Default.Padding.Value;
            Node->Spacing   = Cached.Default.Spacing.Value;
    
            if(!Cached.Default.MinSize.IsSet)
            {
                Node->MinSize = Node->Size;
            }
    
            if(!Cached.Default.MaxSize.IsSet)
            {
                Node->MaxSize = Node->Size;
            }
        }
    }
}

// --- Public API ---

static rect_float GetNodeOuterRect(const layout_node *Node)
{
    rect_float Result = {};

    if(layout_node::IsValid(Node))
    {
        vec2_float Screen = vec2_float(Node->OutputPosition.X, Node->OutputPosition.Y) + Node->ScrollOffset;
        Result = rect_float::FromXYWH(Screen.X, Screen.Y, Node->OutputSize.Width, Node->OutputSize.Height);
    }

    return Result;
}

static rect_float GetNodeInnerRect(layout_node *Node)
{
    vec2_float Screen = vec2_float(Node->OutputPosition.X, Node->OutputPosition.Y) + Node->ScrollOffset;
    rect_float Result = rect_float::FromXYWH(Screen.X, Screen.Y, Node->OutputSize.Width, Node->OutputSize.Height);
    return Result;
}

static rect_float GetNodeContentRect(layout_node *Node)
{
    vec2_float Screen = vec2_float(Node->OutputPosition.X, Node->OutputPosition.Y) + Node->ScrollOffset;
    rect_float Result = rect_float::FromXYWH(Screen.X, Screen.Y, Node->OutputSize.Width, Node->OutputSize.Height);
    return Result;
}

static uint32_t CreateNode(NodeFlags Flags, layout_tree *Tree)
{
    uint32_t Result = InvalidIndex;

    if(layout_tree::IsValid(Tree))
    {
        layout_node *Node = Tree->GetFreeNode();

        if(layout_node::IsValid(Node))
        {
            VOID_ASSERT(Node->First  == InvalidIndex);
            VOID_ASSERT(Node->Last   == InvalidIndex);
            VOID_ASSERT(Node->Prev   == InvalidIndex);
            VOID_ASSERT(Node->Next   == InvalidIndex);
            VOID_ASSERT(Node->Parent == InvalidIndex);

            Node->Flags = Flags;
    
            Result = Node->Index;
        }
    }

    return Result;
}

static uint32_t FindChild(uint32_t NodeIndex, uint32_t FindIndex, layout_tree *Tree)
{
    uint32_t Result = InvalidIndex;

    if(layout_tree::IsValid(Tree))
    {
        layout_node *Node = Tree->GetNode(NodeIndex);
        if(layout_node::IsValid(Node))
        {
            layout_node *Child = Tree->GetNode(Node->First);
            while(layout_node::IsValid(Child) && FindIndex--)
            {
                Child = Tree->GetNode(Child->Next);
            }

            Result = Child->Index;
        }
    }

    return Result;
}

static bool AppendChild(uint32_t ParentIndex, uint32_t ChildIndex, layout_tree *Tree)
{
    bool Result = false;

    if(layout_tree::IsValid(Tree) && ParentIndex != ChildIndex)
    {
        layout_node *Parent = Tree->GetNode(ParentIndex);
        layout_node *Child  = Tree->GetNode(ChildIndex);

        if(layout_node::IsValid(Parent) && layout_node::IsValid(Child))
        {
            Tree->Append(Parent, Child);
        }
    }

    return Result;
}

static void SetNodeOffset(uint32_t NodeIndex, float XOffset, float YOffset, layout_tree *Tree)
{
    if(layout_tree::IsValid(Tree))
    {
        layout_node *Node = Tree->GetNode(NodeIndex);
        if(layout_node::IsValid(Node))
        {
            Node->VisualOffset.X = XOffset;
            Node->VisualOffset.Y = YOffset;
        }
    }
}

// =============================================================================
// DOMAIN: Scroll Region
// =============================================================================

struct scroll_region
{
    vec2_float     ContentSize;
    float          ScrollOffset;
    float          PixelPerLine;
    core::AxisType Axis;
};

// --- Private Helpers ---

static void UpdateScrollNode(float ScrolledLines, layout_node *Node, layout_tree *Tree, scroll_region *Region)
{
    float      ScrolledPixels = ScrolledLines * Region->PixelPerLine;
    vec2_float WindowSize     = vec2_float(Node->OutputSize.Width, Node->OutputSize.Height);

    float ScrollLimit = 0.f;
    if (Region->Axis == core::AxisType::X)
    {
        ScrollLimit = -(Region->ContentSize.X - WindowSize.X);
    } else
    if (Region->Axis == core::AxisType::Y)
    {
        ScrollLimit = -(Region->ContentSize.Y - WindowSize.Y);
    }

    Region->ScrollOffset += ScrolledPixels;
    Region->ScrollOffset  = ClampTop(ClampBot(ScrollLimit, Region->ScrollOffset), 0);

    vec2_float ScrollDelta = vec2_float(0.f, 0.f);
    if(Region->Axis == core::AxisType::X)
    {
        ScrollDelta.X = -1.f * Region->ScrollOffset;
    } else
    if(Region->Axis == core::AxisType::Y)
    {
        ScrollDelta.Y = -1.f * Region->ScrollOffset;
    }

    rect_float WindowContent = GetNodeOuterRect(Node);
    for(layout_node *Child = Tree->GetNode(Node->First); layout_node::IsValid(Child); Child = Tree->GetNode(Child->Next))
    {
        Child->ScrollOffset = vec2_float(-ScrollDelta.X, -ScrollDelta.Y);

        vec2_float FixedContentSize = vec2_float(Child->OutputSize.Width, Child->OutputSize.Height);
        rect_float ChildContent     = GetNodeOuterRect(Child);

        if (FixedContentSize.X > 0.0f && FixedContentSize.Y > 0.0f) 
        {
            if (WindowContent.IsIntersecting(ChildContent))
            {
                // TODO: Prune?
            }
            else
            {
                // TODO: Unprune?
            }
        }
    }
}

// --- Public API ---

static uint64_t GetScrollRegionFootprint(void)
{
    uint64_t Result = sizeof(scroll_region);
    return Result;
}

static scroll_region * PlaceScrollRegionInMemory(scroll_region_params Params, void *Memory)
{
    scroll_region *Result = 0;

    if(Memory)
    {
        Result = (scroll_region *)Memory;
        Result->ContentSize  = vec2_float(0.f, 0.f);
        Result->ScrollOffset = 0.f;
        Result->PixelPerLine = Params.PixelPerLine;
        Result->Axis         = Params.Axis;
    }

    return Result;
}

static vec2_float GetScrollNodeTranslation(scroll_region *Region)
{
    vec2_float Result = vec2_float(0.f, 0.f);

    if (Region->Axis == core::AxisType::X)
    {
        Result = vec2_float(Region->ScrollOffset, 0.f);
    } else
    if (Region->Axis == core::AxisType::Y)
    {
        Result = vec2_float(0.f, Region->ScrollOffset);
    }

    return Result;
}

// =============================================================================
// DOMAIN: Input Handling
// =============================================================================

// --- Private Helpers ---

static bool IsMouseInsideOuterBox(vec2_float MousePosition, layout_node *Node)
{
    VOID_ASSERT(MousePosition.X >= 0.f && MousePosition.Y >= 0.f);
    VOID_ASSERT(layout_node::IsValid(Node));

    vec2_float OuterSize  = vec2_float(Node->OutputSize.Width, Node->OutputSize.Height);
    vec2_float OuterPos   = vec2_float(Node->OutputPosition.X, Node->OutputPosition.Y) + Node->ScrollOffset;
    vec2_float OuterHalf  = vec2_float(OuterSize.X * 0.5f, OuterSize.Y * 0.5f);
    vec2_float Center     = OuterPos + OuterHalf;
    vec2_float LocalMouse = MousePosition - Center;

    rect_sdf_params Params = {0};
    Params.HalfSize      = OuterHalf;
    Params.PointPosition = LocalMouse;

    float Distance = RoundedRectSDF(Params);
    return Distance <= 0.f;
}

static bool IsMouseInsideBorder(vec2_float MousePosition, layout_node *Node)
{
    VOID_ASSERT(MousePosition.X >= 0.f && MousePosition.Y >= 0.f);
    VOID_ASSERT(layout_node::IsValid(Node));

    vec2_float InnerSize   = vec2_float(Node->OutputSize.Width, Node->OutputSize.Height) - vec2_float(0.f, 0.f);
    vec2_float InnerPos    = vec2_float(Node->OutputPosition.X, Node->OutputPosition.Y ) - vec2_float(0.f, 0.f) + Node->ScrollOffset;
    vec2_float InnerHalf   = vec2_float(InnerSize.X * 0.5f, InnerSize.Y * 0.5f);
    vec2_float InnerCenter = InnerPos + InnerHalf;

    rect_sdf_params Params = {0};
    Params.Radius        = 0.f;
    Params.HalfSize      = InnerHalf;
    Params.PointPosition = MousePosition - InnerCenter;

    float Distance = RoundedRectSDF(Params);
    return Distance >= 0.f;
}

// --- Public API ---

static bool HandlePointerClick(vec2_float Position, uint32_t ClickMask, uint32_t NodeIndex, layout_tree *Tree)
{
    VOID_ASSERT(Position.X >= 0.f && Position.Y >= 0.f);
    VOID_ASSERT(layout_tree::IsValid(Tree));

    if(layout_tree::IsValid(Tree))
    {
        layout_node *Node = Tree->GetNode(NodeIndex);
        VOID_ASSERT(layout_node::IsValid(Node));

        if(layout_node::IsValid(Node))
        {
            if(IsMouseInsideOuterBox(Position, Node))
            {
                for(layout_node *Child = Tree->GetNode(Node->First); layout_node::IsValid(Child); Child = Tree->GetNode(Child->Next))
                {
                    bool IsHandled = HandlePointerClick(Position, ClickMask, Child->Index, Tree);
    
                    if(IsHandled)
                    {
                        return true;
                    }
                }
    
                Node->State |= NodeState::IsClicked;
                Node->State |= NodeState::UseFocusedStyle;
                Node->State |= NodeState::HasCapturedPointer;
    
                Tree->CapturedNodeIndex = NodeIndex;
    
                return true;
            }
        }
    }

    return false;
}

static bool HandlePointerRelease(vec2_float Position, uint32_t ButtonMask, uint32_t NodeIndex, layout_tree *Tree)
{
    VOID_ASSERT(Position.X >= 0.f && Position.Y >= 0.f);
    VOID_ASSERT(layout_tree::IsValid(Tree));

    if(layout_tree::IsValid(Tree))
    {
        layout_node *Node = Tree->GetNode(NodeIndex);
        VOID_ASSERT(layout_node::IsValid(Node));

        for(layout_node *Child = Tree->GetNode(Node->First); layout_node::IsValid(Child); Child = Tree->GetNode(Child->Next))
        {
            if(HandlePointerRelease(Position, ButtonMask, Child->Index, Tree))
            {
                return true;
            }
        }

        if((Node->State & NodeState::HasCapturedPointer) != NodeState::None)
        {
            Node->State &= ~(NodeState::HasCapturedPointer | NodeState::UseFocusedStyle);

            Tree->CapturedNodeIndex = InvalidIndex;

            return true;
        }
    }

    return false;
}

static bool HandlePointerHover(vec2_float Position, uint32_t NodeIndex, layout_tree *Tree)
{
    VOID_ASSERT(Position.X >= 0.f && Position.Y >= 0.f);
    VOID_ASSERT(layout_tree::IsValid(Tree));

    if(layout_tree::IsValid(Tree))
    {
        layout_node *Node = Tree->GetNode(NodeIndex);
        VOID_ASSERT(layout_node::IsValid(Node));

        if(IsMouseInsideOuterBox(Position, Node))
        {
            for(layout_node *Child = Tree->GetNode(Node->First); layout_node::IsValid(Child); Child = Tree->GetNode(Child->Next))
            {
                bool IsHandled = HandlePointerHover(Position, Child->Index, Tree);
                if(IsHandled)
                {
                    return true;
                }
            }

            Node->State |= NodeState::UseHoveredStyle;

            return true;
        }
    }

    return false;
}

static void HandlePointerMove(vec2_float Delta, layout_tree *Tree)
{
    VOID_ASSERT(layout_tree::IsValid(Tree));
    
    if(layout_tree::IsValid(Tree))
    {
        layout_node *CapturedNode = Tree->GetNode(Tree->CapturedNodeIndex);
        if(layout_node::IsValid(CapturedNode))
        {
            if((CapturedNode->Flags & NodeFlags::IsDraggable) != NodeFlags::None)
            {
                CapturedNode->OutputPosition.X += Delta.X;
                CapturedNode->OutputPosition.Y += Delta.Y;
            }
        }
    }
}

// =============================================================================
// DOMAIN: Layout Computation
// =============================================================================

// --- Private Helpers ---

static float GetAlignmentOffset(Alignment AlignmentType, float FreeSpace)
{
    float Result = 0.f;

    if(FreeSpace > 0)
    {
        switch(AlignmentType)
        {
        case Alignment::None:   
        case Alignment::Start:
        {
            // No-Op
        } break;
    
        case Alignment::Center:
        {
            Result = 0.5f * FreeSpace;
        } break;
    
        case Alignment::End:
        {
            VOID_ASSERT(!"Please Implement :)");
        } break;
        }
    }

    return Result;
}

static uint32_t WrapText(text *Text, layout_node *Node)
{
    VOID_ASSERT(Text);
    VOID_ASSERT(layout_node::IsValid(Node));

    uint32_t LineCount = 1;
    float    LineWidth = 0.f;

    for(uint32_t WordIdx = 0; WordIdx < Text->WordCount; ++WordIdx)
    {
        text_word Word     = Text->Words[WordIdx];
        float        Required = Word.Advance + Word.LeadingWhitespaceAdvance;

        if(LineWidth + Required > Node->OutputSize.Width)
        {
            if(WordIdx > 0)
            {
                text_word LastWord = Text->Words[WordIdx - 1];
                Text->Shaped[LastWord.LastGlyph].BreakLine = true;

                float    Remaining = Word.LeadingWhitespaceAdvance;
                uint64_t SpaceIdx  = LastWord.LastGlyph + 1;

                while (Remaining)
                {
                    Text->Shaped[SpaceIdx].Skip = true;
                    Remaining -= Text->Shaped[SpaceIdx++].Advance;
                }
            }
            else
            {
                VOID_ASSERT(!"Unsure");
            }

            LineWidth  = Word.Advance;
            LineCount += 1;
        }
        else
        {
            LineWidth += Required;
        }
    }

    return LineCount;
}

static float ComputeNodeSize(sizing Sizing, float ParentSize)
{
    float Result = 0.f;

    switch(Sizing.Type)
    {
    case LayoutSizing::None:
    {
    } break;

    case LayoutSizing::Fixed:
    {
        Result = Sizing.Value;
    } break;

    case LayoutSizing::Percent:
    {
        VOID_ASSERT(Sizing.Value >= 0.f && Sizing.Value <= 100.f);
        Result = (Sizing.Value / 100.f) * ParentSize;
    } break;

    case LayoutSizing::Fit:
    {
        VOID_ASSERT(!"Implement :)");
    } break;
    }

    return Result;
}

static void ComputeLayout(layout_node *Node, layout_tree *Tree, axes ParentBounds, core::resource_table *ResourceTable, bool &Changed)
{
    VOID_ASSERT(layout_node::IsValid(Node));
    VOID_ASSERT(layout_tree::IsValid(Tree));

    Node->OutputChildSize  = {};
    axes LastSize = Node->OutputSize;

    float Width     = ComputeNodeSize(Node->Size.Width    , ParentBounds.Width);
    float MinWidth  = ComputeNodeSize(Node->MinSize.Width , ParentBounds.Width);
    float MaxWidth  = ComputeNodeSize(Node->MaxSize.Width , ParentBounds.Width);
    float Height    = ComputeNodeSize(Node->Size.Height   , ParentBounds.Height);
    float MinHeight = ComputeNodeSize(Node->MinSize.Height, ParentBounds.Height);
    float MaxHeight = ComputeNodeSize(Node->MaxSize.Height, ParentBounds.Height);

    Node->OutputSize = {
        .Width  = Max(MinWidth , Min(Width , MaxWidth )),
        .Height = Max(MinHeight, Min(Height, MaxHeight)),
    };

    if((Node->OutputSize.Width != LastSize.Width || Node->OutputSize.Height != LastSize.Height))
    {
        Changed = true;
    }

    axes ContentBounds = {
        .Width  = Node->OutputSize.Width  - (Node->Padding.Left + Node->Padding.Right),
        .Height = Node->OutputSize.Height - (Node->Padding.Top  + Node->Padding.Bot  ),
    };
    VOID_ASSERT(ContentBounds.Width >= 0 && ContentBounds.Height >= 0);

    for(layout_node *Child = Tree->GetNode(Node->First); layout_node::IsValid(Child); Child = Tree->GetNode(Child->Next))
    {
        ComputeLayout(Child, Tree, ContentBounds, ResourceTable, Changed);

        Node->OutputChildSize.Width  += Child->OutputSize.Width;
        Node->OutputChildSize.Height += Child->OutputSize.Height;
    }

    if(Node->ChildCount > 0)
    {
        Node->OutputChildSize.Width  += Node->Spacing * (Node->ChildCount - 1); 
        Node->OutputChildSize.Height += Node->Spacing * (Node->ChildCount - 1); 
    }

    text *Text = static_cast<text *>(QueryNodeResource(core::ResourceType::Text, Node->Index, Tree, ResourceTable));
    if(Text)
    {
        WrapText(Text, Node);
    }
}

static void PlaceLayout(layout_node *Node, layout_tree *Tree, core::resource_table *ResourceTable)
{
    vec2_float Cursor = {
        Node->OutputPosition.X + Node->Padding.Left,
        Node->OutputPosition.Y + Node->Padding.Top ,
    };

    bool IsXMajor = Node->Direction == LayoutDirection::Horizontal ? true : false;

    auto MajorSize = IsXMajor ? Node->OutputSize.Width  - (Node->Padding.Left + Node->Padding.Right) : Node->OutputSize.Height - (Node->Padding.Top  + Node->Padding.Bot  );
    auto MinorSize = IsXMajor ? Node->OutputSize.Height - (Node->Padding.Top  + Node->Padding.Bot  ) : Node->OutputSize.Width  - (Node->Padding.Left + Node->Padding.Right);

    auto MajorAlignment    = IsXMajor ? Node->XAlign                : Node->YAlign;
    auto MajorChildrenSize = IsXMajor ? Node->OutputChildSize.Width : Node->OutputChildSize.Height;
    auto MajorOffset       = GetAlignmentOffset(MajorAlignment, (MajorSize - MajorChildrenSize));

    if(IsXMajor)
    {
        Cursor.X += MajorOffset;
    }
    else
    {
        Cursor.Y += MajorOffset;
    }

    for(layout_node *Child = Tree->GetNode(Node->First); layout_node::IsValid(Child); Child = Tree->GetNode(Child->Next))
    {
        Child->OutputPosition = Cursor;

        float MinorOffset = GetAlignmentOffset(IsXMajor ? Node->XAlign : Node->YAlign, IsXMajor ? (MinorSize - Child->OutputSize.Height) : (MinorSize - Child->OutputSize.Width));

        if(IsXMajor)
        {
            Child->OutputPosition.Y += MinorOffset + Child->VisualOffset.Y;
            Cursor.X                += Child->OutputSize.Width  + Node->Spacing;
        }
        else
        {
            Child->OutputPosition.X += MinorOffset + Child->VisualOffset.X;
            Cursor.Y                += Child->OutputSize.Height + Node->Spacing;
        }

        PlaceLayout(Child, Tree, ResourceTable);
    }

    auto *Text = static_cast<text *>(QueryNodeResource(core::ResourceType::Text, Node->Index, Tree, ResourceTable));
    if(Text)
    {
        VOID_ASSERT(Node->ChildCount == 0);

        vec2_float TextCursor   = vec2_float(Node->OutputPosition.X + Node->Padding.Left, Node->OutputPosition.Y + Node->Padding.Top);
        float      CursorStartX = TextCursor.X;

        for(uint32_t Idx = 0; Idx < Text->ShapedCount; ++Idx)
        {
            shaped_glyph &Glyph = Text->Shaped[Idx];

            if (!Glyph.Skip)
            {
                Glyph.Position = rect_float::FromXYWH(TextCursor.X + Glyph.OffsetX, TextCursor.Y + Glyph.OffsetY, (Glyph.Source.Right - Glyph.Source.Left), (Glyph.Source.Bottom - Glyph.Source.Top));

                TextCursor.X += Glyph.Advance;

                if (Glyph.BreakLine)
                {
                    TextCursor.X  = CursorStartX;
                    TextCursor.Y += 14.f;
                }
            }
        }
    }
}

// --- Public API ---

static void ComputeTreeLayout(layout_tree *Tree)
{
    VOID_ASSERT(layout_tree::IsValid(Tree));

    if(layout_tree::IsValid(Tree))
    {
        core::void_context &Context = core::GetVoidContext();
        layout_node *ActiveRoot = Tree->GetRootNode();
    
        while(true)
        {
            bool Changed = false;
    
            ComputeLayout(ActiveRoot, Tree, ActiveRoot->OutputSize, Context.ResourceTable, Changed);
    
            if (!Changed)
            {
                break;
            }
        }

        PlaceLayout(ActiveRoot, Tree, Context.ResourceTable);
    }
}

// =============================================================================
// DOMAIN: Paint & Rendering
// =============================================================================

// --- Public API ---

static paint_buffer GeneratePaintBuffer(layout_tree *Tree, cached_style *CachedBuffer, memory_arena *Arena)
{
    VOID_ASSERT(layout_tree::IsValid(Tree));
    VOID_ASSERT(CachedBuffer);
    VOID_ASSERT(Arena);

    uint32_t CommandCount = 0;

    if(layout_tree::IsValid(Tree) && CachedBuffer && Arena)
    {
        uint32_t *NodeBuffer = PushArray(Arena, uint32_t, Tree->NodeCount);
        uint32_t  WriteIndex = 0;
        uint32_t  ReadIndex  = 0;
    
        NodeBuffer[WriteIndex++] = 0;
    
        while (NodeBuffer && ReadIndex < WriteIndex)
        {
            uint32_t        NodeIndex = NodeBuffer[ReadIndex++];
            layout_node *Node      = Tree->GetNode(NodeIndex);
    
            if(layout_node::IsValid(Node))
            {
                paint_command   &Command = Tree->PaintBuffer[CommandCount++];
                cached_style    *Cached  = CachedBuffer + Node->StyleIndex;
                paint_properties Paint   = Cached->Default.MakePaintProperties();
    
                if ((Node->State & NodeState::UseFocusedStyle) != NodeState::None)
                {
                    Paint = Cached->Focused.InheritPaintProperties(Paint);
                } else
                if ((Node->State & NodeState::UseHoveredStyle) != NodeState::None)
                {
                    Paint = Cached->Hovered.InheritPaintProperties(Paint);
    
                    Node->State &= ~NodeState::UseHoveredStyle;
                }

                Node->State &= ~NodeState::IsClicked;
    
                Command.Rectangle     = GetNodeOuterRect(Node);
                Command.RectangleClip = {};
    
                Command.TextKey       = MakeNodeResourceKey(core::ResourceType::Text , Node->Index, Tree);
                Command.ImageKey      = MakeNodeResourceKey(core::ResourceType::Image, Node->Index, Tree);
    
                Command.CornerRadius  = Paint.CornerRadius.Value;
                Command.Softness      = Paint.Softness.Value;
                Command.BorderWidth   = Paint.BorderWidth.Value;
                Command.Color         = Paint.Color.Value;
                Command.BorderColor   = Paint.BorderColor.Value;
                Command.TextColor     = Paint.TextColor.Value;

                for(layout_node *Child = Tree->GetNode(Node->First); layout_node::IsValid(Child); Child = Tree->GetNode(Child->Next))
                {
                    NodeBuffer[WriteIndex++] = Child->Index;
                }
            }
            else
            {
                break;
            }
        }
    }

    paint_buffer Result = {.Commands = Tree->PaintBuffer, .Size = CommandCount};
    return Result;
}

} // namespace layout
