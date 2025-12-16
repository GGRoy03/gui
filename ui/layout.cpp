// -----------------------------------------------
// Tree/Node internal implementation (Helpers/Types)

enum class LayoutNodeFlag
{
    None                = 0,

    UseHoveredStyle    = 1 << 0,
    UseFocusedStyle    = 1 << 1,

    HasCapturedPointer = 1 << 2,
};

inline LayoutNodeFlag operator|(LayoutNodeFlag A, LayoutNodeFlag B)   {return static_cast<LayoutNodeFlag>(static_cast<int>(A) | static_cast<int>(B));}
inline LayoutNodeFlag operator&(LayoutNodeFlag A, LayoutNodeFlag B)   {return static_cast<LayoutNodeFlag>(static_cast<int>(A) & static_cast<int>(B));}
inline LayoutNodeFlag operator|=(LayoutNodeFlag& A, LayoutNodeFlag B) {return A = A | B;}
inline LayoutNodeFlag operator&=(LayoutNodeFlag& A, LayoutNodeFlag B) {return A = A & B;}
inline LayoutNodeFlag operator~(LayoutNodeFlag A)                     {return static_cast<LayoutNodeFlag>(~static_cast<int>(A));}

struct layout_bounds
{
    float X;
    float Y;
    float Width;
    float Height;

    constexpr bool operator!=(const layout_bounds &Other) noexcept
    {
        bool Result = (this->X     != Other.X     || this->Y      != Other.Y     ) ||
                      (this->Width != Other.Width || this->Height != Other.Height);
        return Result;
    }
};

struct ui_layout_node
{
    // Hierarchy
    ui_layout_node *Parent;
    ui_layout_node *First;
    ui_layout_node *Last;
    ui_layout_node *Next;
    ui_layout_node *Prev;

    // Outputs
    layout_bounds Bounds;

    // Inputs
    vec2_float Size;
    vec2_float MinSize;
    vec2_float MaxSize;

    // State
    LayoutNodeFlag Flags;
    vec2_float     ScrollOffset;
    vec2_float     DragOffset;

    // Extra Info
    uint32_t Index;
    uint32_t StyleIndex;
    uint32_t ChildCount;
    uint32_t LegacyFlags;
};

struct ui_parent_node
{
    ui_parent_node *Prev;
    uint32_t        Index;
};

struct ui_parent_list
{
    ui_parent_node *First;
    ui_parent_node *Last;
    uint32_t        Count;
};

struct ui_layout_tree
{
    uint64_t          NodeCapacity;
    uint64_t          NodeCount;
    ui_layout_node   *Nodes;

    // State
    ui_parent_list    ParentList;
    uint32_t          CapturedNodeIndex;

    ui_paint_command *PaintBuffer;
};

static bool
IsValidLayoutNode(ui_layout_node *Node)
{
    bool Result = Node && Node->Index != InvalidLayoutNodeIndex;
    return Result;
}

static bool
IsValidLayoutTree(ui_layout_tree *Tree)
{
    bool Result = (Tree) && (Tree->Nodes) && (Tree->NodeCount <= Tree->NodeCapacity);
    return Result;
}

static ui_layout_node *
GetLayoutNode(uint32_t Index, ui_layout_tree *Tree)
{
    ui_layout_node *Result = 0;

    if(Index < Tree->NodeCount)
    {
        Result = Tree->Nodes + Index;
    }

    return Result;
}

static ui_layout_node *
GetLayoutRoot(ui_layout_tree *Tree)
{
    ui_layout_node *Result = Tree->Nodes;
    return Result;
}

static ui_layout_node *
GetFreeLayoutNode(ui_layout_tree *Tree)
{
    VOID_ASSERT(Tree);

    ui_layout_node *Result = 0;

    if (Tree->NodeCount < Tree->NodeCapacity)
    {
        Result = Tree->Nodes + Tree->NodeCount;
        Result->Index = Tree->NodeCount;

        ++Tree->NodeCount;
    }

    return Result;
}

// ==================================================================================
// @Public : Tree/Node Public API.

static rect_float
GetNodeOuterRect(ui_layout_node *Node)
{
    vec2_float Screen = vec2_float(Node->Bounds.X, Node->Bounds.Y) + Node->ScrollOffset;
    rect_float Result = rect_float::FromXYWH(Screen.X, Screen.Y, Node->Bounds.Width, Node->Bounds.Height);

    return Result;
}

static rect_float
GetNodeInnerRect(ui_layout_node *Node)
{
    vec2_float Screen = vec2_float(Node->Bounds.X, Node->Bounds.Y) + Node->ScrollOffset;
    rect_float Result = rect_float::FromXYWH(Screen.X, Screen.Y, Node->Bounds.Width, Node->Bounds.Height);

    return Result;
}

static rect_float
GetNodeContentRect(ui_layout_node *Node)
{
    vec2_float Screen = vec2_float(Node->Bounds.X, Node->Bounds.Y) + Node->ScrollOffset;
    rect_float Result = rect_float::FromXYWH(Screen.X, Screen.Y, Node->Bounds.Width, Node->Bounds.Height);

    return Result;
}

static void
SetNodeProperties(uint32_t NodeIndex, uint32_t StyleIndex, const ui_cached_style &Cached, ui_layout_tree *Tree)
{
    ui_layout_node *Node = GetLayoutNode(NodeIndex, Tree);
    if(Node)
    {
        Node->StyleIndex  = StyleIndex;

        // Trying something simpler.
        Node->Size    = vec2_float(Cached.Default.Size.Value.Width   , Cached.Default.Size.Value.Width);
        Node->MinSize = vec2_float(Cached.Default.MinSize.Value.Width, Cached.Default.MinSize.Value.Height);
        Node->MaxSize = vec2_float(Cached.Default.MaxSize.Value.Width, Cached.Default.MaxSize.Value.Height);
    }
}

static uint64_t
GetLayoutTreeAlignment(void)
{
    uint64_t Result = AlignOf(ui_layout_node);
    return Result;
}

static uint64_t
GetLayoutTreeFootprint(uint64_t NodeCount)
{
    uint64_t ArraySize = NodeCount * sizeof(ui_layout_node);
    uint64_t PaintSize = NodeCount * sizeof(ui_paint_command);
    uint64_t Result    = sizeof(ui_layout_tree) + ArraySize + PaintSize;

    return Result;
}

static ui_layout_tree *
PlaceLayoutTreeInMemory(uint64_t NodeCount, void *Memory)
{
    ui_layout_tree *Result = 0;

    if (Memory)
    {
        ui_layout_node   *Nodes      = static_cast<ui_layout_node*>(Memory);
        ui_paint_command *PaintBuffer = reinterpret_cast<ui_paint_command *>(Nodes + NodeCount);

        Result = reinterpret_cast<ui_layout_tree *>(PaintBuffer + NodeCount);
        Result->Nodes        = Nodes;
        Result->PaintBuffer  = PaintBuffer;
        Result->NodeCount    = 0;
        Result->NodeCapacity = NodeCount;

        for (uint64_t Idx = 0; Idx < Result->NodeCapacity; Idx++)
        {
            Result->Nodes[Idx].Index = InvalidLayoutNodeIndex;
        }
    }

    return Result;
}

static uint32_t
AllocateLayoutNode(uint32_t Flags, ui_layout_tree *Tree)
{
    VOID_ASSERT(Tree); // Internal Corruption

    uint32_t Result = InvalidLayoutNodeIndex;

    ui_layout_node *Node = GetFreeLayoutNode(Tree);
    if(Node)
    {
        Node->LegacyFlags = Flags;
        Node->Last        = 0;
        Node->Next        = 0;
        Node->First       = 0;

        if(Tree->ParentList.Last)
        {
            Node->Parent = GetLayoutNode(Tree->ParentList.Last->Index, Tree);

            AppendToDoublyLinkedList(Node->Parent, Node, Node->Parent->ChildCount);
        }

        Result = Node->Index;
    }

    return Result;
}

// -----------------------------------------------------------------------------------
// @Internal: Tree Queries

static uint32_t
UITreeFindChild(uint32_t NodeIndex, uint32_t FindIndex, ui_layout_tree *Tree)
{
    VOID_ASSERT(Tree); // Internal Corruption

    uint32_t Result = InvalidLayoutNodeIndex;

    ui_layout_node *LayoutNode = GetLayoutNode(NodeIndex, Tree);
    if(IsValidLayoutNode(LayoutNode))
    {
        ui_layout_node *Child = LayoutNode->First;
        while(Child && FindIndex--)
        {
            Child = Child->Next;
        }

        if(Child)
        {
            Result = Child->Index;
        }
    }

    return Result;
}

static void
UITreeAppendChild(uint32_t ParentIndex, uint32_t ChildIndex, ui_layout_tree *Tree)
{
    VOID_ASSERT(Tree);                      // Internal Corruption
    VOID_ASSERT(ParentIndex != ChildIndex); // Trying to append the node to itself

    ui_layout_node *Parent = GetLayoutNode(ParentIndex, Tree);
    ui_layout_node *Child  = GetLayoutNode(ChildIndex , Tree);

    if(IsValidLayoutNode(Parent) && IsValidLayoutNode(Child) && ParentIndex != ChildIndex)
    {
        AppendToDoublyLinkedList(Parent, Child, Parent->ChildCount);
        Child->Parent = Parent;
    }
    else
    {
        LogError("Invalid Indices Provided | Parent = %u, Child = %u", ParentIndex, ChildIndex);
    }
}

static void
UITreeReserve(uint32_t NodeIndex, uint32_t Amount, ui_layout_tree *Tree)
{
    VOID_ASSERT(Tree);   // Internal Corruption
    VOID_ASSERT(Amount); // Calling this with 0 is useless

    ui_layout_node *Parent = GetLayoutNode(NodeIndex, Tree);
    if(IsValidLayoutNode(Parent))
    {
        for(uint32_t Idx = 0; Idx < Amount; ++Idx)
        {
            ui_layout_node *Reserved = GetFreeLayoutNode(Tree);
            if(IsValidLayoutNode(Reserved))
            {
                AppendToDoublyLinkedList(Parent, Reserved, Parent->ChildCount);
            }
        }
    }

}


static bool
PushLayoutParent(uint32_t Index, ui_layout_tree *Tree, memory_arena *Arena)
{
    ui_parent_node *Node = PushStruct(Arena, ui_parent_node);
    if(Node)
    {
        ui_parent_list &List = Tree->ParentList;

        if(!List.First)
        {
            List.First = Node;
        }

        Node->Index = Index;
        Node->Prev  = List.Last;

        List.Last   = Node;
        List.Count += 1;
    }

    bool Result = Node != 0;
    return Result;
}


static bool
PopLayoutParent(uint32_t Index, ui_layout_tree *Tree)
{
    bool Result = false;

    if(Tree->ParentList.Last && Index == Tree->ParentList.Last->Index)
    {
        ui_parent_list &List = Tree->ParentList;
        ui_parent_node *Node = List.Last;

        if(!Node->Prev)
        {
            List.First = 0;
        }

        List.Last   = Node->Prev;
        List.Count -= 1;
    }

    return Result;
}


// -----------------------------------------------------------
// UI Scrolling internal Implementation

typedef struct ui_scroll_region
{
    vec2_float  ContentSize;
    float       ScrollOffset;
    float       PixelPerLine;
    UIAxis_Type Axis;
} ui_scroll_region;

static uint64_t
GetScrollRegionFootprint(void)
{
    uint64_t Result = sizeof(ui_scroll_region);
    return Result;
}

static ui_scroll_region *
PlaceScrollRegionInMemory(scroll_region_params Params, void *Memory)
{
    ui_scroll_region *Result = 0;

    if(Memory)
    {
        Result = (ui_scroll_region *)Memory;
        Result->ContentSize  = vec2_float(0.f, 0.f);
        Result->ScrollOffset = 0.f;
        Result->PixelPerLine = Params.PixelPerLine;
        Result->Axis         = Params.Axis;
    }

    return Result;
}

static vec2_float
GetScrollNodeTranslation(ui_scroll_region *Region)
{
    vec2_float Result = vec2_float(0.f, 0.f);

    if (Region->Axis == UIAxis_X)
    {
        Result = vec2_float(Region->ScrollOffset, 0.f);
    } else
    if (Region->Axis == UIAxis_Y)
    {
        Result = vec2_float(0.f, Region->ScrollOffset);
    }

    return Result;
}

static void
UpdateScrollNode(float ScrolledLines, ui_layout_node *Node, ui_layout_tree *Tree, ui_scroll_region *Region)
{
    float      ScrolledPixels = ScrolledLines * Region->PixelPerLine;
    vec2_float WindowSize     = vec2_float(Node->Bounds.Width, Node->Bounds.Height);

    float ScrollLimit = 0.f;
    if (Region->Axis == UIAxis_X)
    {
        ScrollLimit = -(Region->ContentSize.X - WindowSize.X);
    } else
    if (Region->Axis == UIAxis_Y)
    {
        ScrollLimit = -(Region->ContentSize.Y - WindowSize.Y);
    }

    Region->ScrollOffset += ScrolledPixels;
    Region->ScrollOffset  = ClampTop(ClampBot(ScrollLimit, Region->ScrollOffset), 0);

    vec2_float ScrollDelta = vec2_float(0.f, 0.f);
    if(Region->Axis == UIAxis_X)
    {
        ScrollDelta.X = -1.f * Region->ScrollOffset;
    } else
    if(Region->Axis == UIAxis_Y)
    {
        ScrollDelta.Y = -1.f * Region->ScrollOffset;
    }

    rect_float WindowContent = GetNodeOuterRect(Node);
    IterateLinkedList(Node, ui_layout_node *, Child)
    {
        Child->ScrollOffset = vec2_float(-ScrollDelta.X, -ScrollDelta.Y);

        vec2_float FixedContentSize = vec2_float(Child->Bounds.Width, Child->Bounds.Height);
        rect_float ChildContent     = GetNodeOuterRect(Child);

        if (FixedContentSize.X > 0.0f && FixedContentSize.Y > 0.0f) 
        {
            if (WindowContent.IsIntersecting(ChildContent))
            {
                Child->LegacyFlags &= ~UILayoutNode_DoNotPaint;
            }
            else
            {
                Child->LegacyFlags |= UILayoutNode_DoNotPaint;
            }
        }
    }
}

static bool
IsMouseInsideOuterBox(vec2_float MousePosition, ui_layout_node *Node)
{
    VOID_ASSERT(Node); // Internal Corruption

    vec2_float OuterSize  = vec2_float(Node->Bounds.Width, Node->Bounds.Height);
    vec2_float OuterPos   = vec2_float(Node->Bounds.X    , Node->Bounds.Y     ) + Node->ScrollOffset;
    vec2_float OuterHalf  = vec2_float(OuterSize.X * 0.5f, OuterSize.Y * 0.5f);
    vec2_float Center     = OuterPos + OuterHalf;
    vec2_float LocalMouse = MousePosition - Center;

    rect_sdf_params Params = {0};
    Params.HalfSize      = OuterHalf;
    Params.PointPosition = LocalMouse;

    float Distance = RoundedRectSDF(Params);
    return Distance <= 0.f;
}

// BUG:
// No access to border width.

static bool
IsMouseInsideBorder(vec2_float MousePosition, ui_layout_node *Node)
{
    vec2_float InnerSize   = vec2_float(Node->Bounds.Width, Node->Bounds.Height) - vec2_float(0.f, 0.f);
    vec2_float InnerPos    = vec2_float(Node->Bounds.X    , Node->Bounds.Y     ) - vec2_float(0.f, 0.f) + Node->ScrollOffset;
    vec2_float InnerHalf   = vec2_float(InnerSize.X * 0.5f, InnerSize.Y * 0.5f);
    vec2_float InnerCenter = InnerPos + InnerHalf;

    rect_sdf_params Params = {0};
    Params.Radius        = 0.f;
    Params.HalfSize      = InnerHalf;
    Params.PointPosition = MousePosition - InnerCenter;

    float Distance = RoundedRectSDF(Params);
    return Distance >= 0.f;
}

static bool
HandlePointerClick(vec2_float Position, uint32_t ClickMask, uint32_t NodeIndex, ui_layout_tree *Tree)
{
    ui_layout_node *Node = GetLayoutNode(NodeIndex, Tree);

    if(Node)
    {
        if(IsMouseInsideOuterBox(Position, Node))
        {
            IterateLinkedList(Node, ui_layout_node *, Child)
            {
                bool IsHandled = HandlePointerClick(Position, ClickMask, Child->Index, Tree);

                if(IsHandled)
                {
                    return true;
                }
            }

            Node->Flags |= LayoutNodeFlag::UseFocusedStyle;
            Node->Flags |= LayoutNodeFlag::HasCapturedPointer;

            Tree->CapturedNodeIndex = NodeIndex;

            return true;
        }
    }

    return false;
}


static bool
HandlePointerRelease(vec2_float Position, uint32_t ButtonMask, uint32_t NodeIndex, ui_layout_tree *Tree)
{
    ui_layout_node *Node = GetLayoutNode(NodeIndex, Tree);

    if(Node)
    {
        IterateLinkedList(Node, ui_layout_node *, Child)
        {
            if(HandlePointerRelease(Position, ButtonMask, Child->Index, Tree))
            {
                return true;
            }
        }

        // Should we check the pointer id?
        // Should also check the ButtonMask

        if((Node->Flags & LayoutNodeFlag::HasCapturedPointer) != LayoutNodeFlag::None)
        {
            Node->Flags &= ~(LayoutNodeFlag::HasCapturedPointer | LayoutNodeFlag::UseFocusedStyle);

            Tree->CapturedNodeIndex = InvalidLayoutNodeIndex;

            return true;
        }
    }

    return false;
}

// This is a dead simple implementation, but it allows us to draw hovered styles.
// It's is easy to extend this implementation to handle other stuff. But it is sufficient for now.
// A better way to do this would be to not ask for the node index. Fine for now.

static bool
HandlePointerHover(vec2_float Position, uint32_t NodeIndex, ui_layout_tree *Tree)
{
    ui_layout_node *Node = GetLayoutNode(NodeIndex, Tree);

    if(Node)
    {
        if(IsMouseInsideOuterBox(Position, Node))
        {
            IterateLinkedList(Node, ui_layout_node *, Child)
            {
                bool IsHandled = HandlePointerHover(Position, Child->Index, Tree);
                if(IsHandled)
                {
                    return true;
                }
            }

            Node->Flags |= LayoutNodeFlag::UseHoveredStyle;

            return true;
        }
    }

    return false;
}

// When is this actually called. This is a bit weird. We have to know the captured node.
// Access it, then if it's draggable apply the delta. Uhm. We can use some sort of state, but we could
// also iterate the tree and use a node state instead. I think this is easier though. At least, since we do
// not need to handle multiple focus yet. Perhaps this will change, but it should be trivial to implement.

static void
HandlePointerMove(vec2_float Delta, ui_layout_tree *Tree)
{
    ui_layout_node *CapturedNode = GetLayoutNode(Tree->CapturedNodeIndex, Tree);

    if(CapturedNode)
    {
        if(CapturedNode->LegacyFlags & UILayoutNode_IsDraggable)
        {
            CapturedNode->Bounds.X += Delta.X;
            CapturedNode->Bounds.Y += Delta.Y;
        }
    }
}

// ----------------------------------------------------------------------------------
// @Public: Layout Pass


// (WIP)
 
static void
WrapText(ui_text *Text, ui_layout_node *Node)
{
    float LineWidth = 0.f;

    for(uint32_t WordIdx = 0; WordIdx < Text->WordCount; ++WordIdx)
    {
        LineWidth += Text->Words[WordIdx].Advance;

        if(LineWidth)
        {
            break;
        }
    }
}


static void
ComputeLayout(ui_layout_node *Node, layout_bounds ParentBounds, bool &Changed)
{
    layout_bounds LastBounds = Node->Bounds;

    Node->Bounds =
    {
        .X      = ParentBounds.X,
        .Y      = ParentBounds.Y,
        .Width  = Max(Node->MinSize.X, Min(Node->Size.X, Node->MaxSize.X)),
        .Height = Max(Node->MinSize.Y, Min(Node->Size.Y, Node->MaxSize.Y)),
    };

    if(Node->Bounds != LastBounds)
    {
        Changed = true;
    }

    IterateLinkedList(Node, ui_layout_node *, Child)
    {
        ComputeLayout(Child, Node->Bounds, Changed);
    }
}


static void
ComputeTreeLayout(ui_layout_tree *Tree)
{
    ui_layout_node *Root = GetLayoutRoot(Tree);

    while(true)
    {
        bool Changed = false;

        ComputeLayout(Root, Root->Bounds, Changed);

        if (!Changed)
        {
            break;
        }
    }
}

// TODO: For some reasons, I really dislike how this is implemented.

static ui_paint_buffer
GeneratePaintBuffer(ui_layout_tree *Tree, ui_cached_style *CachedBuffer, memory_arena *Arena)
{
    ui_layout_node **NodeBuffer   = PushArray(Arena, ui_layout_node *, Tree->NodeCount);
    uint32_t         WriteIndex   = 0;
    uint32_t         ReadIndex    = 0;
    uint32_t         CommandCount = 0;

    NodeBuffer[WriteIndex++] = GetLayoutRoot(Tree);

    while (NodeBuffer && ReadIndex < WriteIndex)
    {
        ui_layout_node *Node = NodeBuffer[ReadIndex++];

        if(Node)
        {
            ui_paint_command   &Command = Tree->PaintBuffer[CommandCount++];
            ui_cached_style    *Cached  = CachedBuffer + Node->StyleIndex;       // WARN: Dangerous!
            ui_paint_properties Paint   = Cached->Default.MakePaintProperties();

            if ((Node->Flags & LayoutNodeFlag::UseFocusedStyle) != LayoutNodeFlag::None)
            {
                Paint = Cached->Focused.InheritPaintProperties(Paint);
            } else
            if ((Node->Flags & LayoutNodeFlag::UseHoveredStyle) != LayoutNodeFlag::None)
            {
                Paint = Cached->Hovered.InheritPaintProperties(Paint);

                Node->Flags &= ~LayoutNodeFlag::UseHoveredStyle;
            }

            // Set Geometry
            Command.Rectangle     = GetNodeOuterRect(Node);
            Command.RectangleClip = {};

            // Set Resources Keys
            Command.TextKey       = MakeNodeResourceKey(UIResource_Text , Node->Index, Tree);
            Command.ImageKey      = MakeNodeResourceKey(UIResource_Image, Node->Index, Tree);

            // Set Paint Properties
            Command.CornerRadius  = Paint.CornerRadius.Value;
            Command.Softness      = Paint.Softness.Value;
            Command.BorderWidth   = Paint.BorderWidth.Value;
            Command.Color         = Paint.Color.Value;
            Command.BorderColor   = Paint.BorderColor.Value;

            IterateLinkedList(Node, ui_layout_node *, Child)
            {
                NodeBuffer[WriteIndex++] = Child;
            }
        }
    }

    ui_paint_buffer Result = {.Commands = Tree->PaintBuffer, .Size = CommandCount};
    return Result;
}
