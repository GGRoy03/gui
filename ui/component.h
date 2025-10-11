// [Macros]

#define UIScrollView_(Style, Pipeline) DeferLoop(UIScrollView(Style, Pipeline), PopLayoutNodeParent(Pipeline->LayoutTree))

// [Components]

internal void  UIWindow      (u32 Style, ui_pipeline *Pipeline);
internal void  UIButton      (u32 Style, ui_click_callback *Callback, ui_pipeline *Pipeline);
internal void  UILabel       (u32 Style, byte_string Text, ui_pipeline *Pipeline);
internal void  UIHeader      (u32 Style, ui_pipeline *Pipeline);
internal void  UIScrollView  (u32 Style, ui_pipeline *Pipeline);
