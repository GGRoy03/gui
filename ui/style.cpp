//-------------------------------------------------------------------------------------
// @Internal: Style Helpers

internal u32
ResolveCachedIndex(u32 Index)
{
    Assert(Index);

    u32 Result = Index - 1;
    return Result;
}

internal ui_cached_style *
GetCachedStyle(u32 Index, ui_style_registry *Registry)
{
    Assert(Registry);

    ui_cached_style *Result = 0;

    if(Index < Registry->StylesCount)
    {
        Result = Registry->Styles + Index;
    }

    return Result;
}

internal style_property *
GetCachedProperties(u32 StyleIndex, StyleState_Type State, ui_style_registry *Registry)
{
    Assert(StyleIndex);
    Assert(StyleIndex <= Registry->StylesCount);

    style_property  *Result = 0;

    u32              ResolvedIndex = ResolveCachedIndex(StyleIndex);
    ui_cached_style *Cached        = GetCachedStyle(ResolvedIndex, Registry);

    if(Cached)
    {
        Result = Cached->Properties[State];
    }

    return Result;
}

// ------------------------------------------------------------------------------------
// Style Manipulation Public API

internal void
SetNodeStyleState(StyleState_Type State, u32 NodeIndex, ui_subtree *Subtree)
{
    Assert(Subtree);

    ui_node_style *Style = GetNodeStyle(NodeIndex, Subtree);
    Assert(Style);

    Style->State = State;
}

internal void
SetNodeStyle(u32 NodeIndex, u32 StyleIndex, ui_subtree *Subtree)
{
    ui_node_style *Style = GetNodeStyle(NodeIndex, Subtree);
    Assert(Style);

    ui_pipeline *Pipeline = GetCurrentPipeline();
    Assert(Pipeline);

    style_property *Cached[StyleState_Count] = {0};
    Cached[StyleState_Default] = GetCachedProperties(Style->CachedStyleIndex, StyleState_Default, Pipeline->Registry);
    Cached[StyleState_Hovered] = GetCachedProperties(Style->CachedStyleIndex, StyleState_Hovered, Pipeline->Registry);
    Cached[StyleState_Focused] = GetCachedProperties(Style->CachedStyleIndex, StyleState_Focused, Pipeline->Registry);

    ForEachEnum(StyleState_Type, StyleState_Count, State)
    {
        Assert(Cached[State]);

        ForEachEnum(StyleProperty_Type, StyleProperty_Count, Prop)
        {
            if(!Style->Properties[State][Prop].IsSetRuntime)
            {
                 Style->Properties[State][Prop] = Cached[State][Prop];
            }
        }
    }

    Style->CachedStyleIndex = StyleIndex;
}

internal style_property *
GetPaintProperties(u32 NodeIndex, b32 ClearState, ui_subtree *Subtree)
{
    Assert(Subtree);

    ui_node_style *Style = GetNodeStyle(NodeIndex, Subtree);
    Assert(Style);

    style_property *Result = PushArray(Subtree->Transient, style_property, StyleProperty_Count);
    Assert(Result);

    ForEachEnum(StyleProperty_Type, StyleProperty_Count, Prop)
    {
        if(Style->Properties[Style->State][Prop].IsSet)
        {
            Result[Prop] = Style->Properties[Style->State][Prop];
        }
        else
        {
            Result[Prop] = Style->Properties[StyleState_Default][Prop];
        }
    }

    if(ClearState)
    {
        Style->State = StyleState_Default;
    }

    return Result;
}

internal ui_node_style *
GetNodeStyle(u32 Index, ui_subtree *Subtree)
{
    Assert(Subtree);

    ui_node_style *Result = 0;

    if(Index < Subtree->NodeCount)
    {
        Result = Subtree->ComputedStyles + Index;
    }

    return Result;
}

// [Helpers]

internal b32
IsVisibleColor(ui_color Color)
{
    b32 Result = (Color.A > 0.f);
    return Result;
}
