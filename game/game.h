// [Core Types]

typedef struct game_state
{
    // Memory
    memory_arena *StaticArena;
    memory_arena *FrameArena;

    // Rendering
    render_pass_list RenderPassList;
    render_handle    RendererHandle;
} game_state;

// [Core API]

internal void GameEntryPoint();
