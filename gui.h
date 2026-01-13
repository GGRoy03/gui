#ifndef GUI_SINGLE_HEADER_H
#define GUI_SINGLE_HEADER_H

#ifdef __cplusplus
extern "C" {
#endif


//-----------------------------------------------------------------------------
// TODO-LIST
//-----------------------------------------------------------------------------


#include <stdint.h>


#ifndef GUI_API
#define GUI_API extern
#endif


//-----------------------------------------------------------------------------
// [SECTION] GUI BASIC PRIMITIVES
// [DESCRIP] ...
// [HISTORY]
// : - 2026-01-11 Basic Implementation
//-----------------------------------------------------------------------------


typedef uint32_t gui_bool;
#define GUI_TRUE  (1u)
#define GUI_FALSE (0u)


typedef struct gui_node
{
    uint32_t Value;
} gui_node;


typedef struct gui_point
{
    float X, Y;
} gui_point;


typedef struct gui_dimensions
{
    float Width, Height;
} gui_dimensions;


typedef struct gui_bounding_box
{
    float Left;
    float Top;
    float Right;
    float Bottom;
} gui_bounding_box;


typedef struct gui_string
{
    uint8_t *String;
    uint64_t Size;
} gui_string;


#define GuiStringLit(String)   GuiByteString((char *)(String), sizeof(String) - 1)
#define GuiStringComp(String)  ((gui_byte_string){ (char *)(String), sizeof(String) - 1 })


/*-----------------------------------------------------------------------------
// [SECTION] FORWARD DECLARATION MESS
// [DESCRIP] ...
// [HISTORY]
// : - 2026-01-11 Basic Implementation
//-----------------------------------------------------------------------------*/


typedef struct gui_resource_table gui_resource_table;
typedef struct gui_pointer_event_node gui_pointer_event_node;
typedef struct gui_pointer_event_list gui_pointer_event_list;
typedef struct gui_layout_tree        gui_layout_tree;


//-----------------------------------------------------------------------------
// [SECTION] GUI MEMORY API
// [DESCRIP] ...
// [HISTORY]
// : - 2026-01-11 Basic Implementation
//-----------------------------------------------------------------------------


typedef enum
{
    Gui_MemoryAllocation_None = 0,

    Gui_MemoryAllocation_Persistent = 1,
    Gui_MemoryAllocation_Transient  = 2,
} Gui_MemoryAllocation_Lifetime;


typedef struct gui_memory_footprint
{
    uint64_t                      SizeInBytes;
    uint64_t                      Alignment;
    Gui_MemoryAllocation_Lifetime Lifetime;
} gui_memory_footprint;


typedef struct gui_memory_block
{
    uint64_t SizeInBytes;
    void    *Base;
} gui_memory_block;


//-----------------------------------------------------------------------------
// [SECTION] GUI INPUT API
// [DESCRIP] ...
// [HISTORY]
// : - 2026-01-11 Basic Implementation
//-----------------------------------------------------------------------------


typedef enum Gui_PointerSource
{
    Gui_PointerSource_None = 0,

    Gui_PointerSource_Mouse = 1,
} Gui_PointerSource;


typedef enum Gui_PointerButton
{
    Gui_PointerButton_None      = 0,
    Gui_PointerButton_Primary   = 1,
    Gui_PointerButton_Secondary = 2,
} Gui_PointerButton;


typedef enum Gui_PointerEvent
{
    Gui_PointerEvent_None    = 0,
    Gui_PointerEvent_Move    = 1,
    Gui_PointerEvent_Click   = 2,
    Gui_PointerEvent_Release = 3,
} Gui_PointerEvent;


typedef struct gui_pointer_event
{
    Gui_PointerEvent  Type;
    uint32_t          PointerId;
    gui_point         Position;
    gui_dimensions    Delta;
    Gui_PointerButton ButtonMask;
} gui_pointer_event;


typedef struct gui_pointer_event_node gui_pointer_event_node;
struct gui_pointer_event_node
{
    gui_pointer_event_node *Next;
    gui_pointer_event       Value;
};


typedef struct gui_pointer_event_list
{
    gui_pointer_event_node *First;
    gui_pointer_event_node *Last;
    uint32_t                Count;
} gui_pointer_event_list;


GUI_API gui_bool GuiPushPointerMoveEvent     (gui_point Position, gui_point LastPosition, gui_pointer_event_node *Node, gui_pointer_event_list *List);
GUI_API gui_bool GuiPushPointerClickEvent    (Gui_PointerButton Button, gui_point Position, gui_pointer_event_node *Node, gui_pointer_event_list *List);
GUI_API gui_bool GuiPushPointerReleaseEvent  (Gui_PointerButton Button, gui_point Position, gui_pointer_event_node *Node, gui_pointer_event_list *List);


//-----------------------------------------------------------------------------
// [SECTION] GUI RESOURCE API
// [DESCRIP] ...
// [HISTORY]
// : - 2026-01-12 Basic Implementation
//-----------------------------------------------------------------------------


typedef enum Gui_ResourceType
{
    Gui_ResourceType_None = 0,
} Gui_ResourceType;


typedef struct gui_resource_key
{
    uint64_t Value;
} gui_resource_key;


typedef struct gui_resource_state
{
    uint32_t          Id;
    Gui_ResourceType  ResourceType;
    void             *Resource;
} gui_resource_state;


typedef struct gui_resource_stats
{
    uint64_t CacheHitCount;
    uint64_t CacheMissCount;
} gui_resource_stats;


typedef struct gui_resource_table_params
{
    uint32_t HashSlotCount;
    uint32_t EntryCount;
} gui_resource_table_params;


GUI_API gui_memory_footprint   GuiGetResourceTableFootprint   (gui_resource_table_params Params);
GUI_API gui_resource_table   * GuiPlaceResourceTableInMemory  (gui_resource_table_params Params, gui_memory_block Block);


GUI_API gui_resource_state     GuiFindResourceByKey           (gui_resource_key Key, gui_resource_table *Table);
GUI_API void                   GuiUpdateResourceTable         (uint32_t Id, gui_resource_key Key, void *Resource, uint64_t ResourceSize, Gui_ResourceType Type, gui_resource_table *Table);
GUI_API gui_resource_stats     GuiGetResourceStats            (gui_bool ClearStats, gui_resource_table *Table);


//-----------------------------------------------------------------------------
// [SECTION] GUI LAYOUT API
// [DESCRIP] ...
// [HISTORY]
// : - 2026-01-11 Basic Implementation
//-----------------------------------------------------------------------------

typedef struct gui_cached_style gui_cached_style;


typedef enum Gui_NodeFlags
{
    Gui_NodeFlags_None        = 0,
    Gui_NodeFlags_IsDraggable = 1 << 0,
    Gui_NodeFlags_IsResizable = 1 << 1,
    Gui_NodeFlags_ClipContent = 1 << 3,
} Gui_NodeFlags;


typedef enum Gui_Alignment
{
    Gui_Alignment_None   = 0,
    Gui_Alignment_Start  = 1,
    Gui_Alignment_Center = 2,
    Gui_Alignment_End    = 3,
} Gui_Alignment;


typedef enum Gui_LayoutDirection
{
    Gui_LayoutDirection_None       = 0,
    Gui_LayoutDirection_Horizontal = 1,
    Gui_LayoutDirection_Vertical   = 2,
} Gui_LayoutDirection;


typedef enum Gui_LayoutSizing
{
    Gui_LayoutSizing_None    = 0,
    Gui_LayoutSizing_Fixed   = 1,
    Gui_LayoutSizing_Percent = 2,
    Gui_LayoutSizing_Fit     = 3,
} Gui_LayoutSizing;


typedef struct gui_sizing
{
    float            Value;
    Gui_LayoutSizing Type;
} gui_sizing;


typedef struct gui_size
{
    gui_sizing  Width;
    gui_sizing  Height;
} gui_size;


typedef struct gui_padding
{
    float Left, Top, Right, Bottom;
} gui_padding;


typedef struct gui_layout_properties
{
    gui_size             Size;
    gui_size             MinSize;
    gui_size             MaxSize;

    Gui_LayoutDirection  Direction;
    Gui_Alignment        XAlign;
    Gui_Alignment        YAlign;

    gui_padding          Padding;
    float                Spacing;

    float                Grow;
    float                Shrink;
} gui_layout_properties;


typedef struct gui_parent_node gui_parent_node;
struct gui_parent_node
{
    gui_parent_node *Prev;
    uint32_t         Value;
};


GUI_API gui_memory_footprint GuiGetLayoutTreeFootprint   (uint32_t NodeCount);
GUI_API gui_layout_tree    * GuiPlaceLayoutTreeInMemory  (uint32_t NodeCount, gui_memory_block Block);


GUI_API gui_node             GuiCreateNode               (uint64_t Key, uint32_t Flags, gui_layout_tree *Tree);
GUI_API void                 GuiUpdateLayout             (gui_node Node, gui_layout_properties *Properties, gui_layout_tree *Tree);


GUI_API gui_bool             GuiEnterParent              (gui_node Node, gui_layout_tree *Tree, gui_parent_node *ParentNode);
GUI_API void                 GuiLeaveParent              (gui_node Node, gui_layout_tree *Tree);


GUI_API gui_bool             GuiAppendChild              (uint32_t ParentIndex, uint32_t ChildIndex, gui_layout_tree *Tree);
GUI_API uint32_t             GuiFindChild                (gui_node Node, uint32_t FindIndex, gui_layout_tree *Tree);

GUI_API void                 GuiComputeTreeLayout        (gui_layout_tree *Tree);


//-----------------------------------------------------------------------------
// [SECTION] GUI PAINTING API
// [DESCRIP] ...
// [HISTORY]
// : - 2026-01-12 Basic Implementation
//-----------------------------------------------------------------------------


typedef enum Gui_RenderCommandType
{
    Gui_RenderCommandType_None = 0,
    Gui_RenderCommandType_End  = 0,

    Gui_RenderCommandType_Rectangle = 1,
    Gui_RenderCommandType_Border    = 2,
    Gui_RenderCommandType_Text      = 3,
} GuiRenderCommandType;


typedef struct gui_color
{
    float R, G, B, A;
} gui_color;


typedef struct gui_corner_radius
{
    float TL, TR, BR, BL;
} gui_corner_radius;


typedef struct gui_paint_style
{
    gui_color         Color;
    gui_color         BorderColor;
    float             BorderWidth;
    gui_corner_radius CornerRadius;
} gui_paint_style;


typedef struct gui_paint_properties
{
    gui_paint_style Default;
    gui_paint_style Hovered;
    gui_paint_style Focused;
} gui_paint_properties;


typedef struct gui_render_command
{
    GuiRenderCommandType Type;
    gui_bounding_box     Box;

    union
    {
        struct
        {
            gui_color         Color;
            gui_corner_radius CornerRadius;
        } Rect;

        struct
        {
            gui_corner_radius CornerRadius;
            gui_color         Color;
            float             Width;
        } Border;

        struct
        {
            void             *Texture;
            gui_color         Color;
            gui_bounding_box  Source;
        } Text;
    };
} gui_render_command;


typedef struct gui_render_command_params
{
    uint32_t Count;
} gui_render_command_params;


GUI_API gui_color              GuiColorFromRGB8               (uint8_t R, uint8_t G, uint8_t B, uint8_t A);
GUI_API void                   GuiUpdateStyle                 (gui_node Node, gui_paint_properties *Properties, gui_layout_tree *Tree);

GUI_API uint32_t               GuiGetRenderCommandCount       (gui_render_command_params Params, gui_layout_tree *Tree);
GUI_API gui_memory_footprint   GuiGetRenderCommandsFootprint  (gui_render_command_params Params, gui_layout_tree *Tree);
GUI_API gui_render_command   * GuiComputeRenderCommands       (gui_render_command_params Params, gui_layout_tree *Tree, gui_memory_block Block);


//-----------------------------------------------------------------------------
// [SECTION] GUI CONTEXT API
// [DESCRIP] ...
// [HISTORY]
// : - 2026-01-11 Basic Implementation
//-----------------------------------------------------------------------------


GUI_API void GuiBeginFrame  (gui_pointer_event_list *EventList, gui_layout_tree *Tree);
GUI_API void GuiEndFrame    (void);


// #define GUI_IMPLEMENTATION
#ifdef GUI_IMPLEMENTATION


//-----------------------------------------------------------------------------
// [SECTION] BASE MACROS/HELPERS FOR INTERNAL USE
// [DESCRIP] ...
// [HISTORY]
// : - 2026-01-11 Basic Implementation
//-----------------------------------------------------------------------------

#if defined(_MSC_VER)
    #define GUI_MSVC 1
#elif defined(__clang__)
    #define GUI_CLANG 1
#elif defined(__GNUC__)
    #define GUI_GCC 1
#else
    #error "GUI: Unknown Compiler"
#endif


#if GUI_MSVC || GUI_CLANG
    #define GUI_ALIGN_OF(T) __alignof(T)
#elif GUI_GCC
    #define GUI_ALIGN_OF(T) __alignof__(T)
#endif

#if GUI_MSVC
    #define GUI_FIND_FIRST_BIT(Mask) _tzcnt_u32(Mask)
#elif GUI_CLANG || GUI_GCC
    #define GUI_FIND_FIRST_BIT(Mask) __builtin_ctz(Mask)
#endif

#if defined(GUI_MSVC)
    #define GUI_DEBUGBREAK() __debugbreak()
#elif defined(GUI_CLANG) || defined(GUI_GCC)
    #define GUI_DEBUGBREAK() __builtin_trap()
#endif

#define GUI_ASSERT(Cond)        do { if (!(Cond))  GUI_DEBUGBREAK(); } while (0)
#define GUI_UNUSED(X)           (void)(X)


#define GUI_KILOBYTE(n)         (((uint64_t)(n)) << 10)
#define GUI_MEGABYTE(n)         (((uint64_t)(n)) << 20)
#define GUI_GIGABYTE(n)         (((uint64_t)(n)) << 30)

#define GUI_ARRAYCOUNT(A)       (sizeof(A) / sizeof(A[0]))
#define GUI_ISPOWEROFTWO(Value) ((Value) != 0 && (((Value) & ((Value) - 1)) == 0))
#define GUI_ALIGN_POW2(x,b)     (((x) + (b) - 1)&(~((b) - 1)))

//-----------------------------------------------------------------------------
// [SECTION] MEMORY MANAGEMENT INTERNAL TYPES
// [DESCRIP] ...
// [HISTORY]
// : - 2026-01-11 Basic Implementation
//-----------------------------------------------------------------------------


typedef struct gui_memory_region
{
    void    *Base;
    uint64_t Size;
    uint64_t At;
} gui_memory_region;


//-----------------------------------------------------------------------------
// [SECTION] MEMORY MANAGEMENT INTERNAL IMPLEMENTATION
// [DESCRIP] ...
// [HISTORY]
// : - 2026-01-11 Basic Implementation
//-----------------------------------------------------------------------------


static gui_bool
GuiIsValidMemoryRegion(gui_memory_region *Region)
{
    gui_bool Result = Region && Region->Base && Region->Size && Region->At <= Region->Size;
    return Result;
}


static gui_memory_region
GuiEnterMemoryRegion(gui_memory_block Block)
{
    gui_memory_region Region;
    Region.Base = Block.Base;
    Region.Size = Block.SizeInBytes;
    Region.At   = 0;
    return Region;
}


static void *
GuiPushMemoryRegion(gui_memory_region *Region, uint64_t Size, uint64_t Alignment)
{
    void *Result = 0;

    uint64_t Before = GUI_ALIGN_POW2(Region->At, Alignment);
    uint64_t After  = Before + Size;

    if(After <= Region->Size)
    {
        Result = (uint8_t *)Region->Base + Before;
        Region->At = After;
    }

    return Result;
}


#define GuiPushArrayNoZeroAligned(Region, Type, Count, Align) ((Type *)GuiPushMemoryRegion((Region), sizeof(Type) * (Count), (Align)))
#define GuiPushArrayAligned(Region, Type, Count, Align)                GuiPushArrayNoZeroAligned((Region), Type, (Count), (Align))
#define GuiPushArray(Region, Type, Count)                              GuiPushArrayAligned((Region), Type, (Count), _Alignof(Type))
#define GuiPushStruct(Region, Type)                                    GuiPushArray((Region), Type, 1)


//-----------------------------------------------------------------------------
// [SECTION] INPUTS INTERNAL TYPES
// [DESCRIP] ...
// [HISTORY]
// : - 2026-01-11 Basic Implementation
//-----------------------------------------------------------------------------


typedef struct gui_pointer_state
{
    uint32_t  Id;
    gui_point Position;
    gui_point LastPosition;
    uint32_t  ButtonMask;
    gui_bool  IsCaptured;
} gui_pointer_state;


//-----------------------------------------------------------------------------
// [SECTION] INPUTS MISC HELPERS
// [DESCRIP] ...
// [HISTORY]
// : - 2026-01-11 Basic Implementation
//-----------------------------------------------------------------------------


static gui_bool
GuiPushPointerEvent(Gui_PointerEvent Type, gui_pointer_event_node *Node, gui_pointer_event_list *List)
{
    gui_bool Pushed = GUI_FALSE;

    if(List && Node && Type != Gui_PointerEvent_None)
    {
        Node->Next = 0;
        Node->Value.Type = Type;

        if(!List->First)
        {
            List->First = Node;
            List->Last  = Node;
        }
        else
        {
            GUI_ASSERT(List->Last);

            List->Last->Next = Node;
            List->Last       = Node;
        }

        ++List->Count;

        Pushed = GUI_TRUE;
    }

    return Pushed;
}


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


//-----------------------------------------------------------------------------
// [SECTION] INPUTS PUBLIC API IMPLEMENTATION
// [DESCRIP] ...
// [HISTORY]
// : - 2026-01-11 Basic Implementation
//-----------------------------------------------------------------------------


GUI_API gui_bool
GuiPushPointerMoveEvent(gui_point Position, gui_point LastPosition, gui_pointer_event_node *Node, gui_pointer_event_list *List)
{
    gui_bool Pushed = GUI_FALSE;

    if(Node)
    {
        Node->Value.Position = Position;
        Node->Value.Delta    = (gui_dimensions){Position.X - LastPosition.X, Position.Y - LastPosition.Y};

        Pushed = GuiPushPointerEvent(Gui_PointerEvent_Move, Node, List);
    }

    return Pushed;
}


GUI_API gui_bool
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


GUI_API gui_bool
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

//-----------------------------------------------------------------------------
// [SECTION] RESOURCES INTERNAL TYPES
// [DESCRIP] ...
// [HISTORY]
// : - 2026-01-11 Basic Implementation
//-----------------------------------------------------------------------------


typedef struct gui_resource_entry
{
    gui_resource_key Key;

    uint32_t         NextWithSameHashSlot;
    uint32_t         NextLRU;
    uint32_t         PrevLRU;

    Gui_ResourceType ResourceType;
    void            *Memory;
    uint64_t         MemorySize;
} gui_resource_entry;


typedef struct gui_resource_table
{
    gui_resource_stats     Stats;

    uint32_t               HashMask;
    uint32_t               HashSlotCount;
    uint32_t               EntryCount;

    uint32_t              *HashTable;
    gui_resource_entry    *Entries;
} gui_resource_table;


//-----------------------------------------------------------------------------
// [SECTION] RESOURCES INTERNAL IMPLEMENTATION
// [DESCRIP] ...
// [HISTORY]
// : - 2026-01-12 Basic Implementation
//-----------------------------------------------------------------------------


static gui_resource_entry *
GuiGetResourceSentinel(gui_resource_table *Table)
{
    gui_resource_entry *Result = Table->Entries;
    return Result;
}


static uint32_t *
GuiGetResourceSlotPointer(gui_resource_key Key, gui_resource_table *Table)
{
    uint64_t HashIndex = Key.Value;
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


//-----------------------------------------------------------------------------
// [SECTION] RESOURCES PUBLIC API
// [DESCRIP] ...
// [HISTORY]
// : - 2026-01-12 Basic Implementation
//-----------------------------------------------------------------------------

// TODO: Align stuff.

GUI_API gui_memory_footprint
GuiGetResourceTableFootprint(gui_resource_table_params Params)
{
    uint64_t HashTableSize  = Params.HashSlotCount * sizeof(uint32_t);
    uint64_t EntryArraySize = Params.EntryCount    * sizeof(gui_resource_entry);

    gui_memory_footprint Result = 
    {
        .SizeInBytes = sizeof(gui_resource_table) + HashTableSize + EntryArraySize,
        .Alignment   = GUI_ALIGN_OF(gui_resource_entry),
        .Lifetime    = Gui_MemoryAllocation_Persistent,
    };

    return Result;
}


GUI_API gui_resource_table *
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

            for(uint32_t Idx = 0; Idx < Params.HashSlotCount; ++Idx)
            {
                Table->HashTable[Idx] = 0;
            }

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


GUI_API gui_resource_state
GuiFindResourceByKey(gui_resource_key Key, gui_resource_table *Table)
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
            if(Entry->Key.Value == Key.Value)
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
        else
        {
            // If we miss an entry we have to first allocate a new one.
            // If we have some hash slot at X: Hash[X]
            // Hash[X] -> (Index) -> (Index)
            // (Entry) -> Hash[X] -> (Index) -> (Index)
            // Hash[X] -> (Index) -> (Index) -> (Index)
        
            EntryIndex = GuiPopFreeResourceEntry(Table);
            GUI_ASSERT(EntryIndex);
        
            FoundEntry = GuiGetResourceEntry(EntryIndex, Table);
            FoundEntry->NextWithSameHashSlot = *Slot;
            FoundEntry->Key                  = Key;
        
            *Slot = EntryIndex;
        
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


GUI_API void
GuiUpdateResourceTable(uint32_t Id, gui_resource_key Key, void *Resource, uint64_t ResourceSize, Gui_ResourceType Type, gui_resource_table *Table)
{
    gui_resource_entry *Entry = GuiGetResourceEntry(Id, Table);
    GUI_ASSERT(Entry);

    Entry->Key          = Key;
    Entry->Memory       = Resource;
    Entry->MemorySize   = ResourceSize;
    Entry->ResourceType = Type;
}


GUI_API gui_resource_stats
GuiGetResourceStats(gui_bool ClearStats, gui_resource_table *Table)
{
    gui_resource_stats OldStats = Table->Stats;

    if(ClearStats)
    {
        Table->Stats = (gui_resource_stats){0};
    }

    return OldStats;
};


//-----------------------------------------------------------------------------
// [SECTION] LAYOUT INTERNAL TYPES
// [DESCRIP] All types used as part of the layout code that are useless to
//           the user.
// [HISTORY]
// : - 2026-01-11 Basic Implementation
//-----------------------------------------------------------------------------


#include <math.h>


static const uint32_t GuiInvalidIndex = 0xFFFFFFFFu;


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


typedef struct gui_layout_tree
{
    // Persistent State

    gui_layout_node        *Nodes;
    uint32_t                NodeCount;
    uint32_t                NodeCapacity;
    gui_paint_properties   *PaintBuffer;

    // Reference Map

    uint64_t                RefHashMask;
    uint64_t               *RefKeys;
    uint32_t               *RefValues;

    // Transient State

    uint32_t                RootIndex;
    uint32_t                CapturedNodeIndex;
    gui_parent_node        *Parent;
} gui_layout_tree;


//-----------------------------------------------------------------------------
// [SECTION] MISC LAYOUT HELPERS
// [DESCRIP] Various Node/Tree helpers
// [HISTORY]
// : - 2026-01-11 Basic Implementation
//-----------------------------------------------------------------------------


static gui_bool
GuiIsValidLayoutNode(gui_layout_node *Node)
{
    gui_bool Result = Node && (Node->Index != GuiInvalidIndex);
    return Result;
}


static gui_bool
GuiIsValidLayoutTree(gui_layout_tree *Tree)
{
    gui_bool Result = (Tree && Tree->Nodes && Tree->NodeCount <= Tree->NodeCapacity);
    return Result;
}


static gui_layout_node *
GuiGetSentinelNode(gui_layout_tree *Tree)
{
    GUI_ASSERT(GuiIsValidLayoutTree(Tree));

    gui_layout_node *Result = Tree->Nodes + Tree->NodeCapacity;
    return Result;
}


static gui_layout_node *
GuiGetLayoutNode(uint32_t Index, gui_layout_tree *Tree)
{
    GUI_ASSERT(GuiIsValidLayoutTree(Tree));

    gui_layout_node *Result = 0;

    if(Index < Tree->NodeCapacity)
    {
        Result = Tree->Nodes + Index;
    }

    return Result;
}


static gui_layout_node *
GuiGetFreeLayoutNode(gui_layout_tree *Tree)
{
    GUI_ASSERT(GuiIsValidLayoutTree(Tree));

    gui_layout_node *Sentinel = GuiGetSentinelNode(Tree);
    gui_layout_node *Result   = 0;

    GUI_ASSERT(Sentinel);

    if(Sentinel->Next != GuiInvalidIndex)
    {
        GUI_ASSERT(Tree->NodeCount < Tree->NodeCapacity);

        uint32_t FreeIndex = Sentinel->Next;
        Result = GuiGetLayoutNode(FreeIndex, Tree);
        GUI_ASSERT(Result);

        Sentinel->Next = Result->Next;

        Result->First      = GuiInvalidIndex;
        Result->Last       = GuiInvalidIndex;
        Result->Prev       = GuiInvalidIndex;
        Result->Next       = GuiInvalidIndex;
        Result->Parent     = GuiInvalidIndex;
        Result->ChildCount = 0;
        Result->Index      = FreeIndex;

        ++Tree->NodeCount;
    }

    return Result;
}


static void
GuiAppendLayoutNode(uint32_t ParentIndex, uint32_t ChildIndex, gui_layout_tree *Tree)
{
    gui_layout_node *Parent = GuiGetLayoutNode(ParentIndex, Tree);
    gui_layout_node *Child  = GuiGetLayoutNode(ChildIndex, Tree);

    if(GuiIsValidLayoutNode(Parent) && GuiIsValidLayoutNode(Child))
    {
        Child->First  = GuiInvalidIndex;
        Child->Last   = GuiInvalidIndex;
        Child->Prev   = GuiInvalidIndex;
        Child->Next   = GuiInvalidIndex;
        Child->Parent = Parent->Index;

        gui_layout_node *First = GuiGetLayoutNode(Parent->First, Tree);
        if(!GuiIsValidLayoutNode(First))
        {
            Parent->First = Child->Index;
        }

        gui_layout_node *Last  = GuiGetLayoutNode(Parent->Last, Tree);
        if(GuiIsValidLayoutNode(Last))
        {
            Last->Next  = Child->Index;
        }

        Child->Prev         = Parent->Last;
        Parent->Last        = Child->Index;
        Parent->ChildCount += 1;
    }
}


//-----------------------------------------------------------------------------
// [SECTION] NODE REFERENCES
// [DESCRIP] Functions to insert/retrieve nodes across frames.
// [HISTORY]
// : - 2026-01-11 Basic Implementation
//-----------------------------------------------------------------------------


static void
GuiInsertNodeReference(uint64_t Key, uint32_t Value, gui_layout_tree *Tree)
{
    uint64_t Slot = Key & Tree->RefHashMask;

    // TODO: This is a bug..
    for(;;)
    {
        uint64_t Stored = Tree->RefKeys[Slot];

        if(Stored == Key)
        {
            break;
        }

        // Should be some EmptyMarker.
        if(Stored == GuiInvalidIndex)
        {
            Tree->RefKeys[Slot]   = Key;
            Tree->RefValues[Slot] = Value;

            break;
        }

        Slot = (Slot + 1) & Tree->RefHashMask;
    }
}


static uint32_t
GuiFindNodeReference(uint64_t Key, gui_layout_tree *Tree)
{
    uint32_t Result = GuiInvalidIndex;
    uint64_t Slot   = Key & Tree->RefHashMask;

    // TODO: This is a bug..
    for(;;)
    {
        uint64_t Stored = Tree->RefKeys[Slot];
        
        // Should be some EmptyMarker.
        if(Stored == GuiInvalidIndex)
        {
            break;
        }

        if(Stored == Key)
        {
            Result = Tree->RefValues[Slot];
            break;
        }

        Slot = (Slot + 1) & Tree->RefHashMask;
    }

    return Result;
}


//-----------------------------------------------------------------------------
// [SECTION] LAYOUT GEOMETRY
// [DESCRIP] Basic layout geometry helpers.
// [HISTORY]
// : - 2026-01-11 Basic Implementation
//-----------------------------------------------------------------------------


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
GuiIsPointInsideOuterBox(gui_point Position, gui_layout_node *Node)
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


static gui_bool
GuiIsPointInsideBorder(gui_point Position, gui_layout_node *Node)
{
    gui_bool Result = GUI_FALSE;

    if(GuiIsValidLayoutNode(Node))
    {
        gui_bounding_box Box      = GuiGetLayoutNodeBoundingBox(Node);
        float            Distance = GuiBoundingBoxSignedDistanceField(Position, Box);

        Result = Distance >= 0.0f;
    }

    return Result;
}


//-----------------------------------------------------------------------------
// [SECTION] LAYOUT RESPONSIVENESS INPUT HANDLING
// [DESCRIP] When the user calls GuiBeginFrame we fire events at _some_ layout
//           tree. These functions are responsible for handling those events.
// [HISTORY]
// : - 2026-01-11 Basic Implementation
//-----------------------------------------------------------------------------


static gui_bool
GuiHandlePointerClick(gui_point Position, uint32_t ClickMask, uint32_t NodeIndex, gui_layout_tree *Tree)
{
    GUI_ASSERT(Position.X >= 0.0f && Position.Y >= 0.0f);
    GUI_ASSERT(GuiIsValidLayoutTree(Tree));

    if(GuiIsValidLayoutTree(Tree))
    {
        gui_layout_node *Node = GuiGetLayoutNode(NodeIndex, Tree);
        GUI_ASSERT(GuiIsValidLayoutNode(Node));

        if(GuiIsValidLayoutNode(Node))
        {
            if(GuiIsPointInsideOuterBox(Position, Node))
            {
                for(gui_layout_node *Child = GuiGetLayoutNode(Node->First, Tree); GuiIsValidLayoutNode(Child); Child = GuiGetLayoutNode(Child->Next, Tree))
                {
                    gui_bool IsHandled = GuiHandlePointerClick(Position, ClickMask, Child->Index, Tree);
                    if(IsHandled)
                    {
                        return GUI_TRUE;
                    }
                }

                Node->State = Node->State | Gui_NodeState_IsClicked;
                Node->State = Node->State | Gui_NodeState_UseFocusedStyle;
                Node->State = Node->State | Gui_NodeState_HasCapturedPointer;

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
        gui_layout_node *Node = GuiGetLayoutNode(NodeIndex, Tree);
        GUI_ASSERT(GuiIsValidLayoutNode(Node));

        for(gui_layout_node *Child = GuiGetLayoutNode(Node->First, Tree); GuiIsValidLayoutNode(Child); Child = GuiGetLayoutNode(Child->Next, Tree))
        {
            if(GuiHandlePointerRelease(Position, ReleaseMask, Child->Index, Tree))
            {
                return GUI_TRUE;
            }
        }

        if(Node->State & Gui_NodeState_HasCapturedPointer)
        {
            Node->State = Node->State & ~(Gui_NodeState_HasCapturedPointer | Gui_NodeState_UseFocusedStyle);

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
        gui_layout_node *Node = GuiGetLayoutNode(NodeIndex, Tree);
        GUI_ASSERT(GuiIsValidLayoutNode(Node));

        if(GuiIsPointInsideOuterBox(Position, Node))
        {
            for(gui_layout_node *Child = GuiGetLayoutNode(Node->First, Tree); GuiIsValidLayoutNode(Child); Child = GuiGetLayoutNode(Child->Next, Tree))
            {
                gui_bool IsHandled = GuiHandlePointerHover(Position, Child->Index, Tree);
                if (IsHandled)
                {
                    return GUI_TRUE;
                }
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
        gui_layout_node *CapturedNode = GuiGetLayoutNode(Tree->CapturedNodeIndex, Tree);
        if(GuiIsValidLayoutNode(CapturedNode))
        {
            if(CapturedNode->Flags & Gui_NodeFlags_IsDraggable)
            {
                CapturedNode->OutputPosition.X += DeltaX;
                CapturedNode->OutputPosition.Y += DeltaY;
            }
        }
    }
}


//-----------------------------------------------------------------------------
// [SECTION] LAYOUT COMPUTATION CORE
// [DESCRIP] Convergence based layout algorithm implementation & helpers
// [HISTORY]
// : - 2026-01-11 Basic Implementation
//-----------------------------------------------------------------------------


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

    float Width     = GuiComputeNodeSize(Node->Size.Width    , ParentBounds.Width);
    float MinWidth  = GuiComputeNodeSize(Node->MinSize.Width , ParentBounds.Width);
    float MaxWidth  = GuiComputeNodeSize(Node->MaxSize.Width , ParentBounds.Width);
    float Height    = GuiComputeNodeSize(Node->Size.Height   , ParentBounds.Height);
    float MinHeight = GuiComputeNodeSize(Node->MinSize.Height, ParentBounds.Height);
    float MaxHeight = GuiComputeNodeSize(Node->MaxSize.Height, ParentBounds.Height);

    Node->OutputSize = (gui_dimensions){ .Width = fmaxf(MinWidth, fminf(Width, MaxWidth)), .Height = fmaxf(MinHeight, fminf(Height, MaxHeight)) };

    if((Node->OutputSize.Width != LastSize.Width) || (Node->OutputSize.Height != LastSize.Height))
    {
        if(Changed) *Changed = GUI_TRUE;
    }

    gui_dimensions ContentBounds = (gui_dimensions){.Width = Node->OutputSize.Width - (Node->Padding.Left + Node->Padding.Right), .Height = Node->OutputSize.Height - (Node->Padding.Top + Node->Padding.Bottom)};

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

    for(gui_layout_node *Child = GuiGetLayoutNode(Node->First, Tree); GuiIsValidLayoutNode(Child); Child = GuiGetLayoutNode(Child->Next, Tree))
    {
        GuiComputeLayout(Child, Tree, ContentBounds, ResourceTable, Changed);

        Node->OutputChildSize.Width  += Child->OutputSize.Width;
        Node->OutputChildSize.Height += Child->OutputSize.Height;
    }
}


static void
GuiPlaceLayout(gui_layout_node *Node, gui_layout_tree *Tree, gui_resource_table *ResourceTable)
{
    gui_point Cursor   = (gui_point){ .X = Node->OutputPosition.X + Node->Padding.Left, .Y = Node->OutputPosition.Y + Node->Padding.Top };
    gui_bool  IsXMajor = (Node->Direction == Gui_LayoutDirection_Horizontal);

    float MajorSize = IsXMajor ? Node->OutputSize.Width - (Node->Padding.Left + Node->Padding.Right) : Node->OutputSize.Height - (Node->Padding.Top + Node->Padding.Bottom);
    float MinorSize = IsXMajor ? Node->OutputSize.Height - (Node->Padding.Top + Node->Padding.Bottom) : Node->OutputSize.Width - (Node->Padding.Left + Node->Padding.Right);

    Gui_Alignment MajorAlignment    = IsXMajor ? Node->XAlign : Node->YAlign;
    float         MajorChildrenSize = IsXMajor ? Node->OutputChildSize.Width : Node->OutputChildSize.Height;
    float         MajorOffset       = GuiGetAlignmentOffset(MajorAlignment, MajorSize - MajorChildrenSize);

    if(IsXMajor)
    {
        Cursor.X += MajorOffset;
    }
    else
    {
        Cursor.Y += MajorOffset;
    }

    for(gui_layout_node *Child = GuiGetLayoutNode(Node->First, Tree); GuiIsValidLayoutNode(Child); Child = GuiGetLayoutNode(Child->Next, Tree))
    {
        Child->OutputPosition = Cursor;
        Gui_Alignment MinorAlign = IsXMajor ? Node->YAlign : Node->XAlign;
        float MinorOffset = GuiGetAlignmentOffset(MinorAlign, IsXMajor ? (MinorSize - Child->OutputSize.Height) : (MinorSize - Child->OutputSize.Width));

        if(IsXMajor)
        {
            Child->OutputPosition.Y += MinorOffset + Child->VisualOffset.Height;
            Cursor.X                += Child->OutputSize.Width + Node->Spacing;
        }
        else
        {
            Child->OutputPosition.X += MinorOffset + Child->VisualOffset.Width;
            Cursor.Y                += Child->OutputSize.Height + Node->Spacing;
        }

        GuiPlaceLayout(Child, Tree, ResourceTable);
    }
}

//-----------------------------------------------------------------------------
// [SECTION] PUBLIC LAYOUT API
// [DESCRIP] ...
// [HISTORY]
// : - 2026-01-11 Basic Implementation
//-----------------------------------------------------------------------------


GUI_API gui_memory_footprint
GuiGetLayoutTreeFootprint(uint32_t NodeCount)
{
    uint64_t TreeEnd       = sizeof(gui_layout_tree);

    uint64_t NodesStart    = GUI_ALIGN_POW2(TreeEnd, GUI_ALIGN_OF(gui_layout_node));
    uint64_t NodesEnd      = NodesStart + ((NodeCount + 1) * sizeof(gui_layout_node));

    uint64_t PaintStart    = GUI_ALIGN_POW2(NodesEnd, GUI_ALIGN_OF(gui_paint_properties));
    uint64_t PaintEnd      = PaintStart + (NodeCount * sizeof(gui_paint_properties));

    uint64_t RefKeyStart   = GUI_ALIGN_POW2(PaintEnd, GUI_ALIGN_OF(uint64_t));
    uint64_t RefKeyEnd     = RefKeyStart + (NodeCount * sizeof(uint64_t));

    uint64_t RefValueStart = GUI_ALIGN_POW2(RefKeyEnd, GUI_ALIGN_OF(uint32_t));
    uint64_t RefValueEnd   = RefValueStart + (NodeCount * sizeof(uint32_t));

    gui_memory_footprint Result =
    {
        .SizeInBytes = RefValueEnd,
        .Alignment   = GUI_ALIGN_OF(gui_layout_tree),
        .Lifetime    = Gui_MemoryAllocation_Persistent,
    };

    return Result;
}


GUI_API gui_layout_tree *
GuiPlaceLayoutTreeInMemory(uint32_t NodeCount, gui_memory_block Block)
{
    gui_layout_tree  *Result = 0;
    gui_memory_region Local  = GuiEnterMemoryRegion(Block);

    if(GuiIsValidMemoryRegion(&Local))
    {
        // ORDER IS IMPORTANT!
        gui_layout_tree      *Tree      = GuiPushStruct(&Local, gui_layout_tree);
        gui_layout_node      *Nodes     = GuiPushArray(&Local, gui_layout_node, NodeCount + 1);
        gui_paint_properties *Paint     = GuiPushArray(&Local, gui_paint_properties, NodeCount);
        uint64_t             *RefKeys   = GuiPushArray(&Local, uint64_t, NodeCount);
        uint32_t             *RefValues = GuiPushArray(&Local, uint32_t, NodeCount);

        if(Nodes && Paint && Tree && RefKeys && RefValues)
        {
            Tree->Nodes             = Nodes;
            Tree->NodeCount         = 0;
            Tree->NodeCapacity      = NodeCount;
            Tree->PaintBuffer       = Paint;
            Tree->RefKeys           = RefKeys;
            Tree->RefValues         = RefValues;
            Tree->RootIndex         = GuiInvalidIndex;
            Tree->CapturedNodeIndex = GuiInvalidIndex;
            Tree->Parent            = 0;
            Tree->RefHashMask       = NodeCount - 1;

            for(uint32_t Idx = 0; Idx < NodeCount; ++Idx)
            {
                Tree->RefKeys[Idx]   = GuiInvalidIndex;
                Tree->RefValues[Idx] = GuiInvalidIndex;
            }

            for(uint32_t Idx = 0; Idx < Tree->NodeCapacity; ++Idx)
            {
                gui_layout_node *Node = GuiGetLayoutNode(Idx, Tree);
                GUI_ASSERT(Node);

                Node->Index      = GuiInvalidIndex;
                Node->First      = GuiInvalidIndex;
                Node->Last       = GuiInvalidIndex;
                Node->Parent     = GuiInvalidIndex;
                Node->Prev       = GuiInvalidIndex;
                Node->ChildCount = 0;
                Node->Next       = (uint32_t)(Idx + 1);
            }

            gui_layout_node *Sentinel = GuiGetSentinelNode(Tree);
            Sentinel->Next  = 0;
            Sentinel->Index = GuiInvalidIndex;

            Result = Tree;
        }
    }

    return Result;
}


GUI_API gui_node
GuiCreateNode(uint64_t Key, uint32_t Flags, gui_layout_tree *Tree)
{
    gui_node Result = {.Value = GuiInvalidIndex};

    if(GuiIsValidLayoutTree(Tree))
    {
        uint32_t FoundIndex = GuiFindNodeReference(Key, Tree);
        if(FoundIndex == GuiInvalidIndex)
        {
            gui_layout_node *Node = GuiGetFreeLayoutNode(Tree);
            if(Node)
            {
                GuiInsertNodeReference(Key, Node->Index, Tree);

                FoundIndex = Node->Index;
            }
        }

        gui_layout_node *Node = GuiGetLayoutNode(FoundIndex, Tree);
        if(GuiIsValidLayoutNode(Node))
        {
            Node->ChildCount = 0;
            Node->Flags      = Flags;

            uint32_t ParentIndex = (Tree->Parent) ? Tree->Parent->Value : (uint32_t)GuiInvalidIndex;
            GuiAppendLayoutNode(ParentIndex, Node->Index, Tree);

            Result.Value = Node->Index;

            if (Tree->NodeCount == 1)
            {
                Tree->RootIndex = Node->Index;
            }
        }
    }

    return Result;
}


GUI_API void
GuiUpdateLayout(gui_node Node, gui_layout_properties *Properties, gui_layout_tree *Tree)
{
    if(GuiIsValidLayoutTree(Tree) && Properties)
    {
        gui_layout_node *LayoutNode = GuiGetLayoutNode(Node.Value, Tree);
        if(GuiIsValidLayoutNode(LayoutNode))
        {
            LayoutNode->Size      = Properties->Size;
            LayoutNode->MinSize   = Properties->MinSize;
            LayoutNode->MaxSize   = Properties->MaxSize;
            LayoutNode->Direction = Properties->Direction;
            LayoutNode->XAlign    = Properties->XAlign;
            LayoutNode->YAlign    = Properties->YAlign;
            LayoutNode->Padding   = Properties->Padding;
            LayoutNode->Spacing   = Properties->Spacing;

            // if(!Cached->Layout.MinSize.IsSet)
            // {
                // Node->MinSize = Node->Size;
            // }

            // if(!Cached->Layout.MaxSize.IsSet)
            // {
                // Node->MaxSize = Node->Size;
            // }
        }
    }
}


GUI_API gui_bool
GuiEnterParent(gui_node Node, gui_layout_tree *Tree, gui_parent_node *ParentNode)
{
    if(GuiIsValidLayoutTree(Tree) && ParentNode)
    {
        ParentNode->Prev  = Tree->Parent;
        ParentNode->Value = Node.Value;

        Tree->Parent = ParentNode;
    }

    return GUI_TRUE;
}


GUI_API void
GuiLeaveParent(gui_node Node, gui_layout_tree *Tree)
{
    GUI_UNUSED(Node.Value);

    if(GuiIsValidLayoutTree(Tree) && Tree->Parent)
    {
        Tree->Parent = Tree->Parent->Prev;
    }
}


GUI_API gui_bool
GuiAppendChild(uint32_t ParentIndex, uint32_t ChildIndex, gui_layout_tree *Tree)
{
    gui_bool Result = GUI_FALSE;

    if(GuiIsValidLayoutTree(Tree) && ParentIndex != ChildIndex)
    {
        GuiAppendLayoutNode(ParentIndex, ChildIndex, Tree);
        Result = GUI_TRUE;
    }

    return Result;
}


GUI_API uint32_t
GuiFindChild(gui_node Node, uint32_t FindIndex, gui_layout_tree *Tree)
{
    uint32_t Result = (uint32_t)GuiInvalidIndex;

    if(GuiIsValidLayoutTree(Tree))
    {
        gui_layout_node *LayoutNode = GuiGetLayoutNode(Node.Value, Tree);
        if(GuiIsValidLayoutNode(LayoutNode))
        {
            gui_layout_node *Child = GuiGetLayoutNode(LayoutNode->First, Tree);

            uint32_t Remaining = FindIndex;
            while(GuiIsValidLayoutNode(Child) && Remaining)
            {
                Child      = GuiGetLayoutNode(Child->Next, Tree);
                Remaining -= 1;
            }

            if(GuiIsValidLayoutNode(Child))
            {
                Result = Child->Index;
            }
        }
    }

    return Result;
}


GUI_API void
GuiComputeTreeLayout(gui_layout_tree *Tree)
{
    if(GuiIsValidLayoutTree(Tree))
    {
        gui_layout_node *ActiveRoot = GuiGetLayoutNode(Tree->RootIndex, Tree);

        while(GUI_TRUE)
        {
            gui_bool Changed = GUI_FALSE;
            GuiComputeLayout(ActiveRoot, Tree, ActiveRoot->OutputSize, 0, &Changed);
            if(!Changed) break;
        }

        GuiPlaceLayout(ActiveRoot, Tree, 0);
    }
}


//-----------------------------------------------------------------------------
// [SECTION] GUI PAINTING INTERNAL HELPERS
// [DESCRIP] ...
// [HISTORY]
// : - 2026-01-12 Basic Implementation
//-----------------------------------------------------------------------------


static uint32_t
GuiCountCommandForTree(uint32_t NodeIndex, gui_layout_tree *Tree)
{
    uint32_t Count = 0;

    gui_layout_node *Node = GuiGetLayoutNode(NodeIndex, Tree);
    GUI_ASSERT(Node);

    gui_paint_style *Style = &Tree->PaintBuffer[Node->Index].Default;

    if (Node->State & Gui_NodeState_UseFocusedStyle)
    {
        Style = &Tree->PaintBuffer[Node->Index].Focused;
    }
    else if (Node->State & Gui_NodeState_UseHoveredStyle)
    {
        Style = &Tree->PaintBuffer[Node->Index].Hovered;
    }

    gui_color Color       = Style->Color;
    float     BorderWidth = Style->BorderWidth;

    if (Color.A > 0.0f)
    {
        ++Count;
    }

    if (BorderWidth > 0.0f)
    {
        ++Count;
    }

    for (uint32_t Child = Node->First; Child != GuiInvalidIndex; Child = GuiGetLayoutNode(Child, Tree)->Next)
    {
        Count += GuiCountCommandForTree(Child, Tree);
    }

    return Count;
}


//-----------------------------------------------------------------------------
// [SECTION] GUI PAINTING API IMPLEMENTATION
// [DESCRIP] ...
// [HISTORY]
// : - 2026-01-12 Basic Implementation
//-----------------------------------------------------------------------------


GUI_API gui_color
GuiColorFromRGB8(uint8_t R, uint8_t G, uint8_t B, uint8_t A)
{
    gui_color Color = 
    {
        .R = (float)(R / (255.f)),
        .G = (float)(G / (255.f)),
        .B = (float)(B / (255.f)),
        .A = (float)(A / (255.f)),
    };

    return Color;
}


GUI_API void
GuiUpdateStyle(gui_node Node, gui_paint_properties *Properties, gui_layout_tree *Tree)
{
    if (GuiIsValidLayoutTree(Tree) && Properties)
    {
        gui_paint_properties *Resolved = &Tree->PaintBuffer[Node.Value];

        Resolved->Default.Color        = Properties->Default.Color;
        Resolved->Default.BorderColor  = Properties->Default.BorderColor;
        Resolved->Default.BorderWidth  = Properties->Default.BorderWidth;
        Resolved->Default.CornerRadius = Properties->Default.CornerRadius;

        Resolved->Hovered.Color        = Properties->Hovered.Color;
        Resolved->Hovered.BorderColor  = Properties->Hovered.BorderColor;
        Resolved->Hovered.BorderWidth  = Properties->Hovered.BorderWidth;
        Resolved->Hovered.CornerRadius = Properties->Hovered.CornerRadius;

        Resolved->Focused.Color        = Properties->Focused.Color;
        Resolved->Focused.BorderColor  = Properties->Focused.BorderColor;
        Resolved->Focused.BorderWidth  = Properties->Focused.BorderWidth;
        Resolved->Focused.CornerRadius = Properties->Focused.CornerRadius;
    }
}


GUI_API uint32_t
GuiGetRenderCommandCount(gui_render_command_params Params, gui_layout_tree *Tree)
{
    GUI_UNUSED(Params);

    uint32_t Result = GuiCountCommandForTree(Tree->RootIndex, Tree);
    return Result;
}


GUI_API gui_memory_footprint
GuiGetRenderCommandsFootprint(gui_render_command_params Params, gui_layout_tree *Tree)
{
    gui_memory_footprint Result = {0};

    if (GuiIsValidLayoutTree(Tree))
    {
        uint64_t CommandEnd = (Params.Count + 1) * sizeof(gui_render_command);

        uint64_t QueueStart = CommandEnd + GUI_ALIGN_POW2(CommandEnd, GUI_ALIGN_OF(uint64_t));
        uint64_t QueueEnd   = QueueStart + Tree->NodeCount * sizeof(uint32_t);

        Result.SizeInBytes = QueueEnd;
        Result.Alignment   = GUI_ALIGN_OF(gui_render_command);
        Result.Lifetime    = Gui_MemoryAllocation_Transient;
    }

    return Result;
}


GUI_API gui_render_command *
GuiComputeRenderCommands(gui_render_command_params Params, gui_layout_tree *Tree, gui_memory_block Block)
{
    gui_render_command *Result = 0;
    gui_memory_region   Local  = GuiEnterMemoryRegion(Block);

    if (GuiIsValidLayoutTree(Tree) && GuiIsValidMemoryRegion(&Local))
    {
        gui_render_command *Commands  = GuiPushArray(&Local, gui_render_command, Params.Count);
        uint32_t           *NodeQueue = GuiPushArray(&Local, uint32_t, Tree->NodeCount);

        if(Commands && NodeQueue)
        {
            uint32_t CommandCount = 0;
            uint32_t Head         = 0;
            uint32_t Tail         = 0;
            
            NodeQueue[Tail++] = Tree->RootIndex;
            
            while (Head < Tail && CommandCount < Params.Count)
            {
                uint32_t         NodeIndex = NodeQueue[Head++];
                gui_layout_node *Node      = GuiGetLayoutNode(NodeIndex, Tree);
                GUI_ASSERT(Node);
            
                for (uint32_t Child = Node->First; Child != GuiInvalidIndex; Child = GuiGetLayoutNode(Child, Tree)->Next)
                {
                    NodeQueue[Tail++] = Child;
                }
            
                gui_paint_style *Style = &Tree->PaintBuffer[Node->Index].Default;
            
                if (Node->State & Gui_NodeState_UseFocusedStyle)
                {
                    Style = &Tree->PaintBuffer[Node->Index].Focused;
                }
                else if (Node->State & Gui_NodeState_UseHoveredStyle)
                {
                    Style = &Tree->PaintBuffer[Node->Index].Hovered;
                }

                gui_color         Color        = Style->Color;
                gui_color         BorderColor  = Style->BorderColor;
                float             BorderWidth  = Style->BorderWidth;
                gui_corner_radius CornerRadius = Style->CornerRadius;
            
                if (Color.A > 0.0f)
                {
                    gui_render_command *Command = &Commands[CommandCount++];
            
                    Command->Type = Gui_RenderCommandType_Rectangle;
                    Command->Box  = GuiGetLayoutNodeBoundingBox(Node);
            
                    Command->Rect.Color        = Color;
                    Command->Rect.CornerRadius = CornerRadius;
                }
            
                if (BorderWidth > 0.0f)
                {
                    gui_render_command *Command = &Commands[CommandCount++];
            
                    Command->Type = Gui_RenderCommandType_Border;
                    Command->Box  = GuiGetLayoutNodeBoundingBox(Node);
            
                    Command->Border.Color        = BorderColor;
                    Command->Border.CornerRadius = CornerRadius;
                    Command->Border.Width        = BorderWidth;
                }
            }

            Commands[CommandCount] = (gui_render_command){.Type = Gui_RenderCommandType_End};
        }

        Result = Commands;
    }

    return Result;
}


//-----------------------------------------------------------------------------
// [SECTION] GUI CONTEXT API IMPLEMENTATION
// [DESCRIP] ...
// [HISTORY]
// : - 2026-01-11 Basic Implementation
//-----------------------------------------------------------------------------


GUI_API void
GuiBeginFrame(gui_pointer_event_list *EventList, gui_layout_tree *Tree)
{
    // Temporary barrier
    if (!GuiIsValidLayoutTree(Tree) || Tree->RootIndex == GuiInvalidIndex)
    {
        return;
    }

    for (uint32_t NodeIdx = 0; NodeIdx < Tree->NodeCapacity; ++NodeIdx)
    {
        gui_layout_node *Node = GuiGetLayoutNode(NodeIdx, Tree);
        GUI_ASSERT(Node);

        // This might be dangerous, maybe do not clear all of the state, but as much as we can.
        Node->State = 0;
    }

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
                float DeltaX = Event.Delta.Width;
                float DeltaY = Event.Delta.Height;
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
            GuiHandlePointerHover(State.Position, Tree->RootIndex, Tree);
        }
    }

    GuiClearPointerEvents(EventList);
}


GUI_API void
GuiEndFrame(void)
{
    // TODO: Unsure
}

#endif // GUI_IMPLEMENTATION


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GUI_SINGLE_HEADER_H */
