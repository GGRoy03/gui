typedef struct glyph_entry
{
	glyph_hash HashValue;

	cim_u32 NextWithSameHash;
	cim_u32 NextLRU;
	cim_u32 PrevLRU;

	bool       IsInAtlas;      // replace this probably?
	cim_f32    U0, V0, U1, V1;
	glyph_size Size;
} glyph_entry;

typedef struct glyph_table_stats
{
	size_t HitCount;
	size_t MissCount;
	size_t RecycleCount;
} glyph_table_stats;

typedef struct glyph_table
{
	glyph_table_stats Stats;

	cim_u32 HashMask;
	cim_u32 HashCount;
	cim_u32 EntryCount;

	cim_u32     *HashTable;
	glyph_entry *Entries;
} glyph_table;

static glyph_entry *
GetGlyphEntry(glyph_table *Table, cim_u32 Index)
{
	Cim_Assert(Index < Table->EntryCount);
	glyph_entry *Result = Table->Entries + Index;
	return Result;
}

static cim_u32*
GetSlotPointer(glyph_table *Table, glyph_hash Hash)
{
	cim_u32 HashIndex = Hash.Value;
	cim_u32 HashSlot  = (HashIndex & Table->HashMask);

	Cim_Assert(HashSlot < Table->HashCount);
	cim_u32*Result = &Table->HashTable[HashSlot];

	return Result;
}

static glyph_entry *
GetSentinel(glyph_table *Table)
{
	glyph_entry *Result = Table->Entries;
	return Result;
}

static glyph_table_stats 
GetAndClearStats(glyph_table *Table)
{
	glyph_table_stats Result    = Table->Stats;
	glyph_table_stats ZeroStats = {};

	Table->Stats = ZeroStats;

	return Result;
}

// NOTE: I know fuck all about hashing, i'll just use this hash for now? I don't know.
// Anyways, I'll see hash collisions on screen quite easily while we settle on a better design.
// We might just drop the LRU?
static glyph_hash
ComputeGlyphHash(cim_u32 CodePoint)
{
	glyph_hash Result = {};
	Result.Value = CimHash_Block32(&CodePoint, sizeof(CodePoint));

	return Result;
}

static void
UpdateGlyphCacheEntry(glyph_table *Table, cim_u32 Id, bool NewIsInAtlas, cim_f32 U0, cim_f32 V0, cim_f32 U1, cim_f32 V1, glyph_size NewSize)
{
	glyph_entry *Entry = GetGlyphEntry(Table, Id);
	Entry->IsInAtlas = NewIsInAtlas;
	Entry->U0        = U0;
	Entry->V0        = V0;
	Entry->U1        = U1;
	Entry->V1        = V1;
	Entry->Size      = NewSize;
}

#if DEBUG_VALIDATE_LRU
static void ValidateLRU(glyph_table *Table, int ExpectedCountChange)
{
	cim_u32 EntryCount = 0;

	glyph_entry *Sentinel     = GetSentinel(Table);
	size_t       LastOrdering = Sentinel->Ordering;
	for (cim_u32 EntryIndex = Sentinel->NextLRU; EntryIndex != 0;)
	{
		glyph_entry *Entry = GetGlyphEntry(Table, EntryIndex);
		Cim_Assert(Entry->Ordering < LastOrdering);

		LastOrdering = Entry->Ordering;
		EntryIndex   = Entry->NextLRU;

		++EntryCount;
	}

	if ((Table->LastLRUCount + ExpectedCountChange) != EntryCount)
	{
		__debugbreak();
	}

	Table->LastLRUCount = EntryCount;
}
#else
#define ValidateLRU(...)
#endif

static void 
RecycleLRU(glyph_table *Table)
{
	glyph_entry *Sentinel = GetSentinel(Table);
	Cim_Assert(Sentinel->PrevLRU);

	cim_u32      EntryIndex = Sentinel->PrevLRU;
	glyph_entry *Entry      = GetGlyphEntry(Table, EntryIndex);
	glyph_entry *Prev       = GetGlyphEntry(Table, Entry->PrevLRU);

	Prev->NextLRU     = 0;
	Sentinel->PrevLRU = Entry->PrevLRU;
	ValidateLRU(Table, -1);

	cim_u32 *NextIndex = GetSlotPointer(Table, Entry->HashValue);
	while (*NextIndex != EntryIndex)
	{
		Cim_Assert(*NextIndex);
		NextIndex = &GetGlyphEntry(Table, *NextIndex)->NextWithSameHash;
	}

	Cim_Assert(*NextIndex == EntryIndex);
	*NextIndex = Entry->NextWithSameHash;

	Entry->NextWithSameHash    = Sentinel->NextWithSameHash;
	Sentinel->NextWithSameHash = EntryIndex;

	UpdateGlyphCacheEntry(Table, EntryIndex, false, 0.0f, 0.0f, 0.0f, 0.0f, {});

	++Table->Stats.RecycleCount;
}

static cim_u32 
PopFreeEntry(glyph_table *Table)
{
	glyph_entry *Sentinel = GetSentinel(Table);

	if (!Sentinel->NextWithSameHash)
	{
		RecycleLRU(Table);
	}

	cim_u32 Result = Sentinel->NextWithSameHash;
	Cim_Assert(Result);

	glyph_entry *Entry = GetGlyphEntry(Table, Result);
	Sentinel->NextWithSameHash = Entry->NextWithSameHash;
	Entry->NextWithSameHash    = 0;

	Cim_Assert(Entry);
	Cim_Assert(Entry != Sentinel);
	Cim_Assert(Entry->NextWithSameHash == 0);
	Cim_Assert(Entry->U0 == 0);
	Cim_Assert(Entry->V0 == 0);
	Cim_Assert(Entry->U1 == 0);
	Cim_Assert(Entry->V1 == 0);
	Cim_Assert(Entry == GetGlyphEntry(Table, Result));

	return Result;
}

static glyph_info 
FindGlyphEntryByHash(glyph_table *Table, glyph_hash Hash)
{
    glyph_entry *Result = 0;

    cim_u32 *Slot       = GetSlotPointer(Table, Hash);
    cim_u32  EntryIndex = *Slot;
    while (EntryIndex)
    {
		// NOTE: Hash collisions are fatal. I mean we do 128 bits hashes. Surely it's fine.
		// But at the same time, I don't know enough about hashing to produce something decent.
		// that has such a low risk of collisions that this is fine. Need to think more about it.
		// Shouldn't it be named NextWithSameSlot?

        glyph_entry *Entry = GetGlyphEntry(Table, EntryIndex);
        if (Entry->HashValue.Value == Hash.Value)
        {
            Result = Entry;
            break;
        }

        EntryIndex = Entry->NextWithSameHash;
    }

    if (Result)
    {
        Cim_Assert(EntryIndex);

        glyph_entry *Prev = GetGlyphEntry(Table, Result->PrevLRU);
        glyph_entry *Next = GetGlyphEntry(Table, Result->NextLRU);

        Prev->NextLRU = Result->NextLRU;
        Next->PrevLRU = Result->PrevLRU;

        ValidateLRU(Table, -1);

        ++Table->Stats.HitCount;
    }
    else
    {
        EntryIndex = PopFreeEntry(Table);
        Cim_Assert(EntryIndex);

        Result = GetGlyphEntry(Table, EntryIndex);
        Cim_Assert(Result->NextWithSameHash == 0);

        Result->NextWithSameHash = *Slot;
        Result->HashValue        = Hash;

        *Slot = EntryIndex;

        ++Table->Stats.MissCount;
    }

    glyph_entry *Sentinel = GetSentinel(Table);
    Cim_Assert(Result != Sentinel);
    Result->NextLRU = Sentinel->NextLRU;
    Result->PrevLRU = 0;

    glyph_entry *NextLRU = GetGlyphEntry(Table, Sentinel->NextLRU);
    NextLRU->PrevLRU  = EntryIndex;
    Sentinel->NextLRU = EntryIndex;

#if DEBUG_VALIDATE_LRU
    Result->Ordering = Sentinel->Ordering++;
#endif
    ValidateLRU(Table, 1);

    glyph_info Info;
    Info.MapId     = EntryIndex;
	Info.IsInAtlas = Result->IsInAtlas;
	Info.Size      = Result->Size;
	Info.U0        = Result->U0;
	Info.V0        = Result->V0;
	Info.U1        = Result->U1;
	Info.V1        = Result->V1;

    return Info;
}

static glyph_table *
PlaceGlyphTableInMemory(glyph_table_params Params, void *Memory)
{
	Cim_Assert(Params.HashCount >= 1);
	Cim_Assert(Params.EntryCount >= 2);
	Cim_Assert(Cim_IsPowerOfTwo(Params.HashCount));

	glyph_table *Result = NULL;

	if (Memory)
	{
		glyph_entry *Entries = (glyph_entry *)Memory;
		Result = (glyph_table *)(Entries + Params.EntryCount);
		Result->HashTable = (uint32_t *)(Result + 1);
		Result->Entries   = Entries;

		Result->HashMask   = Params.HashCount - 1;
		Result->HashCount  = Params.HashCount;
		Result->EntryCount = Params.EntryCount;

		memset(Result->HashTable, 0, Result->HashCount * sizeof(Result->HashTable[0]));

		for (uint32_t EntryIndex = 0; EntryIndex < Params.EntryCount; ++EntryIndex)
		{
			glyph_entry *Entry = GetGlyphEntry(Result, EntryIndex);
			if ((EntryIndex + 1) < Params.EntryCount)
			{
				Entry->NextWithSameHash = EntryIndex + 1;
			}
			else
			{
				Entry->NextWithSameHash = 0;
			}

			Entry->IsInAtlas = false;
			Entry->U0        = 0;
			Entry->V0        = 0;
			Entry->U1        = 0;
			Entry->V1        = 0;
			Entry->Size      = {};
		}

		GetAndClearStats(Result);
	}

	return Result;
}

static size_t 
GetGlyphTableFootprint(glyph_table_params Params)
{
	size_t HashSize  = Params.HashCount * sizeof(uint32_t);
	size_t EntrySize = Params.EntryCount * sizeof(glyph_entry);
	size_t Result    = (sizeof(glyph_table) + HashSize + EntrySize);

	return Result;
}