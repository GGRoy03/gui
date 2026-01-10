#pragma once

#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <xmmintrin.h>
#include <emmintrin.h>


// -----------------------------------------------------------------------------
// Basic Geometry
// -----------------------------------------------------------------------------


static float
GuiBoundingBoxSignedDistanceField(gui_point Point, gui_bounding_box BoundingBox)
{
    gui_dimensions Size   = (gui_dimensions){.Width = BoundingBox.Right - BoundingBox.Left, .Height = BoundingBox.Bottom - BoundingBox.Top};
    gui_dimensions Half   = (gui_dimensions){.Width = Size.Width * 0.5f, .Height = Size.Height * 0.5f};
    gui_point      Start  = (gui_point){.X = BoundingBox.Left, .Y = BoundingBox.Top};
    gui_point      Center = (gui_point){.X = Start.X + Half.Width, .Y = Start.Y + Half.Height};
    gui_point      Local  = (gui_point){.X = Point.X - Center.X, .Y = Point.Y - Center.Y};

    gui_point FirstQuadrant = (gui_point){ .X = fabsf(Local.X) - Half.Width, .Y = fabsf(Local.Y) - Half.Height };

    float OuterX        = (FirstQuadrant.X > 0.0f) ? FirstQuadrant.X : 0.0f;
    float OuterY        = (FirstQuadrant.Y > 0.0f) ? FirstQuadrant.Y : 0.0f;
    float OuterDistance = sqrtf(OuterX * OuterX + OuterY * OuterY);
    float InnerDistance = fminf(fmaxf(FirstQuadrant.X, FirstQuadrant.Y), 0.0f);

    float Result = OuterDistance + InnerDistance;
    return Result;
}


static gui_bool
GuiBoundingBoxesIntersect(gui_bounding_box A, gui_bounding_box B)
{
    gui_bool Result = A.Left < B.Right && A.Right > B.Left && A.Top < B.Bottom && A.Bottom > B.Top;
    return Result;
}

// -----------------------------------------------------------------------------
// Layout Tree & Nodes
// -----------------------------------------------------------------------------


typedef enum Gui_NodeState
{
    Gui_NodeState_None               = 0,
    Gui_NodeState_UseHoveredStyle    = 1 << 0,
    Gui_NodeState_UseFocusedStyle    = 1 << 1,
    Gui_NodeState_HasCapturedPointer = 1 << 2,
    Gui_NodeState_IsClicked          = 1 << 3
} Gui_NodeState;


typedef struct gui_layout_node
{
    uint32_t            Parent;
    uint32_t            First;
    uint32_t            Last;
    uint32_t            Next;
    uint32_t            Prev;
    uint32_t            ChildCount;
    uint32_t            Index;

    gui_point           OutputPosition;
    gui_dimensions      OutputSize;
    gui_dimensions      OutputChildSize;

    gui_size            Size;
    gui_size            MinSize;
    gui_size            MaxSize;
    Gui_Alignment       XAlign;
    Gui_Alignment       YAlign;
    Gui_LayoutDirection Direction;
    gui_padding         Padding;
    float               Spacing;

    gui_dimensions      VisualOffset;
    gui_dimensions      DragOffset;
    gui_dimensions      ScrollOffset;

    uint32_t            State;
    uint32_t            Flags;
} gui_layout_node;


typedef struct gui_parent_node
{
    gui_parent_node *Prev;
    uint32_t Value;
} gui_parent_node;


typedef struct gui_node_buffer
{
    gui_layout_node *Nodes;
    uint64_t Count;
    uint64_t Capacity;
} gui_node_buffer;


typedef struct gui_layout_tree
{
    gui_node_buffer       NodeBuffer;
    gui_paint_properties *PaintBuffer;
    uint64_t              RootIndex;
    uint64_t              CapturedNodeIndex;
    gui_parent_node      *Parent;
} gui_layout_tree;


// -----------------------------------------------------------------------------
// Simple KeyMap replacement (TODO: Actually rewrite this)
// -----------------------------------------------------------------------------


#define KEYMAP_CAPACITY 8192u
#define KEYMAP_EMPTY ((uint64_t)0xFFFFFFFFFFFFFFFFULL)

static uint64_t KeyMap_Keys[KEYMAP_CAPACITY];
static uint32_t KeyMap_Values[KEYMAP_CAPACITY];
static gui_bool KeyMap_Initialized = GUI_FALSE;

static void
GuiKeyMap_Init(void)
{
    if(!KeyMap_Initialized)
    {
        for(uint32_t i = 0; i < KEYMAP_CAPACITY; ++i)
        {
            KeyMap_Keys[i] = KEYMAP_EMPTY;
            KeyMap_Values[i] = (uint32_t)GuiInvalidIndex;
        }
        KeyMap_Initialized = GUI_TRUE;
    }
}

static uint32_t
GuiKeyMap_FindOrInsert(uint64_t Key, gui_bool *Inserted)
{
    // Returns value if found, otherwise inserts and returns GuiInvalidIndex
    GuiKeyMap_Init();

    uint32_t Hash = (uint32_t)(XXH3_64bits(&Key, sizeof(Key)) & (KEYMAP_CAPACITY - 1));
    uint32_t Slot = Hash;

    for(;;)
    {
        uint64_t Stored = KeyMap_Keys[Slot];
        if(Stored == Key)
        {
            if(Inserted) *Inserted = GUI_FALSE;
            return KeyMap_Values[Slot];
        }
        if(Stored == KEYMAP_EMPTY)
        {
            // Insert
            KeyMap_Keys[Slot] = Key;
            KeyMap_Values[Slot] = (uint32_t)GuiInvalidIndex;
            if(Inserted) *Inserted = GUI_TRUE;
            return KeyMap_Values[Slot];
        }
        Slot = (Slot + 1) & (KEYMAP_CAPACITY - 1);
    }
}

static void
GuiKeyMap_Insert(uint64_t Key, uint32_t Value)
{
    GuiKeyMap_Init();

    uint32_t Hash = (uint32_t)(XXH3_64bits(&Key, sizeof(Key)) & (KEYMAP_CAPACITY - 1));
    uint32_t Slot = Hash;

    for(;;)
    {
        uint64_t Stored = KeyMap_Keys[Slot];
        if(Stored == Key || Stored == KEYMAP_EMPTY)
        {
            KeyMap_Keys[Slot] = Key;
            KeyMap_Values[Slot] = Value;
            return;
        }
        Slot = (Slot + 1) & (KEYMAP_CAPACITY - 1);
    }
}

static int
GuiKeyMap_Find(uint64_t Key, uint32_t *OutValue)
{
    GuiKeyMap_Init();

    uint32_t Hash = (uint32_t)(XXH3_64bits(&Key, sizeof(Key)) & (KEYMAP_CAPACITY - 1));
    uint32_t Slot = Hash;

    for(;;)
    {
        uint64_t Stored = KeyMap_Keys[Slot];
        if(Stored == KEYMAP_EMPTY) return 0;
        if(Stored == Key)
        {
            if(OutValue) *OutValue = KeyMap_Values[Slot];
            return 1;
        }
        Slot = (Slot + 1) & (KEYMAP_CAPACITY - 1);
    }
}

// -----------------------------------------------------------------------------
// Node buffer helpers
// -----------------------------------------------------------------------------


static gui_bool GuiIsValidLayoutNode(gui_layout_node *Node)
{
    gui_bool Result = Node && (Node->Index != GuiInvalidIndex);
    return Result;
}


static gui_layout_node *
GuiGetSentinelNode(gui_node_buffer NodeBuffer)
{
    gui_layout_node *Result = NodeBuffer.Nodes + NodeBuffer.Capacity;
    return Result;
}


static gui_layout_node *
GuiGetLayoutNode(gui_node_buffer NodeBuffer, uint64_t Index)
{
    gui_layout_node *Result = NULL;
    if(Index < NodeBuffer.Capacity)
    {
        Result = NodeBuffer.Nodes + Index;
    }
    return Result;
}


static gui_layout_node *
GuiGetFreeLayoutNode(gui_node_buffer *NodeBuffer)
{
    gui_layout_node *Sentinel = GuiGetSentinelNode(*NodeBuffer);
    gui_layout_node *Result   = NULL;

    if(Sentinel->Next != GuiInvalidIndex)
    {
        GUI_ASSERT(NodeBuffer->Count < NodeBuffer->Capacity);

        uint32_t FreeIndex = Sentinel->Next;
        Result = GuiGetLayoutNode(*NodeBuffer, FreeIndex);
        GUI_ASSERT(Result);

        Sentinel->Next = Result->Next;

        Result->First = GuiInvalidIndex;
        Result->Last = GuiInvalidIndex;
        Result->Prev = GuiInvalidIndex;
        Result->Next = GuiInvalidIndex;
        Result->Parent = GuiInvalidIndex;
        Result->ChildCount = 0;
        Result->Index = FreeIndex;

        ++NodeBuffer->Count;
    }

    return Result;
}


static void
GuiAppendLayoutNode(gui_node_buffer NodeBuffer, uint32_t ParentIndex, uint32_t ChildIndex)
{
    gui_layout_node *Parent = GuiGetLayoutNode(NodeBuffer, ParentIndex);
    gui_layout_node *Child = GuiGetLayoutNode(NodeBuffer, ChildIndex);

    if(GuiIsValidLayoutNode(Parent) && GuiIsValidLayoutNode(Child))
    {
        Child->First  = GuiInvalidIndex;
        Child->Last   = GuiInvalidIndex;
        Child->Prev   = GuiInvalidIndex;
        Child->Next   = GuiInvalidIndex;
        Child->Parent = Parent->Index;

        gui_layout_node *First = GuiGetLayoutNode(NodeBuffer, Parent->First);
        if(!GuiIsValidLayoutNode(First))
        {
            Parent->First = Child->Index;
        }

        gui_layout_node *Last  = GuiGetLayoutNode(NodeBuffer, Parent->Last);
        if(GuiIsValidLayoutNode(Last))
        {
            Last->Next  = Child->Index;
        }

        Child->Prev         = Parent->Last;
        Parent->Last        = Child->Index;
        Parent->ChildCount += 1;
    }
}


static gui_bool
GuiIsValidLayoutTree(const gui_layout_tree *Tree)
{
    gui_bool Result = (Tree && Tree->NodeBuffer.Nodes && Tree->NodeBuffer.Count <= Tree->NodeBuffer.Capacity);
    return Result;
}


static gui_memory_footprint
GuiGetLayoutTreeFootprint(uint64_t NodeCount)
{
    uint64_t TreeEnd    = sizeof(gui_layout_tree);
    uint64_t NodesStart = AlignPow2(TreeEnd, AlignOf(gui_layout_node));
    uint64_t NodesEnd   = NodesStart + ((NodeCount + 1) * sizeof(gui_layout_node));
    uint64_t PaintStart = AlignPow2(NodesEnd, AlignOf(gui_paint_properties));
    uint64_t PaintEnd   = PaintStart + (NodeCount * sizeof(gui_paint_properties));

    gui_memory_footprint Result = { .SizeInBytes = PaintEnd, .Alignment = AlignOf(gui_layout_tree) };
    return Result;
}


static gui_layout_tree *
GuiPlaceLayoutTreeInMemory(uint64_t NodeCount, gui_memory_block Block)
{
    gui_layout_tree *Result = NULL;
    gui_memory_region Local = GuiEnterMemoryRegion(Block);

    if(GuiIsValidMemoryRegion(&Local))
    {
        // ORDER IS IMPORTANT!
        gui_layout_tree      *Tree = GuiPushStruct(&Local, gui_layout_tree);
        gui_layout_node      *Nodes = GuiPushArray(&Local, gui_layout_node, NodeCount + 1);
        gui_paint_properties *Paint = GuiPushArray(&Local, gui_paint_properties, NodeCount);

        if(Nodes && Paint && Tree)
        {
            Tree->NodeBuffer.Nodes = Nodes;
            Tree->NodeBuffer.Count = 0;
            Tree->NodeBuffer.Capacity = NodeCount;
            Tree->PaintBuffer = Paint;
            Tree->RootIndex = 0;
            Tree->CapturedNodeIndex = GuiInvalidIndex;
            Tree->Parent = NULL;

            for(uint64_t Idx = 0; Idx < Tree->NodeBuffer.Capacity; ++Idx)
            {
                gui_layout_node *Node = GuiGetLayoutNode(Tree->NodeBuffer, Idx);
                GUI_ASSERT(Node);

                Node->Index      = GuiInvalidIndex;
                Node->First      = GuiInvalidIndex;
                Node->Last       = GuiInvalidIndex;
                Node->Parent     = GuiInvalidIndex;
                Node->Prev       = GuiInvalidIndex;
                Node->ChildCount = 0;
                Node->Next       = (uint32_t)(Idx + 1);
            }

            gui_layout_node *Sentinel = GuiGetSentinelNode(Tree->NodeBuffer);
            Sentinel->Next  = 0;
            Sentinel->Index = GuiInvalidIndex;

            Result = Tree;
        }
    }

    return Result;
}

static void
GuiUpdateInput(uint32_t NodeIndex, gui_cached_style *Cached, gui_layout_tree *Tree)
{
    if(GuiIsValidLayoutTree(Tree) && Cached)
    {
        gui_layout_node *Node = GuiGetLayoutNode(Tree->NodeBuffer, NodeIndex);
        if(GuiIsValidLayoutNode(Node))
        {
            // Layout inputs
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

            // TODO: This is now wrong since we can't rely on C++ assignment stuff to set IsSet.
            // We have to have a new way. Perhaps simply checking for an empty color.

            // Paint properties
            gui_paint_properties *Paint = &Tree->PaintBuffer[NodeIndex];

            Paint->Color               = Cached->Default.Color.Value;
            Paint->BorderColor         = Cached->Default.BorderColor.Value;
            Paint->BorderWidth         = Cached->Default.BorderWidth.Value;
            Paint->TextColor           = Cached->Default.TextColor.Value;
            Paint->CornerRadius        = Cached->Default.CornerRadius.Value;
            Paint->Softness            = Cached->Default.Softness.Value;

            Paint->HoveredColor        = Cached->Hovered.Color.IsSet ? Cached->Hovered.Color.Value : Paint->Color;
            Paint->HoveredBorderColor  = Cached->Hovered.BorderColor.IsSet ? Cached->Hovered.BorderColor.Value : Paint->BorderColor;
            Paint->HoveredBorderWidth  = Cached->Hovered.BorderWidth.IsSet ? Cached->Hovered.BorderWidth.Value : Paint->BorderWidth;
            Paint->HoveredTextColor    = Cached->Hovered.TextColor.IsSet ? Cached->Hovered.TextColor.Value : Paint->TextColor;
            Paint->HoveredCornerRadius = Cached->Hovered.CornerRadius.IsSet ? Cached->Hovered.CornerRadius.Value : Paint->CornerRadius;
            Paint->HoveredSoftness     = Cached->Hovered.Softness.IsSet ? Cached->Hovered.Softness.Value : Paint->Softness;

            Paint->FocusedColor        = Cached->Focused.Color.IsSet ? Cached->Focused.Color.Value : Paint->Color;
            Paint->FocusedBorderColor  = Cached->Focused.BorderColor.IsSet ? Cached->Focused.BorderColor.Value : Paint->BorderColor;
            Paint->FocusedBorderWidth  = Cached->Focused.BorderWidth.IsSet ? Cached->Focused.BorderWidth.Value : Paint->BorderWidth;
            Paint->FocusedTextColor    = Cached->Focused.TextColor.IsSet ? Cached->Focused.TextColor.Value : Paint->TextColor;
            Paint->FocusedCornerRadius = Cached->Focused.CornerRadius.IsSet ? Cached->Focused.CornerRadius.Value : Paint->CornerRadius;
            Paint->FocusedSoftness     = Cached->Focused.Softness.IsSet ? Cached->Focused.Softness.Value : Paint->Softness;

            Paint->CaretColor          = Cached->Focused.CaretColor.Value;
            Paint->CaretWidth          = Cached->Focused.CaretWidth.Value;
        }
    }
}

static gui_bounding_box
GuiGetLayoutNodeBoundingBox(gui_layout_node *Node)
{
    gui_bounding_box Result = { .Left = 0, .Top = 0, .Right = 0, .Bottom = 0 };
    if(GuiIsValidLayoutNode(Node))
    {
        gui_point Screen = {.X = Node->OutputPosition.X + Node->ScrollOffset.Width, .Y = Node->OutputPosition.Y + Node->ScrollOffset.Height};
        Result = (gui_bounding_box){.Left = Screen.X, .Top = Screen.Y, .Right = Screen.X + Node->OutputSize.Width, .Bottom = Screen.Y + Node->OutputSize.Height};
    }
    return Result;
}

static gui_bounding_box
GuiGetLayoutNodeInnerBoundingBox(gui_layout_node *Node)
{
    // For now identical to outer bounding box; placeholder for padding clipping
    gui_bounding_box Result = { .Left = 0, .Top = 0, .Right = 0, .Bottom = 0 };
    if(GuiIsValidLayoutNode(Node))
    {
        gui_point Screen = {.X = Node->OutputPosition.X + Node->ScrollOffset.Width, .Y = Node->OutputPosition.Y + Node->ScrollOffset.Height};
        Result = (gui_bounding_box){ .Left = Screen.X, .Top = Screen.Y, .Right = Screen.X + Node->OutputSize.Width, .Bottom = Screen.Y + Node->OutputSize.Height };
    }
    return Result;
}

static gui_bounding_box
GuiGetLayoutNodeContentRect(gui_layout_node *Node)
{
    // Also placeholder for content rect computation
    gui_bounding_box Result = { .Left = 0, .Top = 0, .Right = 0, .Bottom = 0 };
    if(GuiIsValidLayoutNode(Node))
    {
        gui_point Screen = {.X = Node->OutputPosition.X + Node->ScrollOffset.Width, .Y = Node->OutputPosition.Y + Node->ScrollOffset.Height};
        Result = (gui_bounding_box){.Left = Screen.X, .Top = Screen.Y, .Right = Screen.X + Node->OutputSize.Width, .Bottom = Screen.Y + Node->OutputSize.Height};
    }
    return Result;
}

static uint32_t
GuiCreateNode(uint64_t Key, uint32_t Flags, gui_layout_tree *Tree)
{
    uint32_t Result = (uint32_t)GuiInvalidIndex;

    if(GuiIsValidLayoutTree(Tree))
    {
        uint32_t FoundIndex = (uint32_t)GuiInvalidIndex;
        if(GuiKeyMap_Find(Key, &FoundIndex))
        {
            // existing
        }
        else
        {
            gui_layout_node *Node = GuiGetFreeLayoutNode(&Tree->NodeBuffer);
            if(Node)
            {
                GuiKeyMap_Insert(Key, Node->Index);
                FoundIndex = Node->Index;
            }
        }

        gui_layout_node *Node = GuiGetLayoutNode(Tree->NodeBuffer, FoundIndex);
        if(GuiIsValidLayoutNode(Node))
        {
            Node->ChildCount = 0;
            Node->Flags      = Flags;

            uint32_t ParentIndex = (Tree->Parent) ? Tree->Parent->Value : (uint32_t)GuiInvalidIndex;
            GuiAppendLayoutNode(Tree->NodeBuffer, ParentIndex, Node->Index);

            Result = Node->Index;
        }
    }

    return Result;
}

static void
GuiPushParent(uint32_t NodeIndex, gui_layout_tree *Tree, gui_parent_node *ParentNode)
{
    if(GuiIsValidLayoutTree(Tree) && ParentNode)
    {
        ParentNode->Prev  = Tree->Parent;
        ParentNode->Value = NodeIndex;

        Tree->Parent = ParentNode;
    }
}

static void
GuiPopParent(uint32_t NodeIndex, gui_layout_tree *Tree)
{
    if(GuiIsValidLayoutTree(Tree) && Tree->Parent)
    {
        Tree->Parent = Tree->Parent->Prev;
    }
}

static uint32_t
GuiFindChild(uint32_t NodeIndex, uint32_t FindIndex, gui_layout_tree *Tree)
{
    uint32_t Result = (uint32_t)GuiInvalidIndex;

    if(GuiIsValidLayoutTree(Tree))
    {
        gui_layout_node *Node = GuiGetLayoutNode(Tree->NodeBuffer, NodeIndex);
        if(GuiIsValidLayoutNode(Node))
        {
            gui_layout_node *Child = GuiGetLayoutNode(Tree->NodeBuffer, Node->First);
            uint32_t Remaining = FindIndex;
            while(GuiIsValidLayoutNode(Child) && Remaining)
            {
                Child = GuiGetLayoutNode(Tree->NodeBuffer, Child->Next);
                --Remaining;
            }

            if(GuiIsValidLayoutNode(Child))
            {
                Result = Child->Index;
            }
        }
    }

    return Result;
}

static gui_bool
GuiAppendChild(uint32_t ParentIndex, uint32_t ChildIndex, gui_layout_tree *Tree)
{
    gui_bool Result = GUI_FALSE;
    if(GuiIsValidLayoutTree(Tree) && ParentIndex != ChildIndex)
    {
        GuiAppendLayoutNode(Tree->NodeBuffer, ParentIndex, ChildIndex);
        Result = GUI_TRUE;
    }
    return Result;
}

static void
GuiSetNodeOffset(uint32_t NodeIndex, gui_dimensions Offset, gui_layout_tree *Tree)
{
    if(GuiIsValidLayoutTree(Tree))
    {
        gui_layout_node *Node = GuiGetLayoutNode(Tree->NodeBuffer, NodeIndex);
        if(GuiIsValidLayoutNode(Node))
        {
            Node->VisualOffset = Offset;
        }
    }
}

// -----------------------------------------------------------------------------
// Scroll Region
// -----------------------------------------------------------------------------


typedef struct gui_scroll_region
{
    gui_dimensions ContentSize;
    float          ScrollOffset;
    float          PixelPerLine;
    Gui_AxisType   Axis;
} gui_scroll_region;


static void
GuiUpdateScrollNode(float ScrolledLines, gui_layout_node *Node, gui_layout_tree *Tree, gui_scroll_region *Region)
{
    float ScrolledPixels = ScrolledLines * Region->PixelPerLine;
    gui_dimensions WindowSize = Node->OutputSize;

    float ScrollLimit = 0.0f;
    if(Region->Axis == Gui_AxisType_X)
    {
        ScrollLimit = -(Region->ContentSize.Width - WindowSize.Width);
    }
    else if(Region->Axis == Gui_AxisType_Y)
    {
        ScrollLimit = -(Region->ContentSize.Height - WindowSize.Height);
    }

    Region->ScrollOffset += ScrolledPixels;
    // Clamp to [ScrollLimit, 0]
    if(Region->ScrollOffset < ScrollLimit) Region->ScrollOffset = ScrollLimit;
    if(Region->ScrollOffset > 0.0f) Region->ScrollOffset = 0.0f;

    gui_dimensions ScrollDelta = (gui_dimensions){ .Width = 0.0f, .Height = 0.0f };
    if(Region->Axis == Gui_AxisType_X)
    {
        ScrollDelta.Width = -1.0f * Region->ScrollOffset;
    }
    else if(Region->Axis == Gui_AxisType_Y)
    {
        ScrollDelta.Height = -1.0f * Region->ScrollOffset;
    }

    gui_bounding_box WindowContent = GuiGetLayoutNodeBoundingBox(Node);
    for(gui_layout_node *Child = GuiGetLayoutNode(Tree->NodeBuffer, Node->First); GuiIsValidLayoutNode(Child); Child = GuiGetLayoutNode(Tree->NodeBuffer, Child->Next))
    {
        Child->ScrollOffset = (gui_dimensions){ .Width = -ScrollDelta.Width, .Height = -ScrollDelta.Height };

        gui_dimensions FixedContentSize = Child->OutputSize;
        gui_bounding_box ChildContent = GuiGetLayoutNodeBoundingBox(Child);

        if(FixedContentSize.Width > 0.0f && FixedContentSize.Height > 0.0f)
        {
            if(GuiBoundingBoxesIntersect(WindowContent, ChildContent))
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
GuiGetScrollRegionFootprint(void)
{
    uint64_t Result = sizeof(gui_scroll_region);
    return Result;
}


static gui_scroll_region *
GuiPlaceScrollRegionInMemory(gui_scroll_region_params Params, void *Memory)
{
    gui_scroll_region *Result = NULL;
    if(Memory)
    {
        Result = (gui_scroll_region *)Memory;
        Result->ContentSize = (gui_dimensions){ .Width = 0.0f, .Height = 0.0f };
        Result->ScrollOffset = 0.0f;
        Result->PixelPerLine = Params.PixelPerLine;
        Result->Axis = Params.Axis;
    }
    return Result;
}


static gui_dimensions
GuiGetScrollNodeTranslation(gui_scroll_region *Region)
{
    gui_dimensions Result = (gui_dimensions){ .Width = 0.0f, .Height = 0.0f };
    if(Region->Axis == Gui_AxisType_X)
    {
        Result = (gui_dimensions){ .Width = Region->ScrollOffset, .Height = 0.0f };
    }
    else if(Region->Axis == Gui_AxisType_Y)
    {
        Result = (gui_dimensions){ .Width = 0.0f, .Height = Region->ScrollOffset };
    }
    return Result;
}


// -----------------------------------------------------------------------------
// Input Handling
// -----------------------------------------------------------------------------

static gui_bool GuiIsMouseInsideOuterBox(gui_point Position, gui_layout_node *Node)
{
    gui_bool Result = GUI_FALSE;
    if(GuiIsValidLayoutNode(Node))
    {
        gui_bounding_box Box = GuiGetLayoutNodeBoundingBox(Node);
        float Distance = GuiBoundingBoxSignedDistanceField(Position, Box);
        Result = Distance <= 0.0f;
    }
    return Result;
}

static gui_bool GuiIsPointInsideBorder(gui_point Position, gui_layout_node *Node)
{
    gui_bool Result = GUI_FALSE;
    if(GuiIsValidLayoutNode(Node))
    {
        gui_bounding_box Box = GuiGetLayoutNodeBoundingBox(Node);
        float Distance = GuiBoundingBoxSignedDistanceField(Position, Box);
        Result = Distance >= 0.0f;
    }
    return Result;
}

static gui_bool
GuiHandlePointerClick(gui_point Position, uint32_t ClickMask, uint32_t NodeIndex, gui_layout_tree *Tree)
{
    GUI_ASSERT(Position.X >= 0.0f && Position.Y >= 0.0f);
    GUI_ASSERT(GuiIsValidLayoutTree(Tree));

    if(GuiIsValidLayoutTree(Tree))
    {
        gui_layout_node *Node = GuiGetLayoutNode(Tree->NodeBuffer, NodeIndex);
        GUI_ASSERT(GuiIsValidLayoutNode(Node));

        if(GuiIsValidLayoutNode(Node))
        {
            if(GuiIsMouseInsideOuterBox(Position, Node))
            {
                for(gui_layout_node *Child = GuiGetLayoutNode(Tree->NodeBuffer, Node->First); GuiIsValidLayoutNode(Child); Child = GuiGetLayoutNode(Tree->NodeBuffer, Child->Next))
                {
                    gui_bool IsHandled = GuiHandlePointerClick(Position, ClickMask, Child->Index, Tree);
                    if(IsHandled) return GUI_TRUE;
                }

                Node->State = (Gui_NodeState)(Node->State | Gui_NodeState_IsClicked);
                Node->State = (Gui_NodeState)(Node->State | Gui_NodeState_UseFocusedStyle);
                Node->State = (Gui_NodeState)(Node->State | Gui_NodeState_HasCapturedPointer);

                Tree->CapturedNodeIndex = NodeIndex;
                return GUI_TRUE;
            }
        }
    }

    return GUI_FALSE;
}

static gui_bool
GuiHandlePointerRelease(gui_point Position, uint32_t ReleaseMask, uint32_t NodeIndex, gui_layout_tree *Tree)
{
    GUI_ASSERT(Position.X >= 0.0f && Position.Y >= 0.0f);
    GUI_ASSERT(GuiIsValidLayoutTree(Tree));

    if(GuiIsValidLayoutTree(Tree))
    {
        gui_layout_node *Node = GuiGetLayoutNode(Tree->NodeBuffer, NodeIndex);
        GUI_ASSERT(GuiIsValidLayoutNode(Node));

        for(gui_layout_node *Child = GuiGetLayoutNode(Tree->NodeBuffer, Node->First); GuiIsValidLayoutNode(Child); Child = GuiGetLayoutNode(Tree->NodeBuffer, Child->Next))
        {
            if(GuiHandlePointerRelease(Position, ReleaseMask, Child->Index, Tree)) return GUI_TRUE;
        }

        if((Node->State & Gui_NodeState_HasCapturedPointer) != Gui_NodeState_None)
        {
            Node->State = (Gui_NodeState)(Node->State & ~(Gui_NodeState_HasCapturedPointer | Gui_NodeState_UseFocusedStyle));
            Tree->CapturedNodeIndex = GuiInvalidIndex;
            return GUI_TRUE;
        }
    }

    return GUI_FALSE;
}

static gui_bool
GuiHandlePointerHover(gui_point Position, uint32_t NodeIndex, gui_layout_tree *Tree)
{
    GUI_ASSERT(Position.X >= 0.0f && Position.Y >= 0.0f);

    if(GuiIsValidLayoutTree(Tree))
    {
        gui_layout_node *Node = GuiGetLayoutNode(Tree->NodeBuffer, NodeIndex);
        GUI_ASSERT(GuiIsValidLayoutNode(Node));

        if(GuiIsMouseInsideOuterBox(Position, Node))
        {
            for(gui_layout_node *Child = GuiGetLayoutNode(Tree->NodeBuffer, Node->First); GuiIsValidLayoutNode(Child); Child = GuiGetLayoutNode(Tree->NodeBuffer, Child->Next))
            {
                gui_bool IsHandled = GuiHandlePointerHover(Position, Child->Index, Tree);
                if(IsHandled) return GUI_TRUE;
            }

            Node->State = Node->State | Gui_NodeState_UseHoveredStyle;
            return GUI_TRUE;
        }
    }

    return GUI_FALSE;
}

static void
GuiHandlePointerMove(float DeltaX, float DeltaY, gui_layout_tree *Tree)
{
    GUI_ASSERT(GuiIsValidLayoutTree(Tree));

    if(GuiIsValidLayoutTree(Tree))
    {
        gui_layout_node *CapturedNode = GuiGetLayoutNode(Tree->NodeBuffer, (uint64_t)Tree->CapturedNodeIndex);
        if(GuiIsValidLayoutNode(CapturedNode))
        {
            if((CapturedNode->Flags & Gui_NodeFlags_IsDraggable) != Gui_NodeFlags_None)
            {
                CapturedNode->OutputPosition.X += DeltaX;
                CapturedNode->OutputPosition.Y += DeltaY;
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Layout computation helpers
// -----------------------------------------------------------------------------

static float
GuiGetAlignmentOffset(Gui_Alignment AlignmentType, float FreeSpace)
{
    float Result = 0.0f;
    if(FreeSpace > 0.0f)
    {
        switch(AlignmentType)
        {
            case Gui_Alignment_None:
            case Gui_Alignment_Start:
            {
                // No-Op
            } break;

            case Gui_Alignment_Center:
            {
                Result = 0.5f * FreeSpace;
            } break;

            case Gui_Alignment_End:
            {
                // TODO: implement end alignment
                Result = FreeSpace; // fallback
            } break;
        }
    }
    return Result;
}

static float
GuiComputeNodeSize(gui_sizing Sizing, float ParentSize)
{
    float Result = 0.0f;
    switch(Sizing.Type)
    {
        case Gui_LayoutSizing_None:
        {
            Result = 0.0f;
        } break;

        case Gui_LayoutSizing_Fixed:
        {
            Result = Sizing.Value;
        } break;

        case Gui_LayoutSizing_Percent:
        {
            GUI_ASSERT(Sizing.Value >= 0.0f && Sizing.Value <= 100.0f);
            Result = (Sizing.Value / 100.0f) * ParentSize;
        } break;

        case Gui_LayoutSizing_Fit:
        {
            // TODO: implement fit
            Result = 0.0f;
        } break;
    }
    return Result;
}

static void
GuiComputeLayout(gui_layout_node *Node, gui_layout_tree *Tree, gui_dimensions ParentBounds, gui_resource_table *ResourceTable, gui_bool *Changed)
{
    GUI_ASSERT(GuiIsValidLayoutNode(Node));
    GUI_ASSERT(GuiIsValidLayoutTree(Tree));

    Node->OutputChildSize = (gui_dimensions){ .Width = 0.0f, .Height = 0.0f };
    gui_dimensions LastSize = Node->OutputSize;

    float Width     = GuiComputeNodeSize(Node->Size.Width, ParentBounds.Width);
    float MinWidth  = GuiComputeNodeSize(Node->MinSize.Width, ParentBounds.Width);
    float MaxWidth  = GuiComputeNodeSize(Node->MaxSize.Width, ParentBounds.Width);
    float Height    = GuiComputeNodeSize(Node->Size.Height, ParentBounds.Height);
    float MinHeight = GuiComputeNodeSize(Node->MinSize.Height, ParentBounds.Height);
    float MaxHeight = GuiComputeNodeSize(Node->MaxSize.Height, ParentBounds.Height);

    Node->OutputSize = (gui_dimensions){ .Width = fmaxf(MinWidth, fminf(Width, MaxWidth)), .Height = fmaxf(MinHeight, fminf(Height, MaxHeight)) };

    if((Node->OutputSize.Width != LastSize.Width) || (Node->OutputSize.Height != LastSize.Height))
    {
        if(Changed) *Changed = GUI_TRUE;
    }

    gui_dimensions ContentBounds = (gui_dimensions){ .Width = Node->OutputSize.Width - (Node->Padding.Left + Node->Padding.Right), .Height = Node->OutputSize.Height - (Node->Padding.Top + Node->Padding.Bot) };

    if(Node->ChildCount > 0)
    {
        float Spacing = Node->Spacing * (float)(Node->ChildCount - 1);
        if(Node->Direction == Gui_LayoutDirection_Horizontal)
        {
            ContentBounds.Width -= Spacing;
        }
        else if(Node->Direction == Gui_LayoutDirection_Vertical)
        {
            ContentBounds.Height -= Spacing;
        }
    }

    GUI_ASSERT(ContentBounds.Width >= 0.0f && ContentBounds.Height >= 0.0f);

    for(gui_layout_node *Child = GuiGetLayoutNode(Tree->NodeBuffer, Node->First); GuiIsValidLayoutNode(Child); Child = GuiGetLayoutNode(Tree->NodeBuffer, Child->Next))
    {
        GuiComputeLayout(Child, Tree, ContentBounds, ResourceTable, Changed);
        Node->OutputChildSize.Width += Child->OutputSize.Width;
        Node->OutputChildSize.Height += Child->OutputSize.Height;
    }
}

static void
GuiPlaceLayout(gui_layout_node *Node, gui_layout_tree *Tree, gui_resource_table *ResourceTable)
{
    gui_point Cursor = (gui_point){ .X = Node->OutputPosition.X + Node->Padding.Left, .Y = Node->OutputPosition.Y + Node->Padding.Top };
    gui_bool IsXMajor = (Node->Direction == Gui_LayoutDirection_Horizontal) ? GUI_TRUE : GUI_FALSE;

    float MajorSize = IsXMajor ? Node->OutputSize.Width - (Node->Padding.Left + Node->Padding.Right) : Node->OutputSize.Height - (Node->Padding.Top + Node->Padding.Bot);
    float MinorSize = IsXMajor ? Node->OutputSize.Height - (Node->Padding.Top + Node->Padding.Bot) : Node->OutputSize.Width - (Node->Padding.Left + Node->Padding.Right);

    Gui_Alignment MajorAlignment = IsXMajor ? Node->XAlign : Node->YAlign;
    float MajorChildrenSize = IsXMajor ? Node->OutputChildSize.Width : Node->OutputChildSize.Height;
    float MajorOffset = GuiGetAlignmentOffset(MajorAlignment, MajorSize - MajorChildrenSize);

    if(IsXMajor)
    {
        Cursor.X += MajorOffset;
    }
    else
    {
        Cursor.Y += MajorOffset;
    }

    for(gui_layout_node *Child = GuiGetLayoutNode(Tree->NodeBuffer, Node->First); GuiIsValidLayoutNode(Child); Child = GuiGetLayoutNode(Tree->NodeBuffer, Child->Next))
    {
        Child->OutputPosition = Cursor;
        Gui_Alignment MinorAlign = IsXMajor ? Node->YAlign : Node->XAlign;
        float MinorOffset = GuiGetAlignmentOffset(MinorAlign, IsXMajor ? (MinorSize - Child->OutputSize.Height) : (MinorSize - Child->OutputSize.Width));

        if(IsXMajor)
        {
            Child->OutputPosition.Y += MinorOffset + Child->VisualOffset.Height;
            Cursor.X += Child->OutputSize.Width + Node->Spacing;
        }
        else
        {
            Child->OutputPosition.X += MinorOffset + Child->VisualOffset.Width;
            Cursor.Y += Child->OutputSize.Height + Node->Spacing;
        }

        GuiPlaceLayout(Child, Tree, ResourceTable);
    }
}

static void
GuiComputeTreeLayout(gui_layout_tree *Tree)
{
    if(GuiIsValidLayoutTree(Tree))
    {
        gui_context     *Context    = GuiGetContext();
        gui_layout_node *ActiveRoot = GuiGetLayoutNode(Tree->NodeBuffer, (uint64_t)Tree->RootIndex);

        while(GUI_TRUE)
        {
            gui_bool Changed = GUI_FALSE;
            GuiComputeLayout(ActiveRoot, Tree, ActiveRoot->OutputSize, (gui_resource_table *)Context->ResourceTable, &Changed);
            if(!Changed) break;
        }

        GuiPlaceLayout(ActiveRoot, Tree, (gui_resource_table *)Context->ResourceTable);
    }
}
