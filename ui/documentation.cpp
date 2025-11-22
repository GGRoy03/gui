// TODO:
// NameSpace this?
// Make strings globals.
// Figure out what we even want to do with this

enum VoidDocumentation_Style : u32
{
    VoidDocumentation_None       = 0,
    VoidDocumentation_MainWindow = 1,
    VoidDocumentation_TextTitle  = 2,
    VoidDocumentation_TextBody   = 3,
};

struct ui_void_documentation
{
    ui_pipeline *UIPipeline;
    b32          IsInitialized;
};

// -----------------------------------------------------------------------------------
// @Internal Private : VoidDocumentation Context Management

internal bool
InitializeVoidDocumentation(ui_void_documentation *Doc)
{
    Assert(Doc);
    Assert(!Doc->IsInitialized);

    ui_pipeline_params PipelineParams =
    {
        .ThemeFile = byte_string_literal("styles/void_documentation.void"),
    };
    Doc->UIPipeline = UICreatePipeline(PipelineParams);

    ui_subtree_params SubtreeParams =
    {
        .CreateNew = true,
        .NodeCount = 256,
    };

    UISubtree(SubtreeParams)
    {
        UIBlock(UIWindow(VoidDocumentation_MainWindow))
        {
            UILabel(str8_lit("Official Void Engine Documentation"), VoidDocumentation_TextTitle);
            UILabel(str8_lit("Sampe Text No One Cares About..............") , VoidDocumentation_TextBody);
        }
    }

    return true;
}

internal void
ShowVoidDocumentationUI(void)
{
    static ui_void_documentation Doc;

    if(!Doc.IsInitialized)
    {
        Doc.IsInitialized = InitializeVoidDocumentation(&Doc);
    }

    UIBeginAllSubtrees(Doc.UIPipeline);
    UIExecuteAllSubtrees(Doc.UIPipeline);
}
