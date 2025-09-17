typedef struct ui_test
{
	// UI Objects
	ui_layout_tree LayoutTree;

	// Styles
	ui_style_name WindowStyle;
	ui_style_name ButtonStyle;

	// Fonts
	ui_font *Font;

	// Misc
	b32 IsInitialized;
} ui_test;

internal void
TestUI(ui_state *UIState, render_context *RenderContext, render_handle RendererHandle)
{
	local_persist ui_test UITest;

	if (!UITest.IsInitialized)
	{
		ui_layout_tree_params Params = { 0 };
		Params.NodeCount = 16;
		Params.Depth     = 4;

		// Allocations/Caching
		UITest.LayoutTree  = UIAllocateLayoutTree(Params);
		UITest.WindowStyle = UIGetCachedNameFromStyleName(byte_string_literal("TestWindow"), &UIState->StyleRegistery);
		UITest.ButtonStyle = UIGetCachedNameFromStyleName(byte_string_literal("TestButton"), &UIState->StyleRegistery);
		UITest.Font        = UILoadFont(byte_string_literal("Consolas"), 15, RendererHandle, UIFontCoverage_ASCIIOnly);

		// Layout
		UIWindow(UITest.WindowStyle, &UITest.LayoutTree, &UIState->StyleRegistery);
		UIButton(UITest.ButtonStyle, &UITest.LayoutTree, &UIState->StyleRegistery);
		UIButton(UITest.ButtonStyle, &UITest.LayoutTree, &UIState->StyleRegistery);

		UILabel(byte_string_literal("This is a label."), &UITest.LayoutTree, UITest.Font);

		UITest.IsInitialized = 1;
	}

	UIComputeLayout(&UITest.LayoutTree, RenderContext);
}