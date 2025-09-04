#pragma once

typedef struct memory_arena_params
{
    cim_u64 CommitSize;
    cim_u64 ReserveSize;
    char   *AllocatedFromFile;
    cim_u32 AllocatedFromLine;
} memory_arena_params;

typedef struct memory_arena
{
    cim_u64 CommitSize;
    cim_u64 ReserveSize;

    cim_u64 BasePosition;
    cim_u64 Position;

    char   *AllocatedFromFile;
    cim_u32 AllocatedFromLine;
} memory_arena;

global cim_u64 ArenaDefaultReserveSize = Megabyte(64);
global cim_u64 ArenaDefaultCommitSize  = Kilobyte(64);

internal memory_arena *AllocateArena  (memory_arena_params Params);
internal void         *PushArena      (memory_arena *Arena, cim_u64 Size, cim_u64 Align);
internal void          ClearArena     (memory_arena *Arena);
internal void          PopArena       (memory_arena *Arena, cim_u64 Amount);