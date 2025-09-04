#pragma once

// [Core Types]

typedef struct os_system_info
{
    cim_u64 PageSize;
} os_system_info;

// [Core API]

// Per-OS Functions.

internal os_system_info *OSGetSystemInfo  (void);

internal void           *OSReserveMemory  (cim_u64 Size);
internal cim_b32         OSCommitMemory   (void *Memory, cim_u64 Size);

internal void            OSAbort(cim_i32 ExitCode);
