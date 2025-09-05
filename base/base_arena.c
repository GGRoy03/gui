internal memory_arena *
AllocateArena(memory_arena_params Params)
{
    u64 ReserveSize = AlignPow2(Params.ReserveSize, OSGetSystemInfo()->PageSize);
    u64 CommitSize  = AlignPow2(Params.CommitSize , OSGetSystemInfo()->PageSize);

    void *HeapBase     = OSReserveMemory(ReserveSize);
    b32   CommitResult = OSCommitMemory(HeapBase, CommitSize);
    if(!HeapBase || !CommitResult)
    {
        OSAbort(1);
    }

    memory_arena *Arena = (memory_arena*)HeapBase;
    Arena->Current           = Arena;
    Arena->CommitSize        = Params.CommitSize;
    Arena->ReserveSize       = Params.ReserveSize;
    Arena->Committed         = CommitSize;
    Arena->Reserved          = ReserveSize;
    Arena->BasePosition      = 0;
    Arena->Position          = sizeof(memory_arena);
    Arena->AllocatedFromFile = Params.AllocatedFromFile;
    Arena->AllocatedFromLine = Params.AllocatedFromLine;

    return Arena;
}

internal void *
PushArena(memory_arena *Arena, u64 Size, u64 Alignment)
{
    memory_arena *Active       = Arena->Current;
    u64           PrePosition  = AlignPow2(Active->Position, Alignment);
    u64           PostPosition = PrePosition + Size;

    if(Active->Reserved < PostPosition)
    {
        memory_arena *New = 0;

        u64 ReserveSize = Active->ReserveSize;
        u64 CommitSize  = Active->CommitSize;
        if(Size + sizeof(memory_arena) > ReserveSize)
        {
            ReserveSize = AlignPow2(Size + sizeof(memory_arena), Alignment);
            CommitSize  = AlignPow2(Size + sizeof(memory_arena), Alignment);
        }

        memory_arena_params Params = {0};
        Params.CommitSize        = CommitSize;
        Params.ReserveSize       = ReserveSize;
        Params.AllocatedFromFile = Active->AllocatedFromFile;
        Params.AllocatedFromLine = Active->AllocatedFromLine;

        New = AllocateArena(Params);
        New->BasePosition = Active->BasePosition + Active->ReserveSize;
        New->Prev         = Active;

        Active       = New;
        PrePosition  = AlignPow2(Active->Position, Alignment);
        PostPosition = PrePosition + Size;
    }

    if(Active->Committed < PostPosition)
    {
        u64 CommitPostAligned = PostPosition + (Active->Committed - 1);
        CommitPostAligned    -= CommitPostAligned % Active->Committed;

        u64 CommitPostClamped = ClampTop(CommitPostAligned, Active->ReserveSize);
        u64 CommitSize        = CommitPostClamped - Active->CommitSize;
        u8 *CommitPointer     = (u8*)Active + Active->CommitSize;

        b32 CommitResult = OSCommitMemory(CommitPointer, CommitSize);
        if(!CommitResult)
        {
            OSAbort(1);
        }

        Active->CommitSize = CommitPostClamped;
    }

    void *Result = 0;
    if(Active->CommitSize >= PostPosition)
    {
        Result          = (u8*)Active + PostPosition;
        Active->Position = PostPosition;
    }

    return Result;
}

internal void
ClearArena(memory_arena *Arena)
{
    PopArenaTo(Arena, 0);
}

internal void
PopArenaTo(memory_arena *Arena, u64 Position)
{
    memory_arena *Active    = Arena->Current;
    u64           PoppedPos = ClampBot(sizeof(memory_arena), Position);

    for(memory_arena *Prev = 0; Active->BasePosition >= PoppedPos; Active = Prev)
    {
        Prev = Active->Prev;
        OSRelease(Active);
    }

    Arena->Current           = Active;
    Arena->Current->Position = PoppedPos - Arena->Current->BasePosition;
}
