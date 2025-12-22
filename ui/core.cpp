// ----------------------------------------------------------------------------------
// UI Resource Cache Private Implementation

struct ui_resource_allocator
{
    uint64_t AllocatedCount;
    uint64_t AllocatedBytes;
};

struct ui_resource_entry
{
    ui_resource_key Key;

    uint32_t        NextWithSameHashSlot;
    uint32_t        NextLRU;
    uint32_t        PrevLRU;

    UIResource_Type ResourceType;
    void           *Memory;
};

struct ui_resource_table
{
    ui_resource_stats     Stats;
    ui_resource_allocator Allocator;

    uint32_t              HashMask;
    uint32_t              HashSlotCount;
    uint32_t              EntryCount;

    uint32_t             *HashTable;
    ui_resource_entry    *Entries;
};

static ui_resource_entry *
GetResourceSentinel(ui_resource_table *Table)
{
    ui_resource_entry *Result = Table->Entries;
    return Result;
}

static uint32_t *
GetResourceSlotPointer(ui_resource_key Key, ui_resource_table *Table)
{
    uint32_t HashIndex = _mm_cvtsi128_si32(Key.Value);
    uint32_t HashSlot  = (HashIndex & Table->HashMask);

    VOID_ASSERT(HashSlot < Table->HashSlotCount);

    uint32_t *Result = &Table->HashTable[HashSlot];
    return Result;
}

static ui_resource_entry *
GetResourceEntry(uint32_t Index, ui_resource_table *Table)
{
    VOID_ASSERT(Index < Table->EntryCount);

    ui_resource_entry *Result = Table->Entries + Index;
    return Result;
}

static bool
ResourceKeyAreEqual(ui_resource_key A, ui_resource_key B)
{
    __m128i Compare = _mm_cmpeq_epi32(A.Value, B.Value);
    bool     Result  = (_mm_movemask_epi8(Compare) == 0xffff);

    return Result;
}

static uint32_t
PopFreeResourceEntry(ui_resource_table *Table)
{
    ui_resource_entry *Sentinel = GetResourceSentinel(Table);

    // At initialization we populate sentinel's the hash chain such that:
    // (Sentinel) -> (Slot) -> (Slot) -> (Slot)
    // If (Sentinel) -> (Nothing), then we have no more slots available.

    if(!Sentinel->NextWithSameHashSlot)
    {
        VOID_ASSERT(!"Not Implemented");
    }

    uint32_t Result = Sentinel->NextWithSameHashSlot;

    ui_resource_entry *Entry = GetResourceEntry(Result, Table);
    Sentinel->NextWithSameHashSlot = Entry->NextWithSameHashSlot;
    Entry->NextWithSameHashSlot    = 0;

    return Result;
}

static UIResource_Type
GetResourceTypeFromKey(ui_resource_key Key)
{
    uint64_t             High = _mm_extract_epi64(Key.Value, 1);
    UIResource_Type Type = (UIResource_Type)(High >> 32);
    return Type;
}

// TODO: Make a better resource allocator.

static void *
AllocateUIResource(uint64_t Size, ui_resource_allocator *Allocator)
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
GetResourceTableFootprint(ui_resource_table_params Params)
{
    uint64_t HashTableSize  = Params.HashSlotCount  * sizeof(uint32_t);
    uint64_t EntryArraySize = Params.EntryCount * sizeof(ui_resource_entry);
    uint64_t Result         = sizeof(ui_resource_table) + HashTableSize + EntryArraySize;

    return Result;
}

static ui_resource_table *
PlaceResourceTableInMemory(ui_resource_table_params Params, void *Memory)
{
    VOID_ASSERT(Params.EntryCount);
    VOID_ASSERT(Params.HashSlotCount);
    VOID_ASSERT(VOID_ISPOWEROFTWO(Params.HashSlotCount));

    ui_resource_table *Result = 0;

    if(Memory)
    {
        uint32_t          *HashTable = (uint32_t *)Memory;
        ui_resource_entry *Entries   = (ui_resource_entry *)(HashTable + Params.HashSlotCount);

        Result = (ui_resource_table *)(Entries + Params.EntryCount);
        Result->HashTable     = HashTable;
        Result->Entries       = Entries;
        Result->EntryCount    = Params.EntryCount;
        Result->HashSlotCount = Params.HashSlotCount;
        Result->HashMask      = Params.HashSlotCount - 1;

        for(uint32_t Idx = 0; Idx < Params.EntryCount; ++Idx)
        {
            ui_resource_entry *Entry = GetResourceEntry(Idx, Result);
            if((Idx + 1) < Params.EntryCount)
            {
                Entry->NextWithSameHashSlot = Idx + 1;
            }
            else
            {
                Entry->NextWithSameHashSlot = 0;
            }

            Entry->ResourceType = UIResource_None;
            Entry->Memory       = 0;
        }
    }

    return Result;
}

static ui_resource_key
MakeNodeResourceKey(UIResource_Type Type, uint32_t NodeIndex, ui_layout_tree *Tree)
{
    uint64_t Low  = (uint64_t)Tree;
    uint64_t High = ((uint64_t)Type << 32) | NodeIndex;

    ui_resource_key Key = {.Value = _mm_set_epi64x(High, Low)};
    return Key;
}

// This might be wrong, because I am unsure how string literals are stored when you do something like:
// str8_lit("Consolas") 
// str8_lit("Consolas") 
// Do they get stored in the same place in memory? Does it depend?

static ui_resource_key
MakeFontResourceKey(byte_string Name, float Size)
{
    uint64_t Low  = reinterpret_cast<uint64_t>(Name.String);
    uint64_t High = (static_cast<uint64_t>(UIResource_Font) << 32) | static_cast<uint32_t>(Size);

    ui_resource_key Key = {.Value = _mm_set_epi64x(High, Low)};
    return Key;
}


static ui_resource_state
FindResourceByKey(ui_resource_key Key, FindResourceFlag Flags, ui_resource_table *Table)
{
    ui_resource_entry *FoundEntry = 0;

    uint32_t *Slot       = GetResourceSlotPointer(Key, Table);
    uint32_t  EntryIndex = Slot[0];

    while(EntryIndex)
    {
        ui_resource_entry *Entry = GetResourceEntry(EntryIndex, Table);
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

        ui_resource_entry *Prev = GetResourceEntry(FoundEntry->PrevLRU, Table);
        ui_resource_entry *Next = GetResourceEntry(FoundEntry->NextLRU, Table);

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

        ui_resource_entry *Sentinel = GetResourceSentinel(Table);
        FoundEntry->NextLRU = Sentinel->NextLRU;
        FoundEntry->PrevLRU = 0;
    
        ui_resource_entry *NextLRU = GetResourceEntry(Sentinel->NextLRU, Table);
        NextLRU->PrevLRU  = EntryIndex;
        Sentinel->NextLRU = EntryIndex;
    }

    ui_resource_state Result =
    {
        .Id           = EntryIndex,
        .ResourceType = FoundEntry ? FoundEntry->ResourceType : UIResource_None,
        .Resource     = FoundEntry ? FoundEntry->Memory       : 0,
    };

    return Result;
}


static void
UpdateResourceTable(uint32_t Id, ui_resource_key Key, void *Memory, ui_resource_table *Table)
{
    ui_resource_entry *Entry = GetResourceEntry(Id, Table);
    VOID_ASSERT(Entry);

    // This is weird. Kind of.
    if(Entry->Memory && Entry->Memory != Memory)
    {
        OSRelease(Entry->Memory);
    }

    Entry->Key          = Key;
    Entry->Memory       = Memory;
    Entry->ResourceType = GetResourceTypeFromKey(Key);

    VOID_ASSERT(Entry->ResourceType != UIResource_None);
}


static void *
QueryNodeResource(UIResource_Type Type, uint32_t NodeIndex, ui_layout_tree *Tree, ui_resource_table *Table)
{
    ui_resource_key   Key   = MakeNodeResourceKey(Type, NodeIndex, Tree);
    ui_resource_state State = FindResourceByKey(Key, FindResourceFlag::None, Table);

    void *Result = State.Resource;
    return Result;
}


// ------------------------------------------------------------
// @Public: Image Processing API

static void
UICreateImageGroup(byte_string Name, int Width, int Height)
{
    // TODO: Reimplement.
}

static void
LoadImageInGroup(byte_string GroupName, byte_string Path)
{
    // TODO: Reimplement.
}

// -------------------------------------------------------------
// @Public: Frame Node API

bool ui_node::IsValid()
{
    bool Result = Index != InvalidLayoutNodeIndex;
    return Result;
}

void ui_node::SetStyle(uint32_t StyleIndex, ui_pipeline &Pipeline)
{
    if(StyleIndex >= Pipeline.StyleIndexMin && StyleIndex <= Pipeline.StyleIndexMax)
    {
        SetNodeProperties(Index, StyleIndex, Pipeline.StyleArray[StyleIndex], Pipeline.Tree);
    }
}

ui_node ui_node::FindChild(uint32_t FindIndex, ui_pipeline &Pipeline)
{
    ui_node Result = { UIFindChild(Index, FindIndex, Pipeline.Tree) };
    return Result;
}


void ui_node::Append(ui_node Child, ui_pipeline &Pipeline)
{
    UIAppendChild(Index, Child.Index, Pipeline.Tree);
}


// There are so many indirections. This is unusual.

void ui_node::SetText(byte_string UserText, ui_resource_key FontKey, ui_pipeline &Pipeline)
{
    void_context      &Context       = GetVoidContext();
    ui_resource_table *ResourceTable = Context.ResourceTable;

    auto  TextKey   = MakeNodeResourceKey(UIResource_Text, Index, Pipeline.Tree);
    auto  TextState = FindResourceByKey(TextKey, FindResourceFlag::AddIfNotFound, ResourceTable);
    auto *Text      = static_cast<ui_text *>(TextState.Resource);

    if(!Text)
    {
        auto  FontState = FindResourceByKey(FontKey, FindResourceFlag::None, Context.ResourceTable);
        auto *Font      = static_cast<ui_font *>(FontState.Resource);

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

void ui_node::SetTextInput(uint8_t *Buffer, uint64_t BufferSize, ui_pipeline &Pipeline)
{
}

// This is badly implemented.

void ui_node::SetScroll(float ScrollSpeed, UIAxis_Type Axis, ui_pipeline &Pipeline)
{
    void_context &Context  = GetVoidContext();

    ui_resource_key   Key   = MakeNodeResourceKey(UIResource_ScrollRegion, Index, Pipeline.Tree);
    ui_resource_state State = FindResourceByKey(Key, FindResourceFlag::AddIfNotFound, Context.ResourceTable);

    uint64_t Size   = GetScrollRegionFootprint();
    void    *Memory = AllocateUIResource(Size, &Context.ResourceTable->Allocator);

    scroll_region_params Params =
    {
        .PixelPerLine = ScrollSpeed,
        .Axis         = Axis,
    };

    ui_scroll_region *ScrollRegion = PlaceScrollRegionInMemory(Params, Memory);
    if(ScrollRegion)
    {
        UpdateResourceTable(State.Id, Key, ScrollRegion, Context.ResourceTable);
    }
}

void ui_node::SetImage(byte_string Path, byte_string Group, ui_pipeline &Pipeline)
{
    // TODO: Reimplement.
}

void ui_node::DebugBox(uint32_t Flag, bool Draw, ui_pipeline &Pipeline)
{
}

// ----------------------------------------------------------------------------------
// Context Public API Implementation

struct ui_pointer_state
{
    uint32_t   Id;
    vec2_float Position;
    vec2_float LastPosition;
    uint32_t   ButtonMask;

    // Targets?
    bool       IsCaptured; // This might be, uhhh, a state flag with some other states.
    UIPipeline PipelineSource;

    // Other Stuff
};

static void
UIBeginFrame(vec2_int WindowSize)
{
    void_context       &Context   = GetVoidContext();
    pointer_event_list &EventList = OSGetInputs()->PointerEventList;

    static ui_pointer_state PointerStates[1];

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
            ui_pointer_state &State = PointerStates[0];

            State.LastPosition = State.Position;
            State.Position     = Event.Position;

            if(State.IsCaptured)
            {
                // Not great.
                ui_pipeline &Pipeline = Context.PipelineArray[static_cast<uint32_t>(State.PipelineSource)];

                HandlePointerMove(Event.Delta, Pipeline.Tree);
            }
        } break;

        case PointerEvent::Click:
        {
            ui_pointer_state &State = PointerStates[0];

            State.ButtonMask |= Event.ButtonMask;

            for(uint32_t Idx = 0; Idx < Context.PipelineCount; ++Idx)
            {
                ui_pipeline &Pipeline = Context.PipelineArray[Idx];

                // So here, we'd call something like: IsMouseOverPipeline
                if(true)
                {
                    State.IsCaptured     = HandlePointerClick(State.Position, State.ButtonMask, 0, Pipeline.Tree);
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
            ui_pointer_state &State = PointerStates[0];

            State.ButtonMask &= ~Event.ButtonMask;

            if(State.IsCaptured)
            {
                // Not great.
                ui_pipeline &Pipeline = Context.PipelineArray[static_cast<uint32_t>(State.PipelineSource)];

                HandlePointerRelease(State.Position, State.ButtonMask, 0, Pipeline.Tree);

                State.IsCaptured = false;
            }
        } break;

        default:
        {
            VOID_ASSERT(!"Unknown Event Type");
        } break;

        }
    }

    // If _some_ ButtonMask is 0 then it means the pointer is in a hover state.
    // We look for that hover target in any of the pipelines.

    for(int32_t PointerIdx = 0; PointerIdx < 1; ++PointerIdx)
    {
        ui_pointer_state &State = PointerStates[PointerIdx];

        if(State.ButtonMask == 0)
        {
            for(uint32_t Idx = 0; Idx < Context.PipelineCount; ++Idx)
            {
                ui_pipeline &Pipeline = Context.PipelineArray[Idx];

                bool Handled = HandlePointerHover(State.Position, 0, Pipeline.Tree);
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
UIEndFrame(void)
{
    // TODO: Something.
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
        ui_resource_table_params TableParams =
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

static uint64_t
GetPipelineStateFootprint(const ui_pipeline_params &Params)
{
    uint64_t TreeSize = GetLayoutTreeFootprint(Params.NodeCount);
    uint64_t Result   = TreeSize;

    return Result;
}

// ==================================================================================
// @Public : Pipeline API

// TODO: Error check.

static void
UICreatePipeline(const ui_pipeline_params &Params)
{
    void_context &Context  = GetVoidContext();
    ui_pipeline  &Pipeline = Context.PipelineArray[static_cast<uint32_t>(Params.Pipeline)];

    // Memory
    {
        Pipeline.StateArena = AllocateArena({.ReserveSize = GetPipelineStateFootprint(Params)});
        Pipeline.FrameArena = AllocateArena({.ReserveSize = Params.FrameBudget});

        VOID_ASSERT(Pipeline.StateArena && Pipeline.FrameArena);
    }

    // UI State
    {
        uint64_t TreeFootprint = GetLayoutTreeFootprint(Params.NodeCount);
        void    *TreeMemory    = PushArena(Pipeline.StateArena, TreeFootprint, GetLayoutTreeAlignment());

        Pipeline.Tree = PlaceLayoutTreeInMemory(Params.NodeCount, TreeMemory);

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


static ui_pipeline &
UIBindPipeline(UIPipeline UserPipeline)
{
    void_context &Context  = GetVoidContext();
    ui_pipeline  &Pipeline = Context.PipelineArray[static_cast<uint32_t>(UserPipeline)];

    if(!Pipeline.Bound)
    {
        if(Pipeline.FrameArena)
        {
            PopArenaTo(Pipeline.FrameArena, 0);
        }

        Pipeline.Bound = true;
    }

    return Pipeline;
}


static void
UIUnbindPipeline(UIPipeline UserPipeline)
{
    void_context &Context  = GetVoidContext();
    ui_pipeline  &Pipeline = Context.PipelineArray[static_cast<uint32_t>(UserPipeline)];

    if(Pipeline.Bound)
    {
        ComputeTreeLayout(Pipeline.Tree);

        ui_paint_buffer Buffer = GeneratePaintBuffer(Pipeline.Tree, Pipeline.StyleArray, Pipeline.FrameArena);
        if(Buffer.Commands && Buffer.Size)
        {
            ExecutePaintCommands(Buffer, Pipeline.FrameArena);
        }

        Pipeline.Bound = false;
    }
}


static ui_pipeline_params
UIGetDefaultPipelineParams(void)
{
    ui_pipeline_params Params =
    {
        .VtxShaderByteCode = byte_string(0, 0),
        .PxlShaderByteCode = byte_string(0, 0),
    };

    return Params;
}
