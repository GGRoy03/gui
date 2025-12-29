#pragma once

#include <unordered_map>

namespace gui
{

// =============================================================================
// DOMAIN: Basic Geometry
// =============================================================================


// TODO: Handle radiuses
static float
BoundingBoxSignedDistanceField(point Point, bounding_box BoundingBox)
{
    dimensions Size   = dimensions(BoundingBox.Right - BoundingBox.Left, BoundingBox.Bottom - BoundingBox.Top);
    dimensions Half   = dimensions(Size.Width * 0.5f, Size.Height * 0.5f);
    point      Start  = point(BoundingBox.Left, BoundingBox.Top);
    point      Center = Start + Half;
    point      Local  = Point - Center;

    point FirstQuadrant = point(abs(Local.X), abs(Local.Y)) - Half;
    float OuterX        = Max(FirstQuadrant.X, 0.f);
    float OuterY        = Max(FirstQuadrant.Y, 0.f);
    float OuterDistance = sqrtf(OuterX * OuterX + OuterY * OuterY);
    float InnerDistance = Min(Max(FirstQuadrant.X, FirstQuadrant.Y), 0.f);
    float Result        = OuterDistance + InnerDistance;

    return Result;
}


static bool
BoundingBoxesIntersect(bounding_box A, bounding_box B)
{
    bool Result = A.Left < B.Right && A.Right > B.Left && A.Top < B.Bottom && A.Bottom > B.Top;
    return Result;
}

// =============================================================================
// DOMAIN: Layout Tree & Nodes
// =============================================================================

// Obviously remove this from global. (Don't even want to use STD)
std::unordered_map<uint64_t, uint32_t> KeyMap;

enum class NodeState : uint32_t
{
    None = 0,

    UseHoveredStyle    = 1 << 0,
    UseFocusedStyle    = 1 << 1,
    HasCapturedPointer = 1 << 2,
    IsClicked          = 1 << 3,
};


template<>
struct enable_bitmask_operators<NodeState> : std::true_type {};

template<>
struct enable_bitmask_operators<gui::NodeFlags> : std::true_type {};


struct ui_layout_node
{
    uint32_t            Parent;
    uint32_t            First;
    uint32_t            Last;
    uint32_t            Next;
    uint32_t            Prev;
    uint32_t            ChildCount;
    uint32_t            Index;

    point               OutputPosition;
    dimensions          OutputSize;
    dimensions          OutputChildSize;

    size                Size;
    size                MinSize;
    size                MaxSize;
    Alignment           XAlign;
    Alignment           YAlign;
    LayoutDirection     Direction;
    padding             Padding;
    float               Spacing;

    dimensions          VisualOffset;
    dimensions          DragOffset;
    dimensions          ScrollOffset;

    NodeState           State;
    NodeFlags           Flags;
};


struct parent_node
{
    parent_node *Prev;
    uint32_t     Value;
};


struct ui_node_buffer
{
    ui_layout_node *Nodes;
    uint64_t        Count;
    uint64_t        Capacity;
};


struct ui_layout_tree
{
    ui_node_buffer    NodeBuffer;
    paint_properties *PaintBuffer;
    uint64_t          RootIndex;
    uint64_t          CapturedNodeIndex;
    parent_node      *Parent;
};


static bool
IsValidLayoutNode(ui_layout_node *Node)
{
    bool Result = Node && Node->Index != InvalidIndex;
    return Result;
}


static ui_layout_node *
GetSentinelNode(ui_node_buffer NodeBuffer)
{
    ui_layout_node *Result = NodeBuffer.Nodes + NodeBuffer.Capacity;
    return Result;
}


static ui_layout_node *
GetLayoutNode(ui_node_buffer NodeBuffer, uint64_t Index)
{
    ui_layout_node *Result = 0;

    if (Index < NodeBuffer.Capacity)
    {
        Result = NodeBuffer.Nodes + Index;
    }

    return Result;
}


static ui_layout_node *
GetFreeLayoutNode(ui_node_buffer &NodeBuffer)
{
    ui_layout_node *Sentinel = GetSentinelNode(NodeBuffer);
    ui_layout_node *Result   = 0;

    if (Sentinel->Next != InvalidIndex)
    {
        VOID_ASSERT(NodeBuffer.Count < NodeBuffer.Capacity);

        Result = GetLayoutNode(NodeBuffer, Sentinel->Next);
        Result->Index  = Sentinel->Next;
        Sentinel->Next = Result->Next;

        Result->First      = InvalidIndex;
        Result->Last       = InvalidIndex;
        Result->Prev       = InvalidIndex;
        Result->Next       = InvalidIndex;
        Result->Parent     = InvalidIndex;
        Result->ChildCount = 0;

        // if(NodeBuffer.Count == 0)
        // {
            // RootIndex = Result->Index;
        // }

        ++NodeBuffer.Count;
    }

    return Result;
}


static void
AppendLayoutNode(ui_node_buffer NodeBuffer, uint32_t ParentIndex, uint32_t ChildIndex)
{
    ui_layout_node *Parent = GetLayoutNode(NodeBuffer, ParentIndex);
    ui_layout_node *Child  = GetLayoutNode(NodeBuffer, ChildIndex );

    if(IsValidLayoutNode(Parent) && IsValidLayoutNode(Child))
    {
        Child->First  = InvalidIndex;
        Child->Last   = InvalidIndex;
        Child->Prev   = InvalidIndex;
        Child->Next   = InvalidIndex;
        Child->Parent = Parent->Index;

        ui_layout_node *First = GetLayoutNode(NodeBuffer, Parent->First);
        if(!IsValidLayoutNode(First))
        {
            Parent->First = Child->Index;
        }

        ui_layout_node *Last  = GetLayoutNode(NodeBuffer, Parent->Last);
        if(IsValidLayoutNode(Last))
        {
            Last->Next  = Child->Index;
        }

        Child->Prev         = Parent->Last;
        Parent->Last        = Child->Index;
        Parent->ChildCount += 1;
    }
}


static bool
IsValidLayoutTree(const ui_layout_tree *Tree)
{
    bool Result = (Tree && Tree->NodeBuffer.Nodes && Tree->NodeBuffer.Count <= Tree->NodeBuffer.Capacity);
    return Result;
}


static memory_footprint
GetLayoutTreeFootprint(uint64_t NodeCount)
{
    // ui_layout_tree
    const uint64_t TreeEnd = sizeof(ui_layout_tree);

    // ui_layout_node[NodeCount+1]
    const uint64_t NodesStart = AlignPow2(TreeEnd, AlignOf(ui_layout_node));
    const uint64_t NodesEnd   = NodesStart + ((NodeCount + 1) * sizeof(ui_layout_node));

    // paint_properties[NodeCount]
    const uint64_t PaintStart = AlignPow2(NodesEnd, AlignOf(paint_properties));
    const uint64_t PaintEnd   = PaintStart + (NodeCount * sizeof(paint_properties));

    memory_footprint Result =
    {
        .SizeInBytes = PaintEnd,
        .Alignment   = AlignOf(ui_layout_tree),
    };

    return Result;
}


static ui_layout_tree *
PlaceLayoutTreeInMemory(uint64_t NodeCount, memory_block Block)
{
    ui_layout_tree *Result = 0;
    memory_region   Local  = EnterMemoryRegion(Block);

    if (IsValidMemoryRegion(Local))
    {
        // IMPORTANT:
        // THE ORDER IN WHICH WE PLACE MEMBERS IS IMPORTANT. (SEE FOOTPRINT)

        auto *Tree  = PushStruct<ui_layout_tree>(Local);
        auto *Nodes = PushArray<ui_layout_node>(Local, NodeCount + 1);
        auto *Paint = PushArray<paint_properties>(Local, NodeCount);

        if (Tree && Nodes && Paint)
        {
            Tree->NodeBuffer.Nodes    = Nodes;
            Tree->NodeBuffer.Count    = 0;
            Tree->NodeBuffer.Capacity = NodeCount;
            Tree->PaintBuffer         = Paint;
            
            for (uint64_t Idx = 0; Idx < Tree->NodeBuffer.Capacity; Idx++)
            {
                ui_layout_node *Node = GetLayoutNode(Tree->NodeBuffer, Idx);
                VOID_ASSERT(Node);
            
                Node->Index      = InvalidIndex;
                Node->First      = InvalidIndex;
                Node->Last       = InvalidIndex;
                Node->Parent     = InvalidIndex;
                Node->Prev       = InvalidIndex;
                Node->ChildCount = 0;
            
                Node->Next = Idx + 1;
            }
            
            ui_layout_node *Sentinel = GetSentinelNode(Tree->NodeBuffer);
            Sentinel->Next  = 0;
            Sentinel->Index = InvalidIndex;

            Result = Tree;
        }
    }

    return Result;
}

static void
UpdateInput(uint32_t NodeIndex, cached_style *Cached, ui_layout_tree *Tree)
{
    if(IsValidLayoutTree(Tree) && Cached)
    {
        ui_layout_node *Node = GetLayoutNode(Tree->NodeBuffer, NodeIndex);
        if(IsValidLayoutNode(Node))
        {
            // Write the layout inputs
            {
                Node->Size      = Cached->Layout.Size.Value;
                Node->MinSize   = Cached->Layout.MinSize.Value;
                Node->MaxSize   = Cached->Layout.MaxSize.Value;
                Node->Direction = Cached->Layout.Direction.Value;
                Node->XAlign    = Cached->Layout.XAlign.Value;
                Node->YAlign    = Cached->Layout.YAlign.Value;
                Node->Padding   = Cached->Layout.Padding.Value;
                Node->Spacing   = Cached->Layout.Spacing.Value;
        
                if(!Cached->Layout.MinSize.IsSet)
                {
                    Node->MinSize = Node->Size;
                }
        
                if(!Cached->Layout.MaxSize.IsSet)
                {
                
                    Node->MaxSize = Node->Size;
                }
            }

            // Write the paint properties
            {
                paint_properties &Paint = Tree->PaintBuffer[NodeIndex];
            
                Paint.Color               = Cached->Default.Color.Value;
                Paint.BorderColor         = Cached->Default.BorderColor.Value;
                Paint.BorderWidth         = Cached->Default.BorderWidth.Value;
                Paint.TextColor           = Cached->Default.TextColor.Value;
                Paint.CornerRadius        = Cached->Default.CornerRadius.Value;
                Paint.Softness            = Cached->Default.Softness.Value;
            
                Paint.HoveredColor        = Cached->Hovered.Color.IsSet        ? Cached->Hovered.Color.Value        : Paint.Color;
                Paint.HoveredBorderColor  = Cached->Hovered.BorderColor.IsSet  ? Cached->Hovered.BorderColor.Value  : Paint.BorderColor;
                Paint.HoveredBorderWidth  = Cached->Hovered.BorderWidth.IsSet  ? Cached->Hovered.BorderWidth.Value  : Paint.BorderWidth;
                Paint.HoveredTextColor    = Cached->Hovered.TextColor.IsSet    ? Cached->Hovered.TextColor.Value    : Paint.TextColor;
                Paint.HoveredCornerRadius = Cached->Hovered.CornerRadius.IsSet ? Cached->Hovered.CornerRadius.Value : Paint.CornerRadius;
                Paint.HoveredSoftness     = Cached->Hovered.Softness.IsSet     ? Cached->Hovered.Softness.Value     : Paint.Softness;
            
                Paint.FocusedColor        = Cached->Focused.Color.IsSet        ? Cached->Focused.Color.Value        : Paint.Color;
                Paint.FocusedBorderColor  = Cached->Focused.BorderColor.IsSet  ? Cached->Focused.BorderColor.Value  : Paint.BorderColor;
                Paint.FocusedBorderWidth  = Cached->Focused.BorderWidth.IsSet  ? Cached->Focused.BorderWidth.Value  : Paint.BorderWidth;
                Paint.FocusedTextColor    = Cached->Focused.TextColor.IsSet    ? Cached->Focused.TextColor.Value    : Paint.TextColor;
                Paint.FocusedCornerRadius = Cached->Focused.CornerRadius.IsSet ? Cached->Focused.CornerRadius.Value : Paint.CornerRadius;
                Paint.FocusedSoftness     = Cached->Focused.Softness.IsSet     ? Cached->Focused.Softness.Value     : Paint.Softness;
            
                Paint.CaretColor = Cached->Focused.CaretColor.Value;
                Paint.CaretWidth = Cached->Focused.CaretWidth.Value;
            }
        }
    }
}


static bounding_box
GetLayoutNodeBoundingBox(ui_layout_node *Node)
{
    bounding_box Result = {};

    if(IsValidLayoutNode(Node))
    {
        point Screen = Node->OutputPosition + Node->ScrollOffset;
        Result = bounding_box(Screen, Node->OutputSize);
    }

    return Result;
}

// TODO: FIX THIS
static bounding_box
GetLayoutNodeInnerBoundingBox(ui_layout_node *Node)
{
    bounding_box Result = {};

    if(IsValidLayoutNode(Node))
    {
        point Screen = Node->OutputPosition + Node->ScrollOffset;
        Result = bounding_box(Screen, Node->OutputSize);
    }

    return Result;
}


// TODO: FIX THIS
static bounding_box
GetLayoutNodeContentRect(ui_layout_node *Node)
{
    bounding_box Result = {};

    if(IsValidLayoutNode(Node))
    {
        point Screen = Node->OutputPosition + Node->ScrollOffset;
        Result = bounding_box(Screen, Node->OutputSize);
    }

    return Result;
}


static uint32_t
CreateNode(uint64_t Key, NodeFlags Flags, cached_style *Style, ui_layout_tree *Tree)
{
    uint32_t Result = InvalidIndex;

    if(IsValidLayoutTree(Tree))
    {
        auto         Iter = KeyMap.find(Key);
        ui_layout_node *Node = nullptr;
    
        if (Iter != KeyMap.end())
        {
            Node = GetLayoutNode(Tree->NodeBuffer, Iter->second);
        }
        else
        {
            Node = GetFreeLayoutNode(Tree->NodeBuffer);
            KeyMap.emplace(Key, Node->Index);
        }
    
        if(IsValidLayoutNode(Node))
        {
            Node->ChildCount = 0;
            Node->Flags      = Flags;

            uint32_t ParentIndex = Tree->Parent ? Tree->Parent->Value : InvalidIndex;
            AppendLayoutNode(Tree->NodeBuffer, ParentIndex, Node->Index);

            Result = Node->Index;
        }
    }

    return Result;
}


static void
PushParent(uint32_t NodeIndex, ui_layout_tree *Tree, parent_node *ParentNode)
{
    if(IsValidLayoutTree(Tree) && ParentNode)
    {
        ParentNode->Prev  = Tree->Parent;
        ParentNode->Value = NodeIndex;

        Tree->Parent = ParentNode;
    }
}


static void
PopParent(uint32_t NodeIndex, ui_layout_tree *Tree)
{
    if(IsValidLayoutTree(Tree) && Tree->Parent)
    {
        Tree->Parent = Tree->Parent->Prev;
    }
}


static uint32_t
FindChild(uint32_t NodeIndex, uint32_t FindIndex, ui_layout_tree *Tree)
{
    uint32_t Result = InvalidIndex;

    if(IsValidLayoutTree(Tree))
    {
        ui_layout_node *Node = GetLayoutNode(Tree->NodeBuffer, NodeIndex);
        if(IsValidLayoutNode(Node))
        {
            ui_layout_node *Child = GetLayoutNode(Tree->NodeBuffer, Node->First);
            while(IsValidLayoutNode(Child) && FindIndex--)
            {
                Child = GetLayoutNode(Tree->NodeBuffer, Child->Next);
            }

            Result = Child->Index;
        }
    }

    return Result;
}


static bool
AppendChild(uint32_t ParentIndex, uint32_t ChildIndex, ui_layout_tree *Tree)
{
    bool Result = false;

    if(IsValidLayoutTree(Tree) && ParentIndex != ChildIndex)
    {
        AppendLayoutNode(Tree->NodeBuffer, ParentIndex, ChildIndex);
    }

    return Result;
}


static void
SetNodeOffset(uint32_t NodeIndex, dimensions Offset, ui_layout_tree *Tree)
{
    if(IsValidLayoutTree(Tree))
    {
        ui_layout_node *Node = GetLayoutNode(Tree->NodeBuffer, NodeIndex);
        if(IsValidLayoutNode(Node))
        {
            Node->VisualOffset = Offset;
        }
    }
}

// =============================================================================
// DOMAIN: Scroll Region
// =============================================================================

struct scroll_region
{
    dimensions ContentSize;
    float      ScrollOffset;
    float      PixelPerLine;
    AxisType   Axis;
};

// --- Private Helpers ---

static void UpdateScrollNode(float ScrolledLines, ui_layout_node *Node, ui_layout_tree *Tree, scroll_region *Region)
{
    float      ScrolledPixels = ScrolledLines * Region->PixelPerLine;
    dimensions WindowSize     = Node->OutputSize;

    float ScrollLimit = 0.f;
    if (Region->Axis == AxisType::X)
    {
        ScrollLimit = -(Region->ContentSize.Width  - WindowSize.Width);
    } else
    if (Region->Axis == AxisType::Y)
    {
        ScrollLimit = -(Region->ContentSize.Height - WindowSize.Height);
    }

    Region->ScrollOffset += ScrolledPixels;
    Region->ScrollOffset  = ClampTop(ClampBot(ScrollLimit, Region->ScrollOffset), 0);

    dimensions ScrollDelta = dimensions(0.f, 0.f);
    if(Region->Axis == AxisType::X)
    {
        ScrollDelta.Width  = -1.f * Region->ScrollOffset;
    } else
    if(Region->Axis == AxisType::Y)
    {
        ScrollDelta.Height = -1.f * Region->ScrollOffset;
    }

    bounding_box WindowContent = GetLayoutNodeBoundingBox(Node);
    for(ui_layout_node *Child = GetLayoutNode(Tree->NodeBuffer, Node->First); IsValidLayoutNode(Child); Child = GetLayoutNode(Tree->NodeBuffer, Child->Next))
    {
        Child->ScrollOffset = dimensions(-ScrollDelta.Width, -ScrollDelta.Height);

        dimensions   FixedContentSize = Child->OutputSize;
        bounding_box ChildContent     = GetLayoutNodeBoundingBox(Child);

        if (FixedContentSize.Width > 0.0f && FixedContentSize.Height > 0.0f) 
        {
            if (BoundingBoxesIntersect(WindowContent, ChildContent))
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

static uint64_t
GetScrollRegionFootprint(void)
{
    uint64_t Result = sizeof(scroll_region);
    return Result;
}

static scroll_region *
PlaceScrollRegionInMemory(scroll_region_params Params, void *Memory)
{
    scroll_region *Result = 0;

    if(Memory)
    {
        Result = (scroll_region *)Memory;
        Result->ContentSize  = dimensions(0.f, 0.f);
        Result->ScrollOffset = 0.f;
        Result->PixelPerLine = Params.PixelPerLine;
        Result->Axis         = Params.Axis;
    }

    return Result;
}

static dimensions
GetScrollNodeTranslation(scroll_region *Region)
{
    dimensions Result = {};

    if (Region->Axis == AxisType::X)
    {
        Result = dimensions(Region->ScrollOffset, 0.f);
    } else
    if (Region->Axis == AxisType::Y)
    {
        Result = dimensions(0.f, Region->ScrollOffset);
    }

    return Result;
}

// =============================================================================
// DOMAIN: Input Handling
// =============================================================================


static bool
IsMouseInsideOuterBox(point Position, ui_layout_node *Node)
{
    bool Result = false;

    if(IsValidLayoutNode(Node))
    {
        auto  BoundingBox = GetLayoutNodeBoundingBox(Node);
        float Distance    = BoundingBoxSignedDistanceField(Position, BoundingBox);

        Result = Distance <= 0.f;
    }

    return Result;
}


// TODO: Fix the border stuff
static bool
IsPointInsideBorder(point Position, ui_layout_node *Node)
{
    bool Result = false;

    if(IsValidLayoutNode(Node))
    {
        auto  BoundingBox = GetLayoutNodeBoundingBox(Node);
        float Distance    = BoundingBoxSignedDistanceField(Position, BoundingBox);

        Result = Distance >= 0.f;
    }

    return Result;
}


static bool
HandlePointerClick(point Position, PointerButton ClickMask, uint32_t NodeIndex, ui_layout_tree *Tree)
{
    VOID_ASSERT(Position.X >= 0.f && Position.Y >= 0.f);
    VOID_ASSERT(IsValidLayoutTree(Tree));

    if(IsValidLayoutTree(Tree))
    {
        ui_layout_node *Node = GetLayoutNode(Tree->NodeBuffer, NodeIndex);
        VOID_ASSERT(IsValidLayoutNode(Node));

        if(IsValidLayoutNode(Node))
        {
            if(IsMouseInsideOuterBox(Position, Node))
            {
                for(ui_layout_node *Child = GetLayoutNode(Tree->NodeBuffer, Node->First); IsValidLayoutNode(Child); Child = GetLayoutNode(Tree->NodeBuffer, Child->Next))
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

static bool
HandlePointerRelease(point Position, PointerButton ReleaseMask, uint32_t NodeIndex, ui_layout_tree *Tree)
{
    VOID_ASSERT(Position.X >= 0.f && Position.Y >= 0.f);
    VOID_ASSERT(IsValidLayoutTree(Tree));

    if(IsValidLayoutTree(Tree))
    {
        ui_layout_node *Node = GetLayoutNode(Tree->NodeBuffer, NodeIndex);
        VOID_ASSERT(IsValidLayoutNode(Node));

        for(ui_layout_node *Child = GetLayoutNode(Tree->NodeBuffer, Node->First); IsValidLayoutNode(Child); Child = GetLayoutNode(Tree->NodeBuffer, Child->Next))
        {
            if(HandlePointerRelease(Position, ReleaseMask, Child->Index, Tree))
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


static bool
HandlePointerHover(point Position, uint32_t NodeIndex, ui_layout_tree *Tree)
{
    VOID_ASSERT(Position.X >= 0.f && Position.Y >= 0.f);

    if(IsValidLayoutTree(Tree))
    {
        ui_layout_node *Node = GetLayoutNode(Tree->NodeBuffer, NodeIndex);
        VOID_ASSERT(IsValidLayoutNode(Node));

        if(IsMouseInsideOuterBox(Position, Node))
        {
            for(ui_layout_node *Child = GetLayoutNode(Tree->NodeBuffer, Node->First); IsValidLayoutNode(Child); Child = GetLayoutNode(Tree->NodeBuffer, Child->Next))
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


static void
HandlePointerMove(float DeltaX, float DeltaY, ui_layout_tree *Tree)
{
    VOID_ASSERT(IsValidLayoutTree(Tree));
    
    if(IsValidLayoutTree(Tree))
    {
        ui_layout_node *CapturedNode = GetLayoutNode(Tree->NodeBuffer, Tree->CapturedNodeIndex);
        if(IsValidLayoutNode(CapturedNode))
        {
            if((CapturedNode->Flags & NodeFlags::IsDraggable) != NodeFlags::None)
            {
                CapturedNode->OutputPosition.X += DeltaX;
                CapturedNode->OutputPosition.Y += DeltaY;
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

static uint32_t WrapText(text *Text, ui_layout_node *Node)
{
    VOID_ASSERT(Text);
    VOID_ASSERT(IsValidLayoutNode(Node));

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

static void
ComputeLayout(ui_layout_node *Node, ui_layout_tree *Tree, dimensions ParentBounds, resource_table *ResourceTable, bool &Changed)
{
    VOID_ASSERT(IsValidLayoutNode(Node));
    VOID_ASSERT(IsValidLayoutTree(Tree));

    Node->OutputChildSize  = {};
    dimensions LastSize = Node->OutputSize;

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

    dimensions ContentBounds =
    {
        .Width  = Node->OutputSize.Width  - (Node->Padding.Left + Node->Padding.Right),
        .Height = Node->OutputSize.Height - (Node->Padding.Top  + Node->Padding.Bot  ),
    };

    if(Node->ChildCount > 0)
    {
        float Spacing = Node->Spacing * (Node->ChildCount - 1);

        if(Node->Direction == LayoutDirection::Horizontal)
        {
            ContentBounds.Width  -= Spacing;
        } else
        if(Node->Direction == LayoutDirection::Vertical)
        {
            ContentBounds.Height -= Spacing;
        }
    }

    VOID_ASSERT(ContentBounds.Width >= 0 && ContentBounds.Height >= 0);

    for(ui_layout_node *Child = GetLayoutNode(Tree->NodeBuffer, Node->First); IsValidLayoutNode(Child); Child = GetLayoutNode(Tree->NodeBuffer, Child->Next))
    {
        ComputeLayout(Child, Tree, ContentBounds, ResourceTable, Changed);

        Node->OutputChildSize.Width  += Child->OutputSize.Width;
        Node->OutputChildSize.Height += Child->OutputSize.Height;
    }

    text *Text = static_cast<text *>(QueryNodeResource(ResourceType::Text, Node->Index, Tree, ResourceTable));
    if(Text)
    {
        WrapText(Text, Node);
    }
}

static void PlaceLayout(ui_layout_node *Node, ui_layout_tree *Tree, resource_table *ResourceTable)
{
    point Cursor =
    {
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

    for(ui_layout_node *Child = GetLayoutNode(Tree->NodeBuffer, Node->First); IsValidLayoutNode(Child); Child = GetLayoutNode(Tree->NodeBuffer, Child->Next))
    {
        Child->OutputPosition = Cursor;

        float MinorOffset = GetAlignmentOffset(IsXMajor ? Node->XAlign : Node->YAlign, IsXMajor ? (MinorSize - Child->OutputSize.Height) : (MinorSize - Child->OutputSize.Width));

        if(IsXMajor)
        {
            Child->OutputPosition.Y += MinorOffset + Child->VisualOffset.Height;
            Cursor.X                += Child->OutputSize.Width  + Node->Spacing;
        }
        else
        {
            Child->OutputPosition.X += MinorOffset + Child->VisualOffset.Width;
            Cursor.Y                += Child->OutputSize.Height + Node->Spacing;
        }

        PlaceLayout(Child, Tree, ResourceTable);
    }

    auto *Text = static_cast<text *>(QueryNodeResource(ResourceType::Text, Node->Index, Tree, ResourceTable));
    if(Text)
    {
        VOID_ASSERT(Node->ChildCount == 0);

        point TextCursor   = point(Node->OutputPosition.X + Node->Padding.Left, Node->OutputPosition.Y + Node->Padding.Top);
        float CursorStartX = TextCursor.X;

        for(uint32_t Idx = 0; Idx < Text->ShapedCount; ++Idx)
        {
            shaped_glyph &Glyph = Text->Shaped[Idx];

            if (!Glyph.Skip)
            {
                point      CursorWithOffset = point(TextCursor.X + Glyph.OffsetX, TextCursor.Y + Glyph.OffsetY);
                dimensions GlyphSize        = dimensions(Glyph.Source.Right - Glyph.Source.Left, Glyph.Source.Bottom - Glyph.Source.Top);

                Glyph.Position = bounding_box(CursorWithOffset, GlyphSize);

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

static void ComputeTreeLayout(ui_layout_tree *Tree)
{
    if(IsValidLayoutTree(Tree))
    {
        void_context   &Context    = GetVoidContext();
        ui_layout_node *ActiveRoot = GetLayoutNode(Tree->NodeBuffer, Tree->RootIndex);
    
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

} // namespace gui
