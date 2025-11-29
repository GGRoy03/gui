//-------------------------------------------------------------------------------------
// @internal: Style Helpers

static uint32_t
ResolveCachedIndex(uint32_t Index)
{
    VOID_ASSERT(Index);

    uint32_t Result = Index - 1;
    return Result;
}

static ui_cached_style *
GetCachedStyle(uint32_t StyleIndex, ui_cached_style_list *List)
{
    VOID_ASSERT(StyleIndex);
    VOID_ASSERT(StyleIndex <= List->Count);

    ui_cached_style *Result = 0;

    uint32_t IterCount = 0;
    IterateLinkedList(List, ui_cached_style_node *, Node)
    {
        if(StyleIndex == IterCount)
        {
            Result = &Node->Value;
            break;
        }

        IterCount++;
    }

    return Result;
}

// ------------------------------------------------------------------------------------
// Style Manipulation Public API

static ui_paint_properties *
GetPaintProperties(uint32_t NodeIndex, ui_subtree *Subtree)
{
    VOID_ASSERT(Subtree);

    ui_paint_properties *Result = 0;

    if(NodeIndex < Subtree->NodeCount)
    {
        Result = Subtree->PaintArray + NodeIndex;
    }

    return Result;
}

static void
ClearPaintProperties(uint32_t NodeIndex, ui_subtree *Subtree)
{
    VOID_ASSERT(!"Not Implemented");
}

static void
SetNodeStyle(uint32_t NodeIndex, uint32_t StyleIndex, ui_subtree *Subtree)
{
    ui_pipeline *Pipeline = GetCurrentPipeline();
    VOID_ASSERT(Pipeline);

    ui_cached_style *CachedStyle = GetCachedStyle(StyleIndex, Pipeline->CachedStyles);
    VOID_ASSERT(CachedStyle);

    ui_paint_properties *Paint = GetPaintProperties(NodeIndex, Subtree);
    VOID_ASSERT(Paint);

    Paint->CachedIndex = StyleIndex;
    MemoryCopy(&Paint->Properties, &CachedStyle->Properties[StyleState_Default], sizeof(Paint->Properties));
}

// [Helpers]

static bool
IsVisibleColor(ui_color Color)
{
    bool Result = (Color.A > 0.f);
    return Result;
}
