#include <d2d1.h>
#include <dwrite.h>

extern "C"
{
#include "base/base_inc.h"
#include "os/os_inc.h"
#include "render/render_inc.h"
#include "ui/ui_inc.h"
}

extern "C" b32 
OSWin32FontIsValid(os_font_objects *Objects)
{
    b32 Result = (Objects->FontFace != 0) && (Objects->TextFormat != 0);
    return Result;
}

extern "C" void
OSWin32AcquireTextBackend()
{   
    OSWin32ReleaseTextBackend(OSWin32State.TextBackend);

    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown **)&OSWin32State.TextBackend->DWriteFactory)))
    {
        OSLogMessage(byte_string_literal("Win32: Failed to create DirectWrite factory."), OSMessage_Error);
    }
}

extern "C" void
OSWin32ReleaseTextBackend(os_text_backend *TextBackend)
{
    if (TextBackend)
    {
        if (TextBackend->DWriteFactory)
        {
            TextBackend->DWriteFactory->Release();
            TextBackend->DWriteFactory = 0;
        }
    }
}

extern "C" b32
OSAcquireFontObjects(byte_string Name, f32 Size, gpu_font_objects *GPUObjects, os_font_objects *OSObjects)
{
    b32 Result = 0;

    if (!Name.String || !Name.Size || !Size || !OSObjects)
    {
        return Result;
    }

    IDWriteFactory *Factory  = OSWin32State.TextBackend->DWriteFactory;
    wide_string     FontName = ByteStringToWideString(OSWin32State.Arena, Name);

    DWRITE_FONT_WEIGHT  Weight  = DWRITE_FONT_WEIGHT_REGULAR;
    DWRITE_FONT_STYLE   Style   = DWRITE_FONT_STYLE_NORMAL;
    DWRITE_FONT_STRETCH Stretch = DWRITE_FONT_STRETCH_NORMAL;

    OSReleaseFontObjects(OSObjects);

    if (Factory && FontName.String)
    {
        Factory->CreateTextFormat((WCHAR *)FontName.String, 0,
                                  Weight, Style, Stretch, Size,
                                  L"en-us", &OSObjects->TextFormat);
        if (OSObjects->TextFormat)
        {
            OSObjects->TextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
            OSObjects->TextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }
    }

    if (OSObjects->TextFormat)
    {
        IDWriteFontCollection *Collection = 0;
        Factory->GetSystemFontCollection(&Collection);

        if (Collection)
        {
            UINT32 Index  = 0;
            BOOL   Exists = false;
            Collection->FindFamilyName((WCHAR *)FontName.String, &Index, &Exists);

            if (Exists)
            {
                IDWriteFontFamily *Family = 0;
                Collection->GetFontFamily(Index, &Family);

                if (Family)
                {
                    IDWriteFont *DWriteFont = 0;
                    Family->GetFirstMatchingFont(Weight, Stretch, Style, &DWriteFont);
                    if (DWriteFont)
                    {
                        DWriteFont->CreateFontFace(&OSObjects->FontFace);
                        DWriteFont->Release();
                    }

                    Family->Release();
                }
            }

            Collection->Release();
        }
    }

    if (OSObjects->FontFace)
    {
        ID2D1Factory        *D2DFactory = 0;
        D2D1_FACTORY_OPTIONS Options    = { D2D1_DEBUG_LEVEL_ERROR };

        HRESULT Error = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), &Options, (void **)&D2DFactory);
        if (SUCCEEDED(Error))
        {
            D2D1_RENDER_TARGET_PROPERTIES Properties =
                D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                0, 0);

            Error = D2DFactory->CreateDxgiSurfaceRenderTarget(GPUObjects->GlyphTransferSurface, &Properties, &OSObjects->DWriteTarget);
            if (SUCCEEDED(Error))
            {
                Error = OSObjects->DWriteTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &OSObjects->FillBrush);
                if (FAILED(Error))
                {
                    OSLogMessage(byte_string_literal("Win32: Failed to create D2D Solid Color Brush."), OSMessage_Error);
                }
            }
            else
            {
                OSLogMessage(byte_string_literal("Win32: Failed to create DXGI Transfer Surface."), OSMessage_Error);
            }
        }
        else
        {
            OSLogMessage(byte_string_literal("Win32: Failed to create D2D Factory."), OSMessage_Error);
        }
    }

    // TODO: Release the wide string.

    Result = (OSObjects->TextFormat != 0) && (OSObjects->FontFace != 0) && (OSObjects->DWriteTarget != 0) && (OSObjects->FillBrush != 0);
    if (!Result)
    {
        OSReleaseFontObjects(OSObjects);
    }

    return Result;
}

extern "C" void
OSReleaseFontObjects(os_font_objects *Objects)
{
    if (Objects->TextFormat)
    {
        Objects->TextFormat->Release();
        Objects->TextFormat = 0;
    }

    if (Objects->FontFace)
    {
        Objects->FontFace->Release();
        Objects->FontFace = 0;
    }
}

// NOTE: Only handles extended ASCII right now.

extern "C" os_glyph_layout
OSGetGlyphLayout(u8 Character, ui_font *Font)
{
    os_glyph_layout  Result    = {0};
    HRESULT          Error     = S_OK;
    os_text_backend *OSBackend = OSWin32State.TextBackend;
    memory_arena    *OSArena   = OSWin32State.Arena;;

    if (OSBackend && OSBackend->DWriteFactory)
    {
        wide_string WideCharacter = ByteStringToWideString(OSArena, ByteString(&Character, 1));

        IDWriteTextLayout *TextLayout = 0;
        OSBackend->DWriteFactory->CreateTextLayout((WCHAR *)WideCharacter.String, (u32)WideCharacter.Size, Font->OSFontObjects.TextFormat,
                                                   (FLOAT)Font->TextureSize.X, (FLOAT)Font->TextureSize.Y,
                                                   &TextLayout);
        if (TextLayout)
        {
            DWRITE_TEXT_METRICS Metrics = {0};
            TextLayout->GetMetrics(&Metrics);
            Result.Size.X   = (u16)(Metrics.width  + 0.5f);
            Result.Size.Y   = (u16)(Metrics.height + 0.5f);
            Result.Offset.X = (f32)(Metrics.left);
            Result.Offset.Y = (f32)(Metrics.top);

            u16 GlyphIndex = 0;
            u32 CodePoint  = (u32)WideCharacter.String[0];
            Error = Font->OSFontObjects.FontFace->GetGlyphIndicesW(&CodePoint, 1, &GlyphIndex);

            if (SUCCEEDED(Error))
            {
                DWRITE_GLYPH_METRICS GlyphMetrics = {0};
                Error = Font->OSFontObjects.FontFace->GetDesignGlyphMetrics(&GlyphIndex, 1, &GlyphMetrics, 0);

                if (SUCCEEDED(Error))
                {
                    DWRITE_FONT_METRICS FontMetrics = {0};
                    Font->OSFontObjects.FontFace->GetMetrics(&FontMetrics);

                    f32 Scale = Font->Size / (f32)FontMetrics.designUnitsPerEm;
                    Result.AdvanceX = (f32)(GlyphMetrics.advanceWidth * Scale + 0.5f);
                }
                else
                {
                    OSLogMessage(byte_string_literal("Failed to query glyph metrics."), OSMessage_Error);
                }
            }
            else
            {
                OSLogMessage(byte_string_literal("Failed to query glyph index."), OSMessage_Error);
            }

            TextLayout->Release();
        }

        PopArena(OSArena, WideCharacter.Size * sizeof(WideCharacter.String[0]));
    }


    return Result;
}