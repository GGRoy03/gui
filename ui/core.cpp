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
    void *Result = OSReserveMemory(Size);

    if(Result)
    {
        bool Committed = OSCommitMemory(Result, Size);
        VOID_ASSERT(Committed);

        Allocator->AllocatedCount += 1;
        Allocator->AllocatedBytes += Size;
    }

    return Result;
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
        OSRelease(Entry->Memory);
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

bool node::IsValid()
{
    bool Result = Index != ::layout::InvalidIndex;
    return Result;
}


node node::FindChild(uint32_t FindIndex, pipeline &Pipeline)
{
    node Result = { ::layout::FindChild(Index, FindIndex, Pipeline.Tree) };
    return Result;
}


void node::Append(node Child, pipeline &Pipeline)
{
    ::layout::AppendChild(Index, Child.Index, Pipeline.Tree);
}


void node::SetOffset(float XOffset, float YOffset, pipeline &Pipeline)
{
    ::layout::SetNodeOffset(Index, XOffset, YOffset, Pipeline.Tree);
}


// There are so many indirections. This is unusual.

void node::SetText(byte_string UserText, resource_key FontKey, pipeline &Pipeline)
{
    void_context      &Context       = GetVoidContext();
    resource_table *ResourceTable = Context.ResourceTable;

    auto  TextKey   = MakeNodeResourceKey(ResourceType::Text, Index, Pipeline.Tree);
    auto  TextState = FindResourceByKey(TextKey, FindResourceFlag::AddIfNotFound, ResourceTable);
    auto *Text      = static_cast<text *>(TextState.Resource);

    if(!Text)
    {
        auto  FontState = FindResourceByKey(FontKey, FindResourceFlag::None, Context.ResourceTable);
        auto *Font      = static_cast<font *>(FontState.Resource);

        if(Font)
        {
            ntext::TextAnalysis Flags = ntext::TextAnalysis::GenerateWordSlices;

            auto Analysed = ntext::AnalyzeText(UserText.String, UserText.Size, Flags, Font->Generator);
            auto GlyphRun = ntext::FillAtlas(Analysed, Font->Generator);

            uint64_t Footprint = GetTextFootprint(Analysed, GlyphRun);
            void    *Memory    = AllocateUIResource(Footprint, &ResourceTable->Allocator);

            Text = PlaceTextInMemory(Analysed, GlyphRun, Font, Memory);
            if(Text)
            {
                UpdateResourceTable(TextState.Id, TextKey, Text, Context.ResourceTable);
            }
        }
    }
    else
    {
        VOID_ASSERT(!"Unsure");
    }
}

void node::SetTextInput(uint8_t *Buffer, uint64_t BufferSize, pipeline &Pipeline)
{
}

// This is badly implemented.

void node::SetScroll(float ScrollSpeed, AxisType Axis, pipeline &Pipeline)
{
    void_context &Context  = GetVoidContext();

    resource_key   Key   = MakeNodeResourceKey(ResourceType::ScrollRegion, Index, Pipeline.Tree);
    resource_state State = FindResourceByKey(Key, FindResourceFlag::AddIfNotFound, Context.ResourceTable);

    uint64_t Size   = layout::GetScrollRegionFootprint();
    void    *Memory = AllocateUIResource(Size, &Context.ResourceTable->Allocator);

    layout::scroll_region_params Params =
    {
        .PixelPerLine = ScrollSpeed,
        .Axis         = Axis,
    };

    layout::scroll_region *ScrollRegion = layout::PlaceScrollRegionInMemory(Params, Memory);
    if(ScrollRegion)
    {
        UpdateResourceTable(State.Id, Key, ScrollRegion, Context.ResourceTable);
    }
}

void node::SetImage(byte_string Path, byte_string Group, pipeline &Pipeline)
{
    // TODO: Reimplement.
}

void node::DebugBox(uint32_t Flag, bool Draw, pipeline &Pipeline)
{
}

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
    Pipeline PipelineSource;

    // Other Stuff
};

static void
BeginFrame(vec2_int WindowSize)
{
    void_context       &Context   = GetVoidContext();
    pointer_event_list &EventList = OSGetInputs()->PointerEventList;

    static pointer_state PointerStates[1];

    // It seems like processing the pointer events here is the better idea.
    // But we need some kind of UI side state. Which maps to some pointer.
    // One bad thing that can probably be fixed with better logic is that
    // there is a decent amount of shared state.

    IterateLinkedList((&EventList), pointer_event_node *, EventNode)
    {
        pointer_event &Event = EventNode->Value;

        switch(Event.Type)
        {

        case PointerEvent::Move:
        {
            pointer_state &State = PointerStates[0];

            State.LastPosition = State.Position;
            State.Position     = Event.Position;

            if(State.IsCaptured)
            {
                // Not great.
                pipeline &Pipeline = Context.PipelineArray[static_cast<uint32_t>(State.PipelineSource)];

                layout::HandlePointerMove(Event.Delta, Pipeline.Tree);
            }
        } break;

        case PointerEvent::Click:
        {
            pointer_state &State = PointerStates[0];

            State.ButtonMask |= Event.ButtonMask;

            for(uint32_t Idx = 0; Idx < Context.PipelineCount; ++Idx)
            {
                pipeline &Pipeline = Context.PipelineArray[Idx];

                // So here, we'd call something like: IsMouseOverPipeline
                if(true)
                {
                    State.IsCaptured     = layout::HandlePointerClick(State.Position, State.ButtonMask, 0, Pipeline.Tree);
                    State.PipelineSource = Pipeline.Type;
                }

                if(State.IsCaptured)
                {
                    break;
                }
            }
        } break;

        case PointerEvent::Release:
        {
            pointer_state &State = PointerStates[0];

            State.ButtonMask &= ~Event.ButtonMask;

            if(State.IsCaptured)
            {
                // Not great.
                pipeline &Pipeline = Context.PipelineArray[static_cast<uint32_t>(State.PipelineSource)];

                layout::HandlePointerRelease(State.Position, State.ButtonMask, 0, Pipeline.Tree);

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
            for(uint32_t Idx = 0; Idx < Context.PipelineCount; ++Idx)
            {
                pipeline &Pipeline = Context.PipelineArray[Idx];

                bool Handled = layout::HandlePointerHover(State.Position, 0, Pipeline.Tree);
                if(Handled)
                {
                    break;
                }
            }
        }
    }

    Context.WindowSize = WindowSize;
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
    void_context &Context = GetVoidContext();

    // Memory
    {
        Context.StateArena = AllocateArena({});

        VOID_ASSERT(Context.StateArena);
    }

    // State
    {
        resource_table_params TableParams =
        {
            .HashSlotCount = 512,
            .EntryCount    = 2048,
        };

        uint64_t TableFootprint = GetResourceTableFootprint(TableParams);
        void    *TableMemory    = PushArena(Context.StateArena, TableFootprint, 0);

        Context.ResourceTable =  PlaceResourceTableInMemory(TableParams, TableMemory);

        VOID_ASSERT(Context.ResourceTable);
    }
}

// -----------------------------------------------------------------------------
// @Private Meta-Layout API
// Functions intended for internal use.
// -----------------------------------------------------------------------------


constexpr uint32_t InvalidMetaNodeIndex = 0xFFFFFFFF;


struct meta_node
{
    // Frame State

    NodeType          Type;
    layout::NodeFlags Flags;
    uint64_t          Hint;
    uint32_t          Index;
    uint32_t          Style;
    uint32_t          Parent;

    // Persistent State (EXP)

    uint32_t LayoutIndex;
    
    static bool IsValid(meta_node *Node)
    {
        bool Result = Node && Node->Index != InvalidMetaNodeIndex;
        return Result;
    }
};


struct meta_parent_node
{
    meta_parent_node *Prev;
    uint32_t             Value;
};


struct meta_tree
{
    // Nodes

    meta_node *Nodes;
    uint32_t      NodeCount;
    uint32_t      NodeCapacity;

    // Frame State

    meta_parent_node *Parent;

    // Helpers
    
    static bool IsValid(meta_tree *Tree)
    {
        bool Result = Tree && Tree->Nodes && Tree->NodeCount <= Tree->NodeCapacity;
        return Result;
    }
};


static uint64_t
GetMetaTreeFootprint(uint64_t NodeCount)
{
    uint64_t NodeBuffer = NodeCount * sizeof(meta_node);
    uint64_t Result     = NodeBuffer + sizeof(meta_tree);
    return Result;
}


static meta_tree *
PlaceMetaTreeInMemory(uint64_t NodeCount, void *Memory)
{
    meta_tree *Result = nullptr;
    
    if(Memory)
    {
        meta_node *Nodes = static_cast<meta_node*>(Memory);
        Result = reinterpret_cast<meta_tree*>(Nodes + NodeCount);
        
        Result->Nodes = Nodes;
        Result->NodeCount = 0;
        Result->NodeCapacity = NodeCount;
        
        for(uint32_t i = 0; i < NodeCount; ++i)
        {
            Nodes[i].Index = InvalidMetaNodeIndex;
        }
    }
    
    return Result;
}


static void
MetaTreesEx(uint32_t ActiveIdx, meta_tree *ActiveTree, uint32_t StaticIdx, meta_tree *StaticTree, layout::layout_tree *LayoutTree, cached_style *StyleArray)
{
    if(ActiveIdx >= ActiveTree->NodeCount)
    {
        return;
    }

    meta_node *ActiveNode = ActiveTree->Nodes + ActiveIdx;
    meta_node *StaticNode = (StaticIdx < StaticTree->NodeCount) ? StaticTree->Nodes + StaticIdx : nullptr;
    
    VOID_ASSERT(meta_node::IsValid(ActiveNode));
    
    if(!meta_node::IsValid(StaticNode) || ActiveNode->Type != StaticNode->Type)
    {
        ActiveNode->LayoutIndex = layout::CreateNode(ActiveNode->Flags, LayoutTree);

        layout::UpdateLayoutInput(ActiveNode->LayoutIndex, ActiveNode->Style, StyleArray[ActiveNode->Style], LayoutTree);

        if(ActiveNode->Parent != InvalidMetaNodeIndex)
        {
            meta_node *Parent = ActiveTree->Nodes + ActiveNode->Parent;
            layout::AppendChild(Parent->LayoutIndex, ActiveNode->LayoutIndex, LayoutTree);
        }
    }
    else
    {
        VOID_ASSERT(meta_node::IsValid(StaticNode));

        // If we reach this branch we want to update attributes. Now that is up to us to figure out
        // which attributes make sense.

        // I do not know if persisting state like this is the correct approach.
        ActiveNode->LayoutIndex = StaticNode->LayoutIndex;
    }
}


static void
MetaTrees(meta_tree *ActiveTree, meta_tree *StaticTree, layout::layout_tree *LayoutTree, cached_style *StyleArray)
{
    if(meta_tree::IsValid(ActiveTree))
    {
        for(uint32_t i = 0; i < ActiveTree->NodeCount; ++i)
        {
            MetaTreesEx(i, ActiveTree, i, StaticTree, LayoutTree, StyleArray);
        }
    }
}


static uint32_t
UICreateNode2(uint32_t StyleIndex, NodeType Type, layout::NodeFlags Flags, meta_tree *Tree)
{
    uint32_t Result = InvalidMetaNodeIndex;
    
    if(meta_tree::IsValid(Tree) && Tree->NodeCount < Tree->NodeCapacity)
    {
        uint32_t      Index = Tree->NodeCount++;
        meta_node *Node  = Tree->Nodes + Index;
        
        Node->Parent = Tree->Parent ? Tree->Parent->Value : InvalidMetaNodeIndex;
        Node->Index  = Index;
        Node->Style  = StyleIndex;
        Node->Type   = Type;
        Node->Flags  = Flags;
        Node->Hint   = 0;
        
        Result = Index;
    }
    
    return Result;
}


static bool
UIPushLayoutParent2(uint32_t NodeIndex, meta_tree *MetaTree, memory_arena *Arena)
{
    if(meta_tree::IsValid(MetaTree) && Arena)
    {
        meta_parent_node *ParentNode = PushStruct(Arena, meta_parent_node);
        if(ParentNode)
        {
            ParentNode->Value = NodeIndex;
            ParentNode->Prev  = MetaTree->Parent;

            MetaTree->Parent = ParentNode;
        }
    }

    return true;
}


static bool
UIPopLayoutParent2(uint32_t NodeIndex, meta_tree *MetaTree)
{
    if(meta_tree::IsValid(MetaTree))
    {
        if(MetaTree->Parent && MetaTree->Parent->Value == NodeIndex)
        {
            MetaTree->Parent = MetaTree->Parent->Prev;
        }
    }

    return true;
}


// --------


static uint64_t
GetPipelineStateFootprint(const pipeline_params &Params)
{
    uint64_t TreeSize = layout::GetLayoutTreeFootprint(Params.NodeCount);
    uint64_t MetaSize = GetMetaTreeFootprint(Params.NodeCount) * 2;
    uint64_t Result   = TreeSize + MetaSize;

    return Result;
}

// ==================================================================================
// @Public : Pipeline API

// TODO: Error check.

static void
UICreatePipeline(const pipeline_params &Params)
{
    void_context &Context  = GetVoidContext();
    pipeline  &Pipeline = Context.PipelineArray[static_cast<uint32_t>(Params.Pipeline)];

    // Memory
    {
        Pipeline.StateArena = AllocateArena({.ReserveSize = GetPipelineStateFootprint(Params)});
        Pipeline.FrameArena = AllocateArena({.ReserveSize = Params.FrameBudget});

        VOID_ASSERT(Pipeline.StateArena && Pipeline.FrameArena);
    }

    // UI State
    {
        uint64_t TreeFootprint  = layout::GetLayoutTreeFootprint(Params.NodeCount);
        void    *TreeMemory     = PushArena(Pipeline.StateArena, TreeFootprint, layout::GetLayoutTreeAlignment());

        Pipeline.Tree = layout::PlaceLayoutTreeInMemory(Params.NodeCount, TreeMemory);

        uint64_t MetaFootprint = GetMetaTreeFootprint(Params.NodeCount);
        void    *MetaMemory0   = PushArena(Pipeline.StateArena, MetaFootprint, AlignOf(meta_node));
        void    *MetaMemory1   = PushArena(Pipeline.StateArena, MetaFootprint, AlignOf(meta_node));

        Pipeline.MetaTrees[0] = PlaceMetaTreeInMemory(Params.NodeCount, MetaMemory0);
        Pipeline.MetaTrees[1] = PlaceMetaTreeInMemory(Params.NodeCount, MetaMemory1);

        VOID_ASSERT(Pipeline.Tree);
    }

    // User State
    {
        Pipeline.Type          = Params.Pipeline;
        Pipeline.StyleArray    = Params.StyleArray;
        Pipeline.StyleIndexMin = Params.StyleIndexMin;
        Pipeline.StyleIndexMax = Params.StyleIndexMax;

        VOID_ASSERT(Pipeline.StyleArray && Pipeline.StyleIndexMin <= Pipeline.StyleIndexMax);
    }

    ++Context.PipelineCount;
}


// This probably should be done in frame start. Idk about that whole bind/unbind stuff?


static pipeline &
UIBindPipeline(Pipeline UserPipeline)
{
    void_context &Context  = GetVoidContext();
    pipeline  &Pipeline = Context.PipelineArray[static_cast<uint32_t>(UserPipeline)];

    if(!Pipeline.Bound)
    {
        if(Pipeline.FrameArena)
        {
            PopArenaTo(Pipeline.FrameArena, 0);
        }

        Pipeline.ActiveMetaTree = (Pipeline.ActiveMetaTree + 1) % 2;
        Pipeline.Bound          = true;

        // Experimental
        meta_tree *ActiveTree = Pipeline.GetActiveTree();
        if (meta_tree::IsValid(ActiveTree))
        {
            ActiveTree->NodeCount = 0;
        }
    }

    return Pipeline;
}


static void
UIUnbindPipeline(Pipeline UserPipeline)
{
    void_context &Context  = GetVoidContext();
    pipeline     &Pipeline = Context.PipelineArray[static_cast<uint32_t>(UserPipeline)];

    if(Pipeline.Bound)
    {
        // Experimental.
        meta_tree *ActiveTree = Pipeline.GetActiveTree();
        meta_tree *StaticTree = ActiveTree == Pipeline.MetaTrees[0] ? Pipeline.MetaTrees[1] : Pipeline.MetaTrees[0];

        MetaTrees(ActiveTree, StaticTree, Pipeline.Tree, Pipeline.StyleArray);

        layout::ComputeTreeLayout(Pipeline.Tree);

        paint_buffer Buffer = layout::GeneratePaintBuffer(Pipeline.Tree, Pipeline.StyleArray, Pipeline.FrameArena);
        if(Buffer.Commands && Buffer.Size)
        {
            ExecutePaintCommands(Buffer, Pipeline.FrameArena);
        }

        Pipeline.Bound = false;
    }
}


} // namespace core
