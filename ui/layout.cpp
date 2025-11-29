// [Helpers]

typedef struct resolved_unit
{
    float Value;
    bool  Resolved;
} resolved_unit;

static resolved_unit
TryConvertUnitToFloat(ui_unit Unit, float AvSpace)
{
    resolved_unit Result = {0};

    if(Unit.Type == UIUnit_Float32)
    {
        Result.Value    = Unit.Float32 <= AvSpace ? Unit.Float32 : AvSpace;
        Result.Resolved = 1;
    } else
    if(Unit.Type == UIUnit_Percent)
    {
        Result.Value    = (Unit.Percent / 100.f) * AvSpace;
        Result.Resolved = 1;
    }

    return Result;
}

// -----------------------------------------------
// Tree/Node internal implementation (Helpers/Types)

typedef struct ui_parent_stack
{
    uint64_t NextWrite;
    uint64_t Size;

    ui_layout_node **Nodes;
} ui_parent_stack;

typedef struct ui_layout_box
{
    // Output
    vec2_float FixedOuterPosition;
    vec2_float FixedOuterSize;
    vec2_float FixedInnerPosition;
    vec2_float FixedInnerSize;
    vec2_float FixedContentPosition;
    vec2_float FixedContentSize;

    // Transformations (Applied every time we place)
    vec2_float ScrollOffset;
    vec2_float DragOffset;
} ui_layout_box;

enum class LayoutDirection
{
    None       = 0,
    Horizontal = 1,
    Vertical   = 2,
};

enum class Sizing
{
    None    = 0,
    Fit     = 1,
    Grow    = 2,
    Percent = 3,
    Fixed   = 4,
    Stretch = 5,
};

struct ui_sizing_bounds
{
    float Min;
    float Max;
};

struct ui_sizing_axis
{
    Sizing Type;
    union
    {
        ui_sizing_bounds Bounds;
        float            Percent;
    };
};

struct ui_sizing
{
    ui_sizing_axis Horizontal;
    ui_sizing_axis Vertical;
};

struct ui_layout_node
{
    // Hierarchy
    ui_layout_node *Parent;
    ui_layout_node *First;
    ui_layout_node *Last;
    ui_layout_node *Next;
    ui_layout_node *Prev;

    // Extra Info
    uint32_t SubIndex;
    uint32_t Index;
    uint32_t ChildCount;

    // Layout Information
    float           Width;
    float           Height;
    ui_padding      Padding;
    uint16_t        Spacing;
    LayoutDirection Direction;
    ui_sizing       Sizing;
    float           Grow;
    float           Shrink;

    // Value
    ui_layout_box Value;
    uint32_t      Flags;
};

typedef struct ui_layout_tree
{
    // Nodes
    uint64_t             NodeCapacity;
    uint64_t             NodeCount;
    ui_layout_node *Nodes;

    // Depth
    ui_parent_stack ParentStack;
} ui_layout_tree;

static rect_float
GetOuterBoxRect(ui_layout_box *Box)
{
    vec2_float ScreenPosition = Box->FixedOuterPosition + Box->DragOffset + Box->ScrollOffset;
    rect_float Result = rect_float::FromXYWH(ScreenPosition.X, ScreenPosition.Y, Box->FixedOuterSize.X, Box->FixedOuterSize.Y);
    return Result;
}

static rect_float
GetInnerBoxRect(ui_layout_box *Box)
{
    vec2_float ScreenPosition = Box->FixedInnerPosition + Box->DragOffset + Box->ScrollOffset;
    rect_float Result = rect_float::FromXYWH(ScreenPosition.X, ScreenPosition.Y, Box->FixedInnerSize.X, Box->FixedInnerSize.Y);
    return Result;
}

static rect_float
GetContentBoxRect(ui_layout_box *Box)
{
    vec2_float ScreenPosition = Box->FixedContentPosition + Box->DragOffset + Box->ScrollOffset;
    rect_float Result = rect_float::FromXYWH(ScreenPosition.X, ScreenPosition.Y, Box->FixedContentSize.X, Box->FixedContentSize.Y);
    return Result;
}

static void
ComputeNodeBoxes(ui_layout_box *Box, float BorderWidth, ui_padding Padding, float Width, float Height)
{
    VOID_ASSERT(Box);

    Box->FixedOuterSize   = vec2_float(Width, Height);
    Box->FixedInnerSize   = Box->FixedOuterSize - vec2_float(BorderWidth * 2, BorderWidth * 2);
    Box->FixedContentSize = Box->FixedInnerSize - vec2_float(Padding.Left + Padding.Right, Padding.Top + Padding.Bot);
}

static void
PlaceLayoutBoxes(ui_layout_box *Box, vec2_float Origin, float BorderWidth, ui_padding Padding)
{
    VOID_ASSERT(Box);

    Box->FixedOuterPosition   = Origin;
    Box->FixedInnerPosition   = Box->FixedOuterPosition + (vec2_float(BorderWidth , BorderWidth));
    Box->FixedContentPosition = Box->FixedInnerPosition + (vec2_float(Padding.Left, Padding.Top));
}

static bool 
IsValidLayoutNode(ui_layout_node *Node)
{
    bool Result = Node && Node->Index != InvalidLayoutNodeIndex;
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
GetFreeLayoutNode(ui_layout_tree *Tree)
{
    VOID_ASSERT(Tree && "Calling GetFreeLayoutNode with a NULL tree");

    ui_layout_node *Result = 0;

    if (Tree->NodeCount < Tree->NodeCapacity)
    {
        Result = Tree->Nodes + Tree->NodeCount;
        Result->Index = Tree->NodeCount;

        ++Tree->NodeCount;
    }

    return Result;
}

static ui_layout_node *
NodeToLayoutNode(ui_node Node, ui_layout_tree *Tree)
{
    ui_layout_node *Result = GetLayoutNode(Node.IndexInTree, Tree);
    return Result;
}

static ui_parent_stack
BeginParentStack(uint64_t Size, memory_arena *Arena)
{
    ui_parent_stack Result = {0};
    Result.Nodes = PushArray(Arena, ui_layout_node *, Size);
    Result.Size  = Size;

    return Result;
}

static bool
IsValidParentStack(ui_parent_stack *Stack)
{
    bool Result = (Stack) && (Stack->Size);
    return Result;
}

static void
PushParentStack(ui_layout_node *Node, ui_parent_stack *Stack)
{
    VOID_ASSERT(IsValidParentStack(Stack));

    if(Stack->NextWrite < Stack->Size)
    {
        Stack->Nodes[Stack->NextWrite++] = Node;
    }
}

static ui_layout_node *
PeekParentStack(ui_parent_stack *Stack)
{
    VOID_ASSERT(IsValidParentStack(Stack));

    ui_layout_node *Result = 0;

    if(Stack->NextWrite > 0)
    {
        Result = Stack->Nodes[Stack->NextWrite - 1];
    }

    return Result;
}

static void
PopParentStack(ui_parent_stack *Stack)
{
    VOID_ASSERT(IsValidParentStack(Stack));

    if(Stack->NextWrite > 0)
    {
        --Stack->NextWrite;
    }
}

static ui_layout_node *
CreateAndInsertLayoutNode(uint32_t Flags, ui_subtree *Subtree)
{
    VOID_ASSERT(Subtree);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    VOID_ASSERT(Tree);

    ui_layout_node *Result = GetFreeLayoutNode(Tree);
    if(Result)
    {
        Result->Flags    = Flags;
        Result->SubIndex = Subtree->Id;

        if (!IsValidParentStack(&Tree->ParentStack))
        {
            Tree->ParentStack = BeginParentStack(Tree->NodeCapacity, Subtree->Transient);
        }

        Result->Last   = 0;
        Result->Next   = 0;
        Result->First  = 0;
        Result->Parent = PeekParentStack(&Tree->ParentStack);

        if (Result->Parent)
        {
            AppendToDoublyLinkedList(Result->Parent, Result, Result->Parent->ChildCount);
        }

        if (Result->Flags & UILayoutNode_IsParent)
        {
            PushParentStack(Result, &Tree->ParentStack);
        }
    }

    return Result;
}


// -------------------------------------------------------------
// Tree/Node Public API Implementation

static uint64_t
GetLayoutTreeFootprint(uint64_t NodeCount)
{
    uint64_t ArraySize = NodeCount * sizeof(ui_layout_node);
    uint64_t Result    = sizeof(ui_layout_tree) + ArraySize;

    return Result;
}

static ui_layout_tree *
PlaceLayoutTreeInMemory(uint64_t NodeCount, void *Memory)
{
    ui_layout_tree *Result = 0;

    if (Memory)
    {
        Result = (ui_layout_tree *)Memory;
        Result->NodeCapacity = NodeCount;
        Result->Nodes        = (ui_layout_node *)(Result + 1);

        for (uint32_t Idx = 0; Idx < Result->NodeCapacity; Idx++)
        {
            Result->Nodes[Idx].Index = InvalidLayoutNodeIndex;
        }
    }

    return Result;
}

// ------------------------------------------------------------------------------------
// @internal: Tree Queries

static uint32_t
UITreeFindChild(uint32_t NodeIndex, uint32_t ChildIndex, ui_subtree *Subtree)
{
    uint32_t Result = InvalidLayoutNodeIndex;

    ui_layout_node *LayoutNode = GetLayoutNode(NodeIndex, Subtree->LayoutTree);
    if(IsValidLayoutNode(LayoutNode))
    {
        ui_layout_node *Child = LayoutNode->First;
        while(ChildIndex--)
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
UITreeAppendChild(uint32_t ParentIndex, uint32_t ChildIndex, ui_subtree *Subtree)
{
    VOID_ASSERT(ParentIndex != ChildIndex);

    ui_layout_node *Parent = GetLayoutNode(ParentIndex, Subtree->LayoutTree);
    ui_layout_node *Child  = GetLayoutNode(ChildIndex , Subtree->LayoutTree);

    VOID_ASSERT(Parent);
    VOID_ASSERT(Child);

    AppendToDoublyLinkedList(Parent, Child, Parent->ChildCount);
    Child->Parent = Parent;
}

// BUG:
// Does not check if there is enough space to allocate the children.
// An assertion should be enough? Unsure, same for append layout child.
// Perhaps both?

static void
UITreeReserve(uint32_t NodeIndex, uint32_t Amount, ui_subtree *Subtree)
{
    VOID_ASSERT(NodeIndex != InvalidLayoutNodeIndex);
    VOID_ASSERT(Subtree);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    VOID_ASSERT(Tree);

    bool NeedPush = (PeekParentStack(&Tree->ParentStack)->Index != NodeIndex);

    if(NeedPush)
    {
        ui_layout_node *LayoutNode = GetLayoutNode(NodeIndex, Tree);
        VOID_ASSERT(LayoutNode);

        PushParentStack(LayoutNode, &Tree->ParentStack);
    }

    // WARN:
    // Possibly bugged.

    for(uint32_t Idx = 0; Idx < Amount; ++Idx)
    {
        CreateAndInsertLayoutNode(0, Subtree);
    }

    if(NeedPush)
    {
        PopParentStack(&Tree->ParentStack);
    }
}

// NOTE:
// We surely do not want to expose this...

static void
SetLayoutNodeFlags(uint32_t NodeIndex, uint32_t Flags, ui_subtree *Subtree)
{
    VOID_ASSERT(Subtree);
    VOID_ASSERT(Subtree->LayoutTree);

    ui_layout_node *Node = GetLayoutNode(NodeIndex, Subtree->LayoutTree);
    VOID_ASSERT(Node);

    Node->Flags |= Flags;
}

static void
ClearLayoutNodeFlags(uint32_t NodeIndex, uint32_t Flags, ui_subtree *Subtree)
{
    VOID_ASSERT(Subtree);
    VOID_ASSERT(Subtree->LayoutTree);

    ui_layout_node *Node = GetLayoutNode(NodeIndex, Subtree->LayoutTree);
    VOID_ASSERT(Node);

    Node->Flags &= ~Flags;
}

static uint32_t
AllocateLayoutNode(uint32_t Flags, ui_subtree *Subtree)
{
    VOID_ASSERT(Subtree);

    uint32_t Result = InvalidLayoutNodeIndex;

    ui_layout_node *Node = CreateAndInsertLayoutNode(Flags, Subtree);
    if (Node)
    {
        Result = Node->Index;
    }

    return Result;
}

// NOTE:
// Still hate this.

static void
UIEnd()
{
    ui_pipeline *Pipeline = GetCurrentPipeline();
    VOID_ASSERT(Pipeline);

    ui_subtree *Subtree = Pipeline->CurrentSubtree;
    VOID_ASSERT(Subtree);

    ui_layout_tree *LayoutTree = Subtree->LayoutTree;
    VOID_ASSERT(LayoutTree);

    PopParentStack(&LayoutTree->ParentStack);
}

// -----------------------------------------------------------
// UI Scrolling internal Implementation

typedef struct ui_scroll_region
{
    vec2_float    ContentSize;
    float         ScrollOffset;
    float         PixelPerLine;
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
UpdateScrollNode(float ScrolledLines, ui_layout_node *Node, ui_subtree *Subtree, ui_scroll_region *Region)
{
    float      ScrolledPixels = ScrolledLines * Region->PixelPerLine;
    vec2_float WindowSize     = Node->Value.FixedOuterSize;

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

    rect_float WindowContent = GetOuterBoxRect(&Node->Value);
    IterateLinkedList(Node, ui_layout_node *, Child)
    {
        ui_paint_properties *Paint = GetPaintProperties(Child->Index, Subtree);
        if(Paint)
        {
            Child->Value.ScrollOffset = vec2_float(-ScrollDelta.X, -ScrollDelta.Y);

            vec2_float FixedContentSize = Child->Value.FixedOuterSize;
            rect_float ChildContent     = GetOuterBoxRect(&Child->Value);

            if (FixedContentSize.X > 0.0f && FixedContentSize.Y > 0.0f) 
            {
                if (WindowContent.IsIntersecting(ChildContent))
                {
                    Child->Flags &= ~UILayoutNode_DoNotPaint;
                }
                else
                {
                    Child->Flags |= UILayoutNode_DoNotPaint;
                }
            }
        }
    }
}

// -----------------------------------------------------------
// UI Event internal Implementation

typedef enum UIEvent_Type
{
    UIEvent_Hover     = 1 << 0,
    UIEvent_Click     = 1 << 1,
    UIEvent_Resize    = 1 << 2,
    UIEvent_Drag      = 1 << 3,
    UIEvent_Scroll    = 1 << 4,
    UIEvent_TextInput = 1 << 5,
    UIEvent_Key       = 1 << 6,
} UIEvent_Type;

typedef struct ui_hover_event
{
    ui_layout_node *Node;
} ui_hover_event;

typedef struct ui_click_event
{
    ui_layout_node *Node;
} ui_click_event;

typedef struct ui_resize_event
{
    ui_layout_node *Node;
    vec2_float        Delta;
} ui_resize_event;

typedef struct ui_drag_event
{
    ui_layout_node *Node;
    vec2_float        Delta;
} ui_drag_event;

typedef struct ui_scroll_event
{
    ui_layout_node *Node;
    float             Delta;
} ui_scroll_event;

typedef struct ui_text_input_event
{
    byte_string     Text;
    ui_layout_node *Node;
} ui_text_input_event;

typedef struct ui_key_event
{
    uint32_t             Keycode;
    ui_layout_node *Node;
} ui_key_event;

typedef struct ui_event
{
    UIEvent_Type Type;
    union
    {
        ui_hover_event      Hover;
        ui_click_event      Click;
        ui_resize_event     Resize;
        ui_drag_event       Drag;
        ui_scroll_event     Scroll;
        ui_text_input_event TextInput;
        ui_key_event        Key;
    };
} ui_event;

typedef struct ui_event_node ui_event_node;
struct ui_event_node
{
    ui_event_node *Next;
    ui_event       Value;
};

static void 
RecordUIEvent(ui_event Event, ui_event_list *EventList, memory_arena *Arena)
{
    VOID_ASSERT(EventList);

    ui_event_node *Node = PushStruct(Arena, ui_event_node);
    if(Node)
    {
        Node->Value = Event;
        AppendToLinkedList(EventList, Node, EventList->Count);
    }
}

static void
RecordUIHoverEvent(ui_layout_node *Node, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {};
    Event.Type       = UIEvent_Hover;
    Event.Hover.Node = Node;

    RecordUIEvent(Event, EventList, Arena);
}

static void
RecordUIClickEvent(ui_layout_node *Node, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {};
    Event.Type       = UIEvent_Click;
    Event.Click.Node = Node;
    RecordUIEvent(Event, EventList, Arena);
}

static void
RecordUIScrollEvent(ui_layout_node *Node, float Delta, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {};
    Event.Type         = UIEvent_Scroll;
    Event.Scroll.Node  = Node;
    Event.Scroll.Delta = Delta;
    RecordUIEvent(Event, EventList, Arena);
}

static void
RecordUIResizeEvent(ui_layout_node *Node, vec2_float Delta, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {};
    Event.Type         = UIEvent_Resize;
    Event.Resize.Node  = Node;
    Event.Resize.Delta = Delta;
    RecordUIEvent(Event, EventList, Arena);
}

static void
RecordUIDragEvent(ui_layout_node *Node, vec2_float Delta, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {};
    Event.Type       = UIEvent_Drag;
    Event.Drag.Node  = Node;
    Event.Drag.Delta = Delta;
    RecordUIEvent(Event, EventList, Arena);
}

static void
RecordUIKeyEvent(uint32_t Keycode, ui_layout_node *Node, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {};
    Event.Type        = UIEvent_Key;
    Event.Key.Node    = Node;
    Event.Key.Keycode = Keycode;
    RecordUIEvent(Event, EventList, Arena);
}

static void
RecordUITextInputEvent(byte_string Text, ui_layout_node *Node, ui_event_list *EventList, memory_arena *Arena)
{
    ui_event Event = {};
    Event.Type           = UIEvent_TextInput;
    Event.TextInput.Node = Node;
    Event.TextInput.Text = Text;
    RecordUIEvent(Event, EventList, Arena);
}

static bool
IsMouseInsideOuterBox(vec2_float MousePosition, uint32_t NodeIndex, ui_subtree *Subtree)
{
    VOID_ASSERT(Subtree);
    VOID_ASSERT(Subtree->LayoutTree);

    ui_layout_node *Node = GetLayoutNode(NodeIndex, Subtree->LayoutTree);
    VOID_ASSERT(Node);

    vec2_float OuterSize  = Node->Value.FixedOuterSize;
    vec2_float OuterPos   = Node->Value.FixedOuterPosition + Node->Value.DragOffset + Node->Value.ScrollOffset;
    vec2_float OuterHalf  = vec2_float(OuterSize.X * 0.5f, OuterSize.Y * 0.5f);
    vec2_float Center     = OuterPos + OuterHalf;
    vec2_float LocalMouse = MousePosition - Center;

    rect_sdf_params Params = {0};
    Params.HalfSize      = OuterHalf;
    Params.PointPosition = LocalMouse;

    float Distance = RoundedRectSDF(Params);
    return Distance <= 0.f;
}

static bool
IsMouseInsideBorder(vec2_float MousePosition, ui_layout_node *Node)
{
    vec2_float InnerSize   = Node->Value.FixedInnerSize;
    vec2_float InnerPos    = Node->Value.FixedInnerPosition + Node->Value.DragOffset + Node->Value.ScrollOffset;
    vec2_float InnerHalf   = vec2_float(InnerSize.X * 0.5f, InnerSize.Y * 0.5f);
    vec2_float InnerCenter = InnerPos + InnerHalf;

    rect_sdf_params Params = {0};
    Params.Radius        = 0.f;
    Params.HalfSize      = InnerHalf;
    Params.PointPosition = MousePosition - InnerCenter;

    float Distance = RoundedRectSDF(Params);
    return Distance >= 0.f;
}

static void
ClearUIEvents(ui_event_list *EventList)
{
    VOID_ASSERT(EventList);

    EventList->First = 0;
    EventList->Last  = 0;
    EventList->Count = 0;
}

static UIIntent_Type
DetermineResizeIntent(vec2_float MousePosition, ui_layout_node *Node, ui_subtree *Subtree)
{
    vec2_float InnerSize = Node->Value.FixedInnerSize;
    vec2_float InnerPos  = Node->Value.FixedInnerPosition;

    float Left        = InnerPos.X;
    float Top         = InnerPos.Y;
    float Width       = InnerSize.X;
    float Height      = InnerSize.Y;
    float BorderWidth = GetPaintProperties(Node->Index, Subtree)->Properties.BorderWidth; // WARN: Not safe.

    float CornerTolerance = 5.f;
    float CornerX         = Left + Width;
    float CornerY         = Top  + Height;

    rect_float RightEdge  = rect_float::FromXYWH(Left + Width, Top, BorderWidth, Height);
    rect_float BottomEdge = rect_float::FromXYWH(Left, Top + Height, Width, BorderWidth);
    rect_float Corner     = rect_float::FromXYWH(CornerX, CornerY, CornerTolerance, CornerTolerance);

    UIIntent_Type Result = UIIntent_None;

    if(Corner.IsPointInside(MousePosition))
    {
        Result = UIIntent_ResizeXY;
    } else
    if(RightEdge.IsPointInside(MousePosition))
    {
        Result = UIIntent_ResizeX;
    } else
    if(BottomEdge.IsPointInside(MousePosition))
    {
        Result = UIIntent_ResizeY;
    }

    return Result;
}

// NOTE:
// The hovered node is set on the global state by: UISetNodeHover.
// We call: UIHasNodeHover to check if that global state was set.
// It's a bit opaque but it fits well with other parts of the code.

// WARN:
// Errr actually, it's not that good. Maybe re-visit.

static void
FindHoveredNode(vec2_float MousePosition, ui_layout_node *Node, ui_subtree *Subtree)
{
    if(IsMouseInsideOuterBox(MousePosition, Node->Index, Subtree))
    {
        IterateLinkedListBackward(Node, ui_layout_node *, Child)
        {
            FindHoveredNode(MousePosition, Child, Subtree);

            if(UIHasNodeHover())
            {
                return;
            }
        }

        UISetNodeHover(Node->Index, Subtree);
    }

    return;
}

static void
AttemptNodeFocus(vec2_float MousePosition, ui_layout_node *Root, ui_subtree *Subtree)
{
    FindHoveredNode(MousePosition, Root, Subtree);
    if(UIHasNodeHover())
    {
        ui_hovered_node Hovered = UIGetNodeHover();
        VOID_ASSERT(Hovered.Subtree == Subtree);

        ui_layout_node *Node   = GetLayoutNode(Hovered.Index, Hovered.Subtree->LayoutTree);
        memory_arena   *Arena  = Subtree->Transient;
        ui_event_list  *Events = &Subtree->Events;

        bool MouseClicked = OSIsMouseClicked(OSMouseButton_Left);

        if ((Node->Flags & UILayoutNode_IsResizable) && MouseClicked)
        {
            if (IsMouseInsideBorder(MousePosition, Node))
            {
                UIIntent_Type Intent = DetermineResizeIntent(MousePosition, Node, Subtree);
                if (Intent != UIIntent_None)
                {
                    UISetNodeFocus(Node->Index, Subtree, 0, Intent);
                    return;
                }
            }
        }

        if ((Node->Flags & UILayoutNode_IsDraggable) && MouseClicked)
        {
            UISetNodeFocus(Node->Index, Subtree, 0, UIIntent_Drag);
            return;
        }

        if (MouseClicked)
        {
            if ((Node->Flags & UILayoutNode_HasTextInput))
            {
                UISetNodeFocus(Node->Index, Subtree, 1, UIIntent_EditTextInput);
            }

            RecordUIClickEvent(Node, Events, Arena);
            return;
        }

        // NOTE:
        // Should this take focus?

        float ScrollDelta = OSGetScrollDelta();
        if ((Node->Flags & UILayoutNode_HasScrollRegion) && ScrollDelta != 0.f)
        {
            RecordUIScrollEvent(Node, ScrollDelta, Events, Arena);
        }

        RecordUIHoverEvent(Node, Events, Arena);
    }
}

static void
GenerateFocusedNodeEvents(ui_focused_node Focused, ui_event_list *Events, memory_arena *Arena)
{
    vec2_float MouseDelta = OSGetMouseDelta();
    bool      MouseMoved = (MouseDelta.X != 0 || MouseDelta.Y != 0);

    ui_layout_node *Node = GetLayoutNode(Focused.Index, Focused.Subtree->LayoutTree);
    VOID_ASSERT(Node);

    if(MouseMoved && Focused.Intent >= UIIntent_ResizeX && Focused.Intent <= UIIntent_ResizeXY)
    {
        vec2_float Delta = vec2_float(0.f, 0.f);

        if(Focused.Intent == UIIntent_ResizeX)
        {
            Delta.X = MouseDelta.X;
        } else
        if(Focused.Intent == UIIntent_ResizeY)
        {
            Delta.Y = MouseDelta.Y;
        } else
        if(Focused.Intent == UIIntent_ResizeXY)
        {
            Delta = MouseDelta;
        }

        RecordUIResizeEvent(Node, Delta, Events, Arena);
    } else
    if(MouseMoved && Focused.Intent == UIIntent_Drag)
    {
        RecordUIDragEvent(Node, MouseDelta, Events, Arena);
    } else
    if(Focused.Intent == UIIntent_EditTextInput)
    {
        // NOTE:
        // Might be a more general event.

        os_button_playback *Playback = OSGetButtonPlayback();
        VOID_ASSERT(Playback);

        for(uint32_t Idx = 0; Idx < Playback->Count; ++Idx)
        {
            os_button_action Action = Playback->Actions[Idx];
            if(Action.IsPressed)
            {
                uint32_t Key = Action.Keycode;
                RecordUIKeyEvent(Key, Node, Events, Arena);
            }
        }

        os_utf8_playback *Text = OSGetTextPlayback();
        VOID_ASSERT(Text);

        if(Text->Count)
        {
            byte_string TextAsString = ByteString(Text->UTF8, Text->Count);
            RecordUITextInputEvent(TextAsString, Node, Events, Arena);
        }
    }
}

// WARN:
// This code is quite garbage.

static void
GenerateUIEvents(vec2_float MousePosition, ui_layout_node *Root, ui_subtree *Subtree)
{
    ui_event_list *Events = &Subtree->Events;
    memory_arena  *Arena  = Subtree->Transient;
    VOID_ASSERT(Arena);

    if(!UIHasNodeFocus())
    {
        AttemptNodeFocus(MousePosition, Root, Subtree);
    }

    // BUG:
    // The weird part is that this is done on a per-tree basis.
    // This makes pretty much no sense. Well... Idk?

    if(UIHasNodeFocus())
    {
        ui_focused_node Focused = UIGetNodeFocus();

        GenerateFocusedNodeEvents(Focused, Events, Arena);
    }
}

static void
ProcessUIEvents(ui_event_list *Events, ui_subtree *Subtree)
{
    VOID_ASSERT(Events);

    IterateLinkedList(Events, ui_event_node *, Node)
    {
        ui_event *Event = &Node->Value;

        switch(Event->Type)
        {

        case UIEvent_Hover:
        {
            ui_hover_event Hover = Event->Hover;
            VOID_ASSERT(Hover.Node);

            // TODO: Modify the paint properties.
            // SetNodeStyleState(StyleState_Hovered, Hover.Node->Index, Subtree);
        } break;

        case UIEvent_Click:
        {
        } break;

        case UIEvent_Resize:
        {
            ui_resize_event Resize = Event->Resize;

            ui_paint_properties *Paint = GetPaintProperties(Resize.Node->Index, Subtree);
            if(Paint)
            {
                // TODO: Reimplement this.
            }
        } break;

        // NOTE:
        // Perhaps these are animations? I do not have an animation system right now.

        case UIEvent_Drag:
        {
            ui_drag_event Drag = Event->Drag;

            ui_layout_node *Node = Drag.Node;
            VOID_ASSERT(Node);

            Node->Value.DragOffset += Drag.Delta;
        } break;

        case UIEvent_Scroll:
        {
            ui_scroll_event Scroll = Event->Scroll;
            VOID_ASSERT(Scroll.Node);
            VOID_ASSERT(Scroll.Delta);

            ui_scroll_region *Region = (ui_scroll_region *)QueryNodeResource(Scroll.Node->Index, Subtree, UIResource_ScrollRegion, UIState.ResourceTable);
            VOID_ASSERT(Region);

            UpdateScrollNode(Scroll.Delta, Scroll.Node, Subtree, Region);
        } break;

        case UIEvent_Key:
        {
            ui_key_event Key = Event->Key;
            VOID_ASSERT(Key.Node);

            ui_layout_node    *Node  = Key.Node;
            ui_resource_table *Table = UIState.ResourceTable;

            if((Node->Flags & UILayoutNode_HasTextInput))
            {
                ui_text_input *Input = (ui_text_input *)QueryNodeResource(Node->Index, Subtree, UIResource_TextInput, Table);
                VOID_ASSERT(Input);

                if(Input->OnKey)
                {
                    Input->OnKey((OSInputKey_Type)Key.Keycode, Input->OnKeyUserData);
                }
            }
        } break;

        case UIEvent_TextInput:
        {
            ui_text_input_event TextInput = Event->TextInput;
            VOID_ASSERT(TextInput.Node);

            ui_layout_node    *Node  = TextInput.Node;
            ui_resource_table *Table = UIState.ResourceTable;

            ui_text       *Text  = (ui_text *)      QueryNodeResource(Node->Index, Subtree, UIResource_Text     , Table);
            ui_text_input *Input = (ui_text_input *)QueryNodeResource(Node->Index, Subtree, UIResource_TextInput, Table);

            VOID_ASSERT(Text);
            VOID_ASSERT(Input);

            if(IsValidByteString(TextInput.Text))
            {
                ui_paint_properties *Paint = GetPaintProperties(Node->Index, Subtree);
                if(Paint)
                {
                    uint32_t CountBeforeAppend = Text->ShapedCount;
                    UITextAppend_(TextInput.Text, Paint->Properties.Font, Text);
                    uint32_t CountAfterAppend = Text->ShapedCount;

                    UITextInputMoveCaret_(Text, Input, CountAfterAppend - CountBeforeAppend);

                    // NOTE: BAD! IDK WHAT TO DO!
                    ByteStringAppendTo(Input->UserBuffer, TextInput.Text, Input->internalCount);
                    Input->internalCount += TextInput.Text.Size;
                }
            }

            if(Text->ShapedCount > 0)
            {
                Node->Flags |= UILayoutNode_HasText;
            }
            else
            {
                Node->Flags &= ~UILayoutNode_HasText;
            }
        } break;

        }
    }
}

// ----------------------------------------------------------------------------------
// Text Layout Implementation

static float
GetUITextOffset(UIAlign_Type Align, float FixedContentSize, float TextSize)
{
    float Result = 0.f;

    if(Align == UIAlign_Center)
    {
        Result = (FixedContentSize - TextSize) * 0.5f;
    } else
    if(Align == UIAlign_End)
    {
        Result = (FixedContentSize - TextSize);
    }

    return Result;
}

static float
GetUITextSpace(ui_layout_node *Node, ui_unit Width)
{
    VOID_ASSERT(Node);

    float Result = Node->Value.FixedContentSize.X;

    if(Width.Type == UIUnit_Auto)
    {
        ui_layout_node *Parent = Node->Parent;
        VOID_ASSERT(Parent);

        Result = Parent->Value.FixedContentSize.X;
    }

    return Result;
}

static void
AlignUITextLine(float ContentWidth, float LineWidth, UIAlign_Type XAlign, ui_shaped_glyph *Glyphs, uint32_t Count)
{
    float XAlignOffset = GetUITextOffset(XAlign, ContentWidth, LineWidth);

    if(XAlignOffset != 0.f)
    {
        for(uint32_t Idx = 0; Idx < Count; ++Idx)
        {
            Glyphs[Idx].Position.Left += XAlignOffset;
            Glyphs[Idx].Position.Right += XAlignOffset;
        }
    }
}

// -----------------------------------------------------------
// Layout Pass internal Helpers/Types

DEFINE_TYPED_QUEUE(Node, node, ui_layout_node *);

// NOTE:
// This code is really garbage.

static void
PreOrderPlaceSubtree(ui_layout_node *Root, ui_subtree *Subtree)
{
    VOID_ASSERT(Root);
    VOID_ASSERT(Subtree);
    VOID_ASSERT(Subtree->Transient);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    VOID_ASSERT(Tree);

    ui_resource_table *Table = UIState.ResourceTable;
    VOID_ASSERT(Table);

    node_queue Queue = BeginNodeQueue((typed_queue_params){.QueueSize = Tree->NodeCount}, Subtree->Transient);
    if (Queue.Data)
    {
        ui_paint_properties *RootPaint = GetPaintProperties(Root->Index, Subtree);
        if(RootPaint)
        {
            PlaceLayoutBoxes(&Root->Value, vec2_float(0, 0), RootPaint->Properties.BorderWidth, RootPaint->Properties.Padding);
        }

        PushNodeQueue(&Queue, Root);

        while (!IsNodeQueueEmpty(&Queue))
        {
            // TODO: ReImplement the placer.
        }
    }
}

// ----------------------------------------------------------------------------------
// @internal: PreOrder Measure Passes Implementation

static void
PreOrderMeasureSubtree(ui_layout_node *Root, ui_subtree *Subtree)
{
}

static void
SizeContainerAlongAxis(ui_layout_node *Root, ui_subtree *Subtree, bool IsXAxis)
{
    node_queue Queue = BeginNodeQueue({.QueueSize = Subtree->LayoutTree->NodeCount}, Subtree->Transient);

    PushNodeQueue(&Queue, Root);

    while(!IsNodeQueueEmpty(&Queue))
    {
        ui_layout_node *Parent = PopNodeQueue(&Queue);

        float ParentSize    = IsXAxis ? Parent->Width : Parent->Height;
        float ParentPadding = IsXAxis ? Parent->Padding.Left + Parent->Padding.Right : Parent->Padding.Top + Parent->Padding.Bot;
        float ParentSpacing = Parent->Spacing;

        float InnerContentSize       = 0;
        float TotalGrow              = 0.f;
        float TotalShrink            = 0.f;
        float TotalPaddingAndSpacing = ParentPadding;

        bool IsSizingAlongParentAxis = ( IsXAxis && Parent->Direction == LayoutDirection::Horizontal) ||
                                       (!IsXAxis && Parent->Direction == LayoutDirection::Vertical);

        IterateLinkedList(Parent, ui_layout_node *, Child)
        {
            ui_sizing_axis ChildSizing = IsXAxis ? Child->Sizing.Horizontal : Child->Sizing.Vertical;
            float          ChildSize   = IsXAxis ? Child->Width             : Child->Height;

            if(Child->ChildCount > 0)
            {
                PushNodeQueue(&Queue, Child);
            }

            if(IsSizingAlongParentAxis)
            {
                InnerContentSize += (ChildSizing.Type == Sizing::Percent ? 0.f : ChildSize);

                if(ChildSizing.Type == Sizing::Grow)
                {
                    TotalGrow += Child->Grow;
                }

                if(Child != Parent->First)
                {
                    InnerContentSize       += ParentSpacing;
                    TotalPaddingAndSpacing += ParentSpacing;
                }

                TotalShrink += Child->Shrink * ChildSize;
            }
            else
            {
                InnerContentSize = max(ChildSize, InnerContentSize);
            }
        }

        IterateLinkedList(Parent, ui_layout_node *, Child)
        {
            ui_sizing_axis ChildSizing = IsXAxis ? Child->Sizing.Horizontal : Child->Sizing.Vertical;
            float         *ChildSize   = IsXAxis ? &Child->Width            : &Child->Height;

            if(ChildSizing.Type == Sizing::Percent)
            {
                *ChildSize = (ParentSize - TotalPaddingAndSpacing) * ChildSizing.Percent;

                if(IsSizingAlongParentAxis)
                {
                    InnerContentSize += *ChildSize;
                }
            }
        }

        if(IsSizingAlongParentAxis)
        {
            float FreeSpace = ParentSize - ParentPadding - InnerContentSize;

            if(FreeSpace < 0.f && TotalShrink > 0.f)
            {
                IterateLinkedList(Parent, ui_layout_node *, Child)
                {
                    float *ChildSize = IsXAxis ? &Child->Width : &Child->Height;
                    *ChildSize = ((Child->Shrink * (*ChildSize)) / TotalShrink) * FreeSpace;
                }
            } else
            if(FreeSpace > 0.f && TotalGrow > 0.f)
            {
                IterateLinkedList(Parent, ui_layout_node *, Child)
                {
                    ui_sizing_axis ChildSizing = IsXAxis ? Child->Sizing.Horizontal : Child->Sizing.Vertical;

                    if(ChildSizing.Type == Sizing::Grow)
                    {
                        float *ChildSize = IsXAxis ? &Child->Width : &Child->Height;
                        *ChildSize = (Child->Grow / TotalGrow) * FreeSpace;
                    }
                }
            }
        }
        else
        {
            float ParentMaxSize = ParentSize - ParentPadding;

            IterateLinkedList(Parent, ui_layout_node *, Child)
            {
                ui_sizing_axis ChildSizing = IsXAxis ? Child->Sizing.Horizontal : Child->Sizing.Vertical;
                float         *ChildSize   = IsXAxis ? &Child->Width            : &Child->Height;

                float ChildMinSize = ChildSizing.Bounds.Min;
                float ChildMaxSize = ChildSizing.Bounds.Max;

                if(ChildSizing.Type == Sizing::Stretch)
                {
                    *ChildSize = min(ChildMaxSize, ParentMaxSize);
                }

                *ChildSize = max(ChildMinSize, min(*ChildSize, ParentMaxSize));
            }
        }
    }
}

// ----------------------------------------------------------------------------------
// @Internal: PostOrder Measure Pass Implementation

static void
PostOrderMeasureSubtree(ui_layout_node *Root, ui_subtree *Subtree)
{
    IterateLinkedList(Root, ui_layout_node *, Child)
    {
        PostOrderMeasureSubtree(Child, Subtree);
    }

    // TODO: ReImplement.

}

// -----------------------------------------------------------------------------------
// Layout Pass Public API implementation

// NOTE:
// Core?

static void
UpdateSubtreeState(ui_subtree *Subtree)
{
    VOID_ASSERT(Subtree);
    VOID_ASSERT(Subtree->LayoutTree);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    ui_layout_node *Root = GetLayoutNode(0, Tree);

    vec2_float MousePosition = OSGetMousePosition();

    ClearUIEvents(&Subtree->Events);
    GenerateUIEvents(MousePosition, Root, Subtree);
    ProcessUIEvents(&Subtree->Events, Subtree);
}

static void
ComputeSubtreeLayout(ui_subtree *Subtree)
{
    TimeFunction;

    VOID_ASSERT(Subtree);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    VOID_ASSERT(Tree);

    ui_layout_node *Root = GetLayoutNode(0, Tree);
    {
        PreOrderMeasureSubtree   (Root, Subtree);
        PostOrderMeasureSubtree  (Root, Subtree);
        PreOrderPlaceSubtree     (Root, Subtree);
    }
}

static void 
PaintSubtree(ui_subtree *Subtree)
{
    TimeFunction;

    VOID_ASSERT(Subtree);
    VOID_ASSERT(Subtree->LayoutTree);
    VOID_ASSERT(Subtree->Transient);

    ui_layout_tree *Tree = Subtree->LayoutTree;
    ui_layout_node *Root = GetLayoutNode(0, Tree);

    if(Root)
    {
        PaintLayoutTreeFromRoot(Root, Subtree);
    }
}
