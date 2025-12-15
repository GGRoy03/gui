// -----------------------------------------------------------------------------------
// @Internal: Pointer Events Helpers

static pointer_event_node *
EnqueuePointerEvent(PointerEvent Type, memory_arena *Arena, pointer_event_list &List)
{
    VOID_ASSERT(Type != PointerEvent::None);  // Internal Corruption
    // VOID_ASSERT(Arena);                       // Intennal Corruption

    auto *Node = static_cast<pointer_event_node *>(malloc(sizeof(pointer_event_node)));
    if(Node)
    {
        Node->Next             = 0;
        Node->Value.Type       = Type;

        AppendToDoublyLinkedList((&List), Node, List.Count);
    }

    return Node;
}

// -----------------------------------------------------------------------------------
// @Private: Pointer Events Helpers

static void
EnqueuePointerMoveEvent(vec2_float Position, vec2_float Delta, memory_arena *Arena, pointer_event_list &List)
{
    pointer_event_node *Node = EnqueuePointerEvent(PointerEvent::Move, Arena, List);
    if(Node)
    {
        Node->Value.Position = Position;
        Node->Value.Delta    = Delta;
    }
}

static void
EnqueuePointerClickEvent(uint32_t Button, vec2_float Position, memory_arena *Arena, pointer_event_list &List)
{
    pointer_event_node *Node = EnqueuePointerEvent(PointerEvent::Click, Arena, List);
    if(Node)
    {
        Node->Value.ButtonMask = Button;
        Node->Value.Position   = Position;
    }
}

static void
EnqueuePointerReleaseEvent(uint32_t Button, vec2_float Position, memory_arena *Arena, pointer_event_list &List)
{
    pointer_event_node *Node = EnqueuePointerEvent(PointerEvent::Release, Arena, List);
    if(Node)
    {
        Node->Value.ButtonMask = Button;
        Node->Value.Position   = Position;
    }
}

// -----------------------------------------------------------------------------------
// Inputs Private Implementation

static bool
IsButtonStateClicked(os_button_state *State)
{
    bool Result = (State->EndedDown && State->HalfTransitionCount > 0);
    return Result;
}

static bool
IsKeyClicked(OSInputKey_Type Key)
{
    VOID_ASSERT(Key > OSInputKey_None && Key < OSInputKey_Count);

    os_inputs *Inputs = OSGetInputs();
    VOID_ASSERT(Inputs);

    bool Result = IsButtonStateClicked(&Inputs->KeyboardButtons[Key]);
    return Result;
}

// [Inputs]

static void 
ProcessInputMessage(os_button_state *NewState, bool IsDown)
{
    if(NewState->EndedDown != IsDown)
    {
        NewState->EndedDown            = IsDown;
        NewState->HalfTransitionCount += 1;
    }
}

static float
OSGetScrollDelta(void)
{
    os_inputs *Inputs = OSGetInputs();
    float      Result = Inputs->ScrollDeltaInLines;

    return Result;
}

static void
OSClearInputs(os_inputs *Inputs)
{
    Inputs->ScrollDeltaInLines = 0.f;

    for (uint32_t Idx = 0; Idx < OS_KeyboardButtonCount; Idx++)
    {
        Inputs->KeyboardButtons[Idx].HalfTransitionCount = 0;
    }

    Inputs->PointerEventList.First = 0;
    Inputs->PointerEventList.Last  = 0;
    Inputs->PointerEventList.Count = 0;
    Inputs->Pointers[0].Delta      = vec2_float(0.f, 0.f);
}

// [Misc]

static bool
OSIsValidHandle(os_handle Handle)
{
    bool Result = (Handle.uint64_t[0] != 0);
    return Result;
}
