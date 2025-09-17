typedef struct ID2D1RenderTarget    ID2D1RenderTarget;
typedef struct ID2D1SolidColorBrush ID2D1SolidColorBrush;
typedef struct IDWriteTextFormat    IDWriteTextFormat;
typedef struct IDWriteFontFace      IDWriteFontFace;
typedef struct IDWriteFactory       IDWriteFactory;

typedef struct os_font_objects
{
    IDWriteTextFormat    *TextFormat;
    IDWriteFontFace      *FontFace;
    ID2D1RenderTarget    *DWriteTarget;
    ID2D1SolidColorBrush *FillBrush;
} os_font_objects;

typedef struct os_text_backend
{
    IDWriteFactory *DWriteFactory;
} os_text_backend;