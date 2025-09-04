internal memory_arena *
AllocateArena(memory_arena_params Params)
{
    cim_u64 ReserveSize = AlignPow2(Params.ReserveSize, OSGetSystemInfo()->PageSize);
    cim_u64 CommitSize  = AlignPow2(Params.CommitSize , OSGetSystemInfo()->PageSize);

    void   *HeapBase     = OSReserveMemory(ReserveSize);
    cim_b32 CommitResult = OSCommitMemory(HeapBase, CommitSize);
    if(!HeapBase || !CommitResult)
    {
        OSAbort(1);
    }

    memory_arena *Arena = (memory_arena*)HeapBase;
    Arena->CommitSize        = Params.CommitSize;
    Arena->ReserveSize       = Params.ReserveSize;
    Arena->BasePosition      = 0;
    Arena->Position          = sizeof(memory_arena);
    Arena->AllocatedFromFile = Params.AllocatedFromFile;
    Arena->AllocatedFromLine = Params.AllocatedFromLine;

    return Arena;
}

internal void *
PushArena(memory_arena *Arena, cim_u64 Size, cim_u64 Alignment)
{
    cim_u64 PositionBefore = AlignPow2(Arena->Position, Alignment);
    cim_u32 PositionAfter  = PositionBefore + Size;

    if(Arena->ReserveSize < PositionAfter)
    {
        OSAbort(1);
    }

    if(Arena->CommitSize < PositionAfter)
    {
        cim_u64 CommitPostAligned = PositionAfter + (Arena->CommitSize - 1);
        CommitPostAligned        -= CommitPostAligned % Arena->CommitSize;

        cim_u64 CommitPostClamped = ClampTop(CommitPostAligned, Arena->ReserveSize);
        cim_u64 CommitSize        = CommitPostClamped - Arena->CommitSize;
        cim_u8 *CommitPointer     = (cim_u8*)Arena + Arena->CommitSize;

        cim_b32 CommitResult = OSCommitMemory(CommitPointer, CommitSize);
        if(!CommitResult)
        {
            OSAbort(1);
        }

        Arena->CommitSize = CommitPostClamped;
    }

    void *Result = 0;
    if(Arena->CommitSize >= PositionAfter)
    {
        Result          = (cim_u8*)Arena + PositionAfter;
        Arena->Position = PositionAfter;
    }

    return Result;
}