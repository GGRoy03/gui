namespace core
{

// ----------------------------------------------------------------------------------
// UI Resource Cache Private Implementation

struct resource_allocator
{
    uint64_t AllocatedCount;
    uint64_t AllocatedBytes;
};

struct resource_entry
{
    resource_key Key;

    uint32_t        NextWithSameHashSlot;
    uint32_t        NextLRU;
    uint32_t        PrevLRU;

    ResourceType ResourceType;
    void           *Memory;
};

struct resource_table
{
    resource_stats     Stats;
    resource_allocator Allocator;

    uint32_t              HashMask;
    uint32_t              HashSlotCount;
    uint32_t              EntryCount;

    uint32_t             *HashTable;
    resource_entry    *Entries;
};

static resource_entry *
GetResourceSentinel(resource_table *Table)
{
    resource_entry *Result = Table->Entries;
    return Result;
}

static uint32_t *
GetResourceSlotPointer(resource_key Key, resource_table *Table)
{
    uint32_t HashIndex = _mm_cvtsi128_si32(Key.Value);
    uint32_t HashSlot  = (HashIndex & Table->HashMask);

    VOID_ASSERT(HashSlot < Table->HashSlotCount);

    uint32_t *Result = &Table->HashTable[HashSlot];
    return Result;
}

static resource_entry *
GetResourceEntry(uint32_t Index, resource_table *Table)
{
    VOID_ASSERT(Index < Table->EntryCount);

    resource_entry *Result = Table->Entries + Index;
    return Result;
}

static bool
ResourceKeyAreEqual(resource_key A, resource_key B)
{
    __m128i Compare = _mm_cmpeq_epi32(A.Value, B.Value);
    bool     Result  = (_mm_movemask_epi8(Compare) == 0xffff);

    return Result;
}

static uint32_t
PopFreeResourceEntry(resource_table *Table)
{
    resource_entry *Sentinel = GetResourceSentinel(Table);

    // At initialization we populate sentinel's the hash chain such that:
    // (Sentinel) -> (Slot) -> (Slot) -> (Slot)
    // If (Sentinel) -> (Nothing), then we have no more slots available.

    if(!Sentinel->NextWithSameHashSlot)
    {
        VOID_ASSERT(!"Not Implemented");
    }

    uint32_t Result = Sentinel->NextWithSameHashSlot;

    resource_entry *Entry = GetResourceEntry(Result, Table);
    Sentinel->NextWithSameHashSlot = Entry->NextWithSameHashSlot;
    Entry->NextWithSameHashSlot    = 0;

    return Result;
}

static ResourceType
GetResourceTypeFromKey(resource_key Key)
{
    uint64_t             High = _mm_extract_epi64(Key.Value, 1);
    ResourceType Type = (ResourceType)(High >> 32);
    return Type;
}

// TODO: Make a better resource allocator.

static void *
AllocateUIResource(uint64_t Size, resource_allocator *Allocator)
{
    return 0;
}

// ----------------------------------------------------------------------------------
// UI Resource Cache Public API

static uint64_t
GetResourceTableFootprint(resource_table_params Params)
{
    uint64_t HashTableSize  = Params.HashSlotCount  * sizeof(uint32_t);
    uint64_t EntryArraySize = Params.EntryCount * sizeof(resource_entry);
    uint64_t Result         = sizeof(resource_table) + HashTableSize + EntryArraySize;

    return Result;
}

static resource_table *
PlaceResourceTableInMemory(resource_table_params Params, void *Memory)
{
    VOID_ASSERT(Params.EntryCount);
    VOID_ASSERT(Params.HashSlotCount);
    VOID_ASSERT(VOID_ISPOWEROFTWO(Params.HashSlotCount));

    resource_table *Result = 0;

    if(Memory)
    {
        uint32_t          *HashTable = (uint32_t *)Memory;
        resource_entry *Entries   = (resource_entry *)(HashTable + Params.HashSlotCount);

        Result = (resource_table *)(Entries + Params.EntryCount);
        Result->HashTable     = HashTable;
        Result->Entries       = Entries;
        Result->EntryCount    = Params.EntryCount;
        Result->HashSlotCount = Params.HashSlotCount;
        Result->HashMask      = Params.HashSlotCount - 1;

        for(uint32_t Idx = 0; Idx < Params.EntryCount; ++Idx)
        {
            resource_entry *Entry = GetResourceEntry(Idx, Result);
            if((Idx + 1) < Params.EntryCount)
            {
                Entry->NextWithSameHashSlot = Idx + 1;
            }
            else
            {
                Entry->NextWithSameHashSlot = 0;
            }

            Entry->ResourceType = ResourceType::None;
            Entry->Memory       = 0;
        }
    }

    return Result;
}

static resource_key
MakeNodeResourceKey(ResourceType Type, uint32_t NodeIndex, layout::layout_tree *Tree)
{
    uint64_t Low  = (uint64_t)Tree;
    uint64_t High = ((uint64_t)Type << 32) | NodeIndex;

    resource_key Key = {.Value = _mm_set_epi64x(High, Low)};
    return Key;
}

// This might be wrong, because I am unsure how string literals are stored when you do something like:
// str8_lit("Consolas") 
// str8_lit("Consolas") 
// Do they get stored in the same place in memory? Does it depend?

static resource_key
MakeFontResourceKey(byte_string Name, float Size)
{
    uint64_t Low  = reinterpret_cast<uint64_t>(Name.String);
    uint64_t High = (static_cast<uint64_t>(ResourceType::Font) << 32) | static_cast<uint32_t>(Size);

    resource_key Key = {.Value = _mm_set_epi64x(High, Low)};
    return Key;
}


static resource_state
FindResourceByKey(resource_key Key, FindResourceFlag Flags, resource_table *Table)
{
    resource_entry *FoundEntry = 0;

    uint32_t *Slot       = GetResourceSlotPointer(Key, Table);
    uint32_t  EntryIndex = Slot[0];

    while(EntryIndex)
    {
        resource_entry *Entry = GetResourceEntry(EntryIndex, Table);
        if(ResourceKeyAreEqual(Entry->Key, Key))
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

        resource_entry *Prev = GetResourceEntry(FoundEntry->PrevLRU, Table);
        resource_entry *Next = GetResourceEntry(FoundEntry->NextLRU, Table);

        Prev->NextLRU = FoundEntry->NextLRU;
        Next->PrevLRU = FoundEntry->PrevLRU;

        ++Table->Stats.CacheHitCount;
    }
    else if((Flags & FindResourceFlag::AddIfNotFound) != FindResourceFlag::None)
    {
        // If we miss an entry we have to first allocate a new one.
        // If we have some hash slot at X: Hash[X]
        // Hash[X] -> (Index) -> (Index)
        // (Entry) -> Hash[X] -> (Index) -> (Index)
        // Hash[X] -> (Index) -> (Index) -> (Index)

        EntryIndex = PopFreeResourceEntry(Table);
        VOID_ASSERT(EntryIndex);

        FoundEntry = GetResourceEntry(EntryIndex, Table);
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

        resource_entry *Sentinel = GetResourceSentinel(Table);
        FoundEntry->NextLRU = Sentinel->NextLRU;
        FoundEntry->PrevLRU = 0;
    
        resource_entry *NextLRU = GetResourceEntry(Sentinel->NextLRU, Table);
        NextLRU->PrevLRU  = EntryIndex;
        Sentinel->NextLRU = EntryIndex;
    }

    resource_state Result =
    {
        .Id           = EntryIndex,
        .ResourceType = FoundEntry ? FoundEntry->ResourceType : ResourceType::None,
        .Resource     = FoundEntry ? FoundEntry->Memory       : 0,
    };

    return Result;
}


static void
UpdateResourceTable(uint32_t Id, resource_key Key, void *Memory, resource_table *Table)
{
    resource_entry *Entry = GetResourceEntry(Id, Table);
    VOID_ASSERT(Entry);

    // This is weird. Kind of.
    if(Entry->Memory && Entry->Memory != Memory)
    {
        VOID_ASSERT(!"...");
        // OSRelease(Entry->Memory);
    }

    Entry->Key          = Key;
    Entry->Memory       = Memory;
    Entry->ResourceType = GetResourceTypeFromKey(Key);

    VOID_ASSERT(Entry->ResourceType != ResourceType::None);
}


static void *
QueryNodeResource(ResourceType Type, uint32_t NodeIndex, layout::layout_tree *Tree, resource_table *Table)
{
    resource_key   Key   = MakeNodeResourceKey(Type, NodeIndex, Tree);
    resource_state State = FindResourceByKey(Key, FindResourceFlag::None, Table);

    void *Result = State.Resource;
    return Result;
}


// -------------------------------------------------------------
// @Public: Frame Node API

//bool node::IsValid()
//{
//    bool Result = Index != ::layout::InvalidIndex;
//    return Result;
//}
//
//
//node node::FindChild(uint32_t FindIndex, pipeline &Pipeline)
//{
//    node Result = { ::layout::FindChild(Index, FindIndex, Tree) };
//    return Result;
//}
//
//
//void node::Append(node Child, pipeline &Pipeline)
//{
//    ::layout::AppendChild(Index, Child.Index, Tree);
//}
//
//
//void node::SetOffset(float XOffset, float YOffset, pipeline &Pipeline)
//{
//    ::layout::SetNodeOffset(Index, XOffset, YOffset, Tree);
//}
//
//
//// There are so many indirections. This is unusual.
//
//void node::SetText(byte_string UserText, resource_key FontKey, pipeline &Pipeline)
//{
//    void_context      &Context       = GetVoidContext();
//    resource_table *ResourceTable = Context.ResourceTable;
//
//    auto  TextKey   = MakeNodeResourceKey(ResourceType::Text, Index, Tree);
//    auto  TextState = FindResourceByKey(TextKey, FindResourceFlag::AddIfNotFound, ResourceTable);
//    auto *Text      = static_cast<text *>(TextState.Resource);
//
//    if(!Text)
//    {
//        auto  FontState = FindResourceByKey(FontKey, FindResourceFlag::None, Context.ResourceTable);
//        auto *Font      = static_cast<font *>(FontState.Resource);
//
//        if(Font)
//        {
//            ntext::TextAnalysis Flags = ntext::TextAnalysis::GenerateWordSlices;
//
//            auto Analysed = ntext::AnalyzeText(UserText.String, UserText.Size, Flags, Font->Generator);
//            auto GlyphRun = ntext::FillAtlas(Analysed, Font->Generator);
//
//            uint64_t Footprint = GetTextFootprint(Analysed, GlyphRun);
//            void    *Memory    = AllocateUIResource(Footprint, &ResourceTable->Allocator);
//
//            Text = PlaceTextInMemory(Analysed, GlyphRun, Font, Memory);
//            if(Text)
//            {
//                UpdateResourceTable(TextState.Id, TextKey, Text, Context.ResourceTable);
//            }
//        }
//    }
//    else
//    {
//        VOID_ASSERT(!"Unsure");
//    }
//}
//
//void node::SetTextInput(uint8_t *Buffer, uint64_t BufferSize, pipeline &Pipeline)
//{
//}
//
//// This is badly implemented.
//
//void node::SetScroll(float ScrollSpeed, AxisType Axis, pipeline &Pipeline)
//{
//    void_context &Context  = GetVoidContext();
//
//    resource_key   Key   = MakeNodeResourceKey(ResourceType::ScrollRegion, Index, Tree);
//    resource_state State = FindResourceByKey(Key, FindResourceFlag::AddIfNotFound, Context.ResourceTable);
//
//    uint64_t Size   = layout::GetScrollRegionFootprint();
//    void    *Memory = AllocateUIResource(Size, &Context.ResourceTable->Allocator);
//
//    layout::scroll_region_params Params =
//    {
//        .PixelPerLine = ScrollSpeed,
//        .Axis         = Axis,
//    };
//
//    layout::scroll_region *ScrollRegion = layout::PlaceScrollRegionInMemory(Params, Memory);
//    if(ScrollRegion)
//    {
//        UpdateResourceTable(State.Id, Key, ScrollRegion, Context.ResourceTable);
//    }
//}
//
//void node::SetImage(byte_string Path, byte_string Group, pipeline &Pipeline)
//{
//    // TODO: Reimplement.
//}
//
//void node::DebugBox(uint32_t Flag, bool Draw, pipeline &Pipeline)
//{
//}

// ----------------------------------------------------------------------------------
// Context Public API Implementation

struct pointer_state
{
    uint32_t   Id;
    vec2_float Position;
    vec2_float LastPosition;
    uint32_t   ButtonMask;

    // Targets?
    bool     IsCaptured; // This might be, uhhh, a state flag with some other states.
};


static void
PushPointerEvent(pointer_event_list *List, pointer_event_node *Node, PointerEvent Type)
{
    VOID_ASSERT(Node);

    if(List && Type != PointerEvent::None)
    {
        Node->Next = 0;
        Node->Prev = 0;
        Node->Value.Type = Type;

        AppendToDoublyLinkedList(List, Node, List->Count);
    }
}


static void
PushPointerMoveEvent(pointer_event_list *List, pointer_event_node *Node, vec2_float Position, vec2_float Delta)
{
    if(Node)
    {
        Node->Value.Position = Position;
        Node->Value.Delta = Delta;

        PushPointerEvent(List, Node, PointerEvent::Move);
    }
}


static void
PushPointerClickEvent(pointer_event_list *List, pointer_event_node *Node, uint32_t Button, vec2_float Position)
{
    PushPointerEvent(List, Node, PointerEvent::Click);

    if(Node)
    {
        Node->Value.ButtonMask = Button;
        Node->Value.Position = Position;
    }
}


static void
PushPointerReleaseEvent(pointer_event_list *List, pointer_event_node *Node, uint32_t Button, vec2_float Position)
{
    if(Node)
    {
        Node->Value.ButtonMask = Button;
        Node->Value.Position = Position;

        PushPointerEvent(List, Node, PointerEvent::Release);
    }
}


// We take this tree parameter for simplicity while we refactor this part

static void
BeginFrame(float Width, float Height, const pointer_event_list &EventList, layout::layout_tree *Tree)
{
    void_context &Context = GetVoidContext();

    // Obviously this is temporary. Unsure how I want to structure this yet.
    static pointer_state PointerStates[1];

    for(pointer_event_node *EventNode = EventList.First; EventNode != 0; EventNode = EventNode->Next)
    {
        const pointer_event &Event = EventNode->Value;

        switch(Event.Type)
        {

        case PointerEvent::Move:
        {
            pointer_state &State = PointerStates[0];
            State.LastPosition = State.Position;
            State.Position     = Event.Position;

            if(State.IsCaptured)
            {
                layout::HandlePointerMove(Event.Delta, Tree);
            }
        } break;

        case PointerEvent::Click:
        {
            pointer_state &State = PointerStates[0];
            State.ButtonMask |= Event.ButtonMask;
            State.IsCaptured  = layout::HandlePointerClick(State.Position, State.ButtonMask, 0, Tree);
        } break;

        case PointerEvent::Release:
        {
            pointer_state &State = PointerStates[0];
            State.ButtonMask &= ~Event.ButtonMask;

            if(State.IsCaptured)
            {
                layout::HandlePointerRelease(State.Position, State.ButtonMask, 0, Tree);
                State.IsCaptured = false;
            }
        } break;

        default:
        {
            VOID_ASSERT(!"Unknown Event Type");
        } break;

        }
    }

    // If the ButtonMask is 0 then it means the pointer is in a hover state.
    // We look for that hover target in any of the pipelines.

    for(int32_t PointerIdx = 0; PointerIdx < 1; ++PointerIdx)
    {
        pointer_state &State = PointerStates[PointerIdx];

        if(State.ButtonMask == 0)
        {
            layout::HandlePointerHover(State.Position, 0, Tree);
        }
    }

    Context.Width  = Width;
    Context.Height = Height;
}

static void
EndFrame(void)
{
    // TODO: Unsure
}

// ==================================================================================
// @Public : Context

static void_context &
GetVoidContext(void)
{
    return GlobalVoidContext;
}

// NOTE:
// Context parameters? Unsure.

static void
CreateVoidContext(void)
{
    // void_context &Context = GetVoidContext();

    // State
    {
        // resource_table_params TableParams =
        //{
        //    .HashSlotCount = 512,
        //    .EntryCount    = 2048,
        //};

        //uint64_t TableFootprint = GetResourceTableFootprint(TableParams);
        //void    *TableMemory    = PushArena(Context.StateArena, TableFootprint, 0);

        //Context.ResourceTable =  PlaceResourceTableInMemory(TableParams, TableMemory);

        //VOID_ASSERT(Context.ResourceTable);
    }
}

// -----------------------------------------------------------------------------
// @Private Meta-Layout API
// Functions intended for internal use.
// -----------------------------------------------------------------------------

component::component(const char *Name, layout::NodeFlags Flags, const cached_style *Style, layout::layout_tree *Tree)
    : component(str8_comp(Name), Flags, Style, Tree)
{
}

component::component(byte_string Name, layout::NodeFlags Flags, const cached_style *Style, layout::layout_tree *Tree)
{
    LayoutIndex = layout::CreateNode(HashByteString(Name), Flags, Style, Tree);
    LayoutTree  = Tree;
    layout::UpdateInput(LayoutIndex, Style, Tree);
}


void component::SetStyle(const cached_style *Style)
{
    layout::UpdateInput(LayoutIndex, Style, LayoutTree);
}


bool component::Push(layout::parent_node *Node)
{
    bool Result = false;

    if(Node)
    {
        layout::PushParent(LayoutIndex, LayoutTree, Node);
        Result = true;
    }

    return Result;
}


bool component::Pop()
{
    layout::PopParent(LayoutIndex, LayoutTree);

    return true;
}

} // namespace core
