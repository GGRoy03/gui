// [Macros]

#define ui_id(Id)  byte_string_literal(Id)
#define UIBlock(X) DeferLoop(X, UIEnd())


internal ui_node_chain * UIWindow_      (u32 Style);
internal ui_node_chain * UIScrollView_  (u32 Style);
internal ui_node_chain * UITextInput    (u8 *TextBuffer, u64 TextBufferSize, u32 Style);
