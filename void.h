// ====================================================================
// [Third-Party Libraries]
// ====================================================================

#define STB_RECT_PACK_IMPLEMENTATION
#include "third_party/stb_rect_pack.h"

#define XXH_NO_STREAM
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "third_party/xxhash.h"

// ====================================================================
// [Basic Types]
// ====================================================================


#include <stdint.h>

typedef int gui_bool;
#define GUI_TRUE  1
#define GUI_FALSE 0


typedef struct gui_dimensions
{
    float Width;
    float Height;
} gui_dimensions;


typedef struct gui_point
{
    float X;
    float Y;
} gui_point;


typedef struct gui_translation
{
    float X;
    float Y;
} gui_translation;


static gui_dimensions
GuiDimensionsAdd(gui_dimensions A, gui_dimensions B)
{
    gui_dimensions Result;
    Result.Width  = A.Width  + B.Width;
    Result.Height = A.Height + B.Height;
    return Result;
}


static gui_point
GuiPointAddPoint(gui_point A, gui_point B)
{
    gui_point Result;
    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;
    return Result;
}


static gui_point
GuiPointSubPoint(gui_point A, gui_point B)
{
    gui_point Result;
    Result.X = A.X - B.X;
    Result.Y = A.Y - B.Y;
    return Result;
}


static gui_point
GuiPointAddDimensions(gui_point P, gui_dimensions D)
{
    gui_point Result;
    Result.X = P.X + D.Width;
    Result.Y = P.Y + D.Height;
    return Result;
}


static gui_point
GuiPointSubDimensions(gui_point P, gui_dimensions D)
{
    gui_point Result;
    Result.X = P.X - D.Width;
    Result.Y = P.Y - D.Height;
    return Result;
}


static gui_translation
GuiTranslationFromPoints(gui_point A, gui_point B)
{
    gui_translation Result;
    Result.X = A.X - B.X;
    Result.Y = A.Y - B.Y;
    return Result;
}


// --------------------------------------------------------------------
// gui_bounding_box
// --------------------------------------------------------------------


typedef struct gui_bounding_box
{
    float Left;
    float Top;
    float Right;
    float Bottom;
} gui_bounding_box;


static gui_bounding_box
GuiBoundingBoxZero(void)
{
    gui_bounding_box Result;
    Result.Left   = 0.0f;
    Result.Top    = 0.0f;
    Result.Right  = 0.0f;
    Result.Bottom = 0.0f;
    return Result;
}


static gui_bounding_box
GuiBoundingBoxFromPointAndDimensions(gui_point P, gui_dimensions D)
{
    gui_bounding_box Result;
    Result.Left   = P.X;
    Result.Top    = P.Y;
    Result.Right  = P.X + D.Width;
    Result.Bottom = P.Y + D.Height;
    return Result;
}


// ====================================================================
// [Headers]
// ====================================================================

#include "base/base_core.h"
#include "ui/ui_inc.h"

// ====================================================================
// [Source Files]
// ====================================================================

#include "ui/ui_inc.cpp"
