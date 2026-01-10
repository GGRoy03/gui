// =============================================================================
// DOMAIN: Strings
// =============================================================================


static gui_byte_string 
GuiByteString(char *String, uint64_t Size)
{
    gui_byte_string Result = { String, Size };
    return Result;
}


static gui_bool
GuiIsValidByteString(gui_byte_string Input)
{
    gui_bool Result = (Input.String) && (Input.Size);
    return Result;
}


static uint64_t
GuiHashByteString(gui_byte_string Input)
{
    uint64_t Result = XXH3_64bits(Input.String, Input.Size);
    return Result;
}


// =============================================================================
// DOMAIN: Resource Cache
// =============================================================================


typedef struct gui_resource_allocator
{
    uint64_t AllocatedCount;
    uint64_t AllocatedBytes;
} gui_resource_allocator;


typedef struct gui_resource_entry
{
    gui_resource_key Key;

    uint32_t         NextWithSameHashSlot;
    uint32_t         NextLRU;
    uint32_t         PrevLRU;

    Gui_ResourceType ResourceType;
    void            *Memory;
} gui_resource_entry;


typedef struct gui_resource_table
{
    gui_resource_stats     Stats;
    gui_resource_allocator Allocator;

    uint32_t               HashMask;
    uint32_t               HashSlotCount;
    uint32_t               EntryCount;

    uint32_t              *HashTable;
    gui_resource_entry    *Entries;
} gui_resource_table;


static gui_resource_entry *
GuiGetResourceSentinel(gui_resource_table *Table)
{
    gui_resource_entry *Result = Table->Entries;
    return Result;
}


static uint32_t *
GuiGetResourceSlotPointer(gui_resource_key Key, gui_resource_table *Table)
{
    uint32_t HashIndex = _mm_cvtsi128_si32(Key.Value);
    uint32_t HashSlot  = (HashIndex & Table->HashMask);

    GUI_ASSERT(HashSlot < Table->HashSlotCount);

    uint32_t *Result = &Table->HashTable[HashSlot];
    return Result;
}


static gui_resource_entry *
GuiGetResourceEntry(uint32_t Index, gui_resource_table *Table)
{
    GUI_ASSERT(Index < Table->EntryCount);

    gui_resource_entry *Result = Table->Entries + Index;
    return Result;
}


static gui_bool
GuiResourceKeyAreEqual(gui_resource_key A, gui_resource_key B)
{
    __m128i Compare = _mm_cmpeq_epi32(A.Value, B.Value);
    gui_bool     Result  = (_mm_movemask_epi8(Compare) == 0xffff);

    return Result;
}


static uint32_t
GuiPopFreeResourceEntry(gui_resource_table *Table)
{
    gui_resource_entry *Sentinel = GuiGetResourceSentinel(Table);

    // At initialization we populate sentinel's the hash chain such that:
    // (Sentinel) -> (Slot) -> (Slot) -> (Slot)
    // If (Sentinel) -> (Nothing), then we have no more slots available.

    if(!Sentinel->NextWithSameHashSlot)
    {
        GUI_ASSERT(!"Not Implemented");
    }

    uint32_t Result = Sentinel->NextWithSameHashSlot;

    gui_resource_entry *Entry = GuiGetResourceEntry(Result, Table);
    Sentinel->NextWithSameHashSlot = Entry->NextWithSameHashSlot;
    Entry->NextWithSameHashSlot    = 0;

    return Result;
}

static Gui_ResourceType
GuiGetResourceTypeFromKey(gui_resource_key Key)
{
    uint64_t         High = _mm_extract_epi64(Key.Value, 1);
    Gui_ResourceType Type = (Gui_ResourceType)(High >> 32);
    return Type;
}

// TODO: Make a better resource allocator.
static void *
GuiAllocateUIResource(uint64_t Size, gui_resource_allocator *Allocator)
{
    return 0;
}


static gui_memory_footprint
GuiGetResourceTableFootprint(gui_resource_table_params Params)
{
    uint64_t HashTableSize  = Params.HashSlotCount * sizeof(uint32_t);
    uint64_t EntryArraySize = Params.EntryCount    * sizeof(gui_resource_entry);

    gui_memory_footprint Result = 
    {
        .SizeInBytes = sizeof(gui_resource_table) + HashTableSize + EntryArraySize,
        .Alignment   = AlignOf(gui_resource_entry),
    };

    return Result;
}


static gui_resource_table *
GuiPlaceResourceTableInMemory(gui_resource_table_params Params, gui_memory_block Block)
{
    // Since this is user facing maybe we want hard-validation??
    GUI_ASSERT(GUI_ISPOWEROFTWO(Params.HashSlotCount));

    gui_resource_table *Result = 0;
    gui_memory_region   Local  = GuiEnterMemoryRegion(Block);

    if(GuiIsValidMemoryRegion(&Local))
    {
        gui_resource_entry *Entries   = GuiPushArray(&Local, gui_resource_entry, Params.EntryCount);
        uint32_t           *HashTable = GuiPushArray(&Local, uint32_t, Params.HashSlotCount);
        gui_resource_table *Table     = GuiPushStruct(&Local, gui_resource_table);

        if(Entries && HashTable && Table)
        {
            Table->HashTable     = HashTable;
            Table->Entries       = Entries;
            Table->EntryCount    = Params.EntryCount;
            Table->HashSlotCount = Params.HashSlotCount;
            Table->HashMask      = Params.HashSlotCount - 1;

            for(uint32_t Idx = 0; Idx < Params.EntryCount; ++Idx)
            {
                gui_resource_entry *Entry = GuiGetResourceEntry(Idx, Table);
                if((Idx + 1) < Params.EntryCount)
                {
                    Entry->NextWithSameHashSlot = Idx + 1;
                }
                else
                {
                    Entry->NextWithSameHashSlot = 0;
                }
    
                Entry->ResourceType = Gui_ResourceType_None;
                Entry->Memory       = 0;
            }

            Result = Table;
        }
    }

    return Result;
}

static gui_resource_key
GuiMakeNodeResourceKey(Gui_ResourceType Type, uint32_t NodeIndex, gui_layout_tree *Tree)
{
    uint64_t Low  = (uint64_t)Tree;
    uint64_t High = ((uint64_t)Type << 32) | NodeIndex;

    gui_resource_key Key = {.Value = _mm_set_epi64x(High, Low)};
    return Key;
}

static gui_resource_state
GuiFindResourceByKey(gui_resource_key Key, Gui_FindResourceFlag Flags, gui_resource_table *Table)
{
    gui_resource_state Result = {};

    if (Table)
    {
        gui_resource_entry *FoundEntry = 0;

        uint32_t *Slot       = GuiGetResourceSlotPointer(Key, Table);
        uint32_t  EntryIndex = Slot[0];
        
        while(EntryIndex)
        {
            gui_resource_entry *Entry = GuiGetResourceEntry(EntryIndex, Table);
            if(GuiResourceKeyAreEqual(Entry->Key, Key))
            {
                FoundEntry = Entry;
                break;
            }
        
            EntryIndex = Entry->NextWithSameHashSlot;
        }
        
        if(FoundEntry)
        {
            // If we hit an already existing entry we must pop it off the LRU chain.
            // (Prev) -> (Entry) -> (Next)
            // (Prev) -> (Next)
        
            gui_resource_entry *Prev = GuiGetResourceEntry(FoundEntry->PrevLRU, Table);
            gui_resource_entry *Next = GuiGetResourceEntry(FoundEntry->NextLRU, Table);
        
            Prev->NextLRU = FoundEntry->NextLRU;
            Next->PrevLRU = FoundEntry->PrevLRU;
        
            ++Table->Stats.CacheHitCount;
        }
        else if((Flags & Gui_FindResourceFlag_AddIfNotFound) != Gui_FindResourceFlag_None)
        {
            // If we miss an entry we have to first allocate a new one.
            // If we have some hash slot at X: Hash[X]
            // Hash[X] -> (Index) -> (Index)
            // (Entry) -> Hash[X] -> (Index) -> (Index)
            // Hash[X] -> (Index) -> (Index) -> (Index)
        
            EntryIndex = GuiPopFreeResourceEntry(Table);
            GUI_ASSERT(EntryIndex);
        
            FoundEntry = GuiGetResourceEntry(EntryIndex, Table);
            FoundEntry->NextWithSameHashSlot = Slot[0];
            FoundEntry->Key                  = Key;
        
            Slot[0] = EntryIndex;
        
            ++Table->Stats.CacheMissCount;
        }
        
        if(FoundEntry)
        {
            // If we find an entry we must assure that this new entry is now
            // the most recent one in the LRU chain.
            // What we have: (Sentinel) -> (Entry)    -> (Entry) -> (Entry)
            // What we want: (Sentinel) -> (NewEntry) -> (Entry) -> (Entry) -> (Entry)
        
            gui_resource_entry *Sentinel = GuiGetResourceSentinel(Table);
            FoundEntry->NextLRU = Sentinel->NextLRU;
            FoundEntry->PrevLRU = 0;
        
            gui_resource_entry *NextLRU = GuiGetResourceEntry(Sentinel->NextLRU, Table);
            NextLRU->PrevLRU  = EntryIndex;
            Sentinel->NextLRU = EntryIndex;
        }
        
        Result.Id           = EntryIndex;
        Result.ResourceType = FoundEntry ? FoundEntry->ResourceType : Gui_ResourceType_None;
        Result.Resource     = FoundEntry ? FoundEntry->Memory : 0;
    }

    return Result;
}


static void
GuiUpdateResourceTable(uint32_t Id, gui_resource_key Key, void *Memory, gui_resource_table *Table)
{
    gui_resource_entry *Entry = GuiGetResourceEntry(Id, Table);
    GUI_ASSERT(Entry);

    // This is weird. Kind of.
    if(Entry->Memory && Entry->Memory != Memory)
    {
        GUI_ASSERT(!"...");
        // OSRelease(Entry->Memory);
    }

    Entry->Key          = Key;
    Entry->Memory       = Memory;
    Entry->ResourceType = GuiGetResourceTypeFromKey(Key);

    GUI_ASSERT(Entry->ResourceType != Gui_ResourceType_None);
}


static void *
GuiQueryNodeResource(Gui_ResourceType Type, uint32_t NodeIndex, gui_layout_tree *Tree, gui_resource_table *Table)
{
    gui_resource_key   Key   = GuiMakeNodeResourceKey(Type, NodeIndex, Tree);
    gui_resource_state State = GuiFindResourceByKey(Key, Gui_FindResourceFlag_None, Table);

    void *Result = State.Resource;
    return Result;
}


// ----------------------------------------------------------------------------------
// Context Public API Implementation

typedef struct gui_pointer_state
{
    uint32_t          Id;
    gui_point         Position;
    gui_point         LastPosition;
    uint32_t          ButtonMask;

    // Targets?
    gui_bool     IsCaptured; // This might be, uhhh, a state flag with some other states.
} gui_pointer_state;


static void
GuiClearPointerEvents(gui_pointer_event_list *List)
{
    if(List)
    {
        List->First = 0;
        List->Last  = 0;
        List->Count = 0;
    }
}


static gui_bool
GuiPushPointerEvent(Gui_PointerEvent Type, gui_pointer_event_node *Node, gui_pointer_event_list *List)
{
    gui_bool Pushed = GUI_FALSE;

    if(List && Node && Type != Gui_PointerEvent_None)
    {
        Node->Next = 0;
        Node->Prev = 0;
        Node->Value.Type = Type;

        AppendToDoublyLinkedList(List, Node, List->Count);

        Pushed = GUI_TRUE;
    }

    return Pushed;
}


static gui_bool
GuiPushPointerMoveEvent(gui_point Position, gui_point LastPosition, gui_pointer_event_node *Node, gui_pointer_event_list *List)
{
    gui_bool Pushed = GUI_FALSE;

    if(Node)
    {
        Node->Value.Position = Position;
        Node->Value.Delta    = GuiTranslationFromPoints(Position, LastPosition);

        Pushed = GuiPushPointerEvent(Gui_PointerEvent_Move, Node, List);
    }

    return Pushed;
}


static gui_bool
GuiPushPointerClickEvent(Gui_PointerButton Button, gui_point Position, gui_pointer_event_node *Node, gui_pointer_event_list *List)
{
    gui_bool Pushed = GUI_FALSE;

    if(Node)
    {
        Node->Value.ButtonMask = Button;
        Node->Value.Position   = Position;

        Pushed = GuiPushPointerEvent(Gui_PointerEvent_Click, Node, List);
    }

    return Pushed;
}


static gui_bool
GuiPushPointerReleaseEvent(Gui_PointerButton Button, gui_point Position, gui_pointer_event_node *Node, gui_pointer_event_list *List)
{
    gui_bool Pushed = GUI_FALSE;

    if(Node)
    {
        Node->Value.ButtonMask = Button;
        Node->Value.Position   = Position;

        Pushed = GuiPushPointerEvent(Gui_PointerEvent_Release, Node, List);
    }

    return Pushed;
}


// We take this tree parameter for simplicity while we refactor this part

static void
GuiBeginFrame(float Width, float Height, gui_pointer_event_list *EventList, gui_layout_tree *Tree)
{
    gui_context *Context = GuiGetContext();

    // Obviously this is temporary. Unsure how I want to structure this yet.
    static gui_pointer_state PointerStates[1];

    for(gui_pointer_event_node *EventNode = EventList->First; EventNode != 0; EventNode = EventNode->Next)
    {
        gui_pointer_event Event = EventNode->Value;

        switch(Event.Type)
        {

        case Gui_PointerEvent_Move:
        {
            gui_pointer_state *State = &PointerStates[0];
            State->LastPosition = State->Position;
            State->Position     = Event.Position;

            if(State->IsCaptured)
            {
                float DeltaX = Event.Delta.X;
                float DeltaY = Event.Delta.Y;
                GuiHandlePointerMove(DeltaX, DeltaY, Tree);
            }
        } break;

        case Gui_PointerEvent_Click:
        {
            gui_pointer_state *State = &PointerStates[0];
            State->ButtonMask |= Event.ButtonMask;
            State->IsCaptured  = GuiHandlePointerClick(State->Position, State->ButtonMask, 0, Tree);
        } break;

        case Gui_PointerEvent_Release:
        {
            gui_pointer_state *State = &PointerStates[0];
            State->ButtonMask &= ~Event.ButtonMask;

            if(State->IsCaptured)
            {
                GuiHandlePointerRelease(State->Position, State->ButtonMask, 0, Tree);
                State->IsCaptured = GUI_FALSE;
            }
        } break;

        default:
        {
            GUI_ASSERT(!"Unknown Event Type");
        } break;

        }
    }

    // If the ButtonMask is 0 then it means the pointer is in a hover state.
    // We look for that hover target in any of the pipelines.

    for(int32_t PointerIdx = 0; PointerIdx < 1; ++PointerIdx)
    {
        gui_pointer_state State = PointerStates[PointerIdx];

        if(State.ButtonMask == Gui_PointerButton_None) 
        {
            GuiHandlePointerHover(State.Position, 0, Tree);
        }
    }

    Context->Width  = Width;
    Context->Height = Height;
}


static void
GuiEndFrame(void)
{
    // TODO: Unsure
}


static gui_context *
GuiGetContext(void)
{
    return &GlobalVoidContext;
}


static void
GuiCreateContext(void)
{
    // void_context &Context = GetVoidContext();

    // State
    {
        // gui_resource_table_params TableParams =
        //{
        //    .HashSlotCount = 512,
        //    .EntryCount    = 2048,
        //};

        //uint64_t TableFootprint = GetResourceTableFootprint(TableParams);
        //void    *TableMemory    = PushArena(Context.StateArena, TableFootprint, 0);

        //Context.ResourceTable =  PlaceResourceTableInMemory(TableParams, TableMemory);

        //GUI_ASSERT(Context.ResourceTable);
    }
}

// -------------------
// Component stuff


static gui_component
GuiCreateComponent(gui_byte_string Name, uint32_t Flags, gui_cached_style *Style, gui_layout_tree *Tree)
{
    gui_component Component = 
    {
        .LayoutIndex = GuiCreateNode(GuiHashByteString(Name), Flags, Tree),
        .LayoutTree  = Tree,
    };

    GuiUpdateInput(Component.LayoutIndex, Style, Tree);

    return Component;
}


static void
GuiSetStyle(gui_component *Component, gui_cached_style *Style)
{
    GuiUpdateInput(Component->LayoutIndex, Style, Component->LayoutTree);
}


static gui_bool
GuiPushComponent(gui_component *Component, gui_parent_node *Node)
{
    gui_bool Result = Component && Node;

    if(Result)
    {
        GuiPushParent(Component->LayoutIndex, Component->LayoutTree, Node);
    }

    return Result;
}


static gui_bool
GuiPopComponent(gui_component *Component)
{
    GuiPopParent(Component->LayoutIndex, Component->LayoutTree);
    return GUI_TRUE;
}
