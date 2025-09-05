// [Core Types]

typedef struct game_state
{
    memory_arena *StaticArena;
    memory_arena *FrameArena;

    render_context RenderContext;
} game_state;

// [Core API]

internal void GameEntryPoint();
