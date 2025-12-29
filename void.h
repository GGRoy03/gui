// [Third-Party Libraries]

#define STB_RECT_PACK_IMPLEMENTATION
#include "third_party/stb_rect_pack.h"

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

#define XXH_NO_STREAM
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "third_party/xxhash.h"

#include "third_party/nxi_log.h"
#include "third_party/ntext.h"

// ====================================================================
// [Basic Types]
// ====================================================================

namespace gui
{

#include <type_traits>

template<typename E>
struct enable_bitmask_operators : std::false_type {};

template<typename E>
constexpr std::enable_if_t<enable_bitmask_operators<E>::value, E>
operator|(E a, E b) noexcept {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(a) | static_cast<U>(b));
}

template<typename E>
constexpr std::enable_if_t<enable_bitmask_operators<E>::value, E>
operator&(E a, E b) noexcept {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(a) & static_cast<U>(b));
}

template<typename E>
constexpr std::enable_if_t<enable_bitmask_operators<E>::value, E&>
operator|=(E& a, E b) noexcept {
    return a = a | b;
}

template<typename E>
constexpr std::enable_if_t<enable_bitmask_operators<E>::value, E&>
operator&=(E& a, E b) noexcept {
    return a = a & b;
}

template<typename E>
constexpr std::enable_if_t<enable_bitmask_operators<E>::value, E>
operator~(E a) noexcept {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(~static_cast<U>(a));
}

struct dimensions
{
    float Width;
    float Height;

    constexpr dimensions operator+(dimensions Other) const noexcept
    {
        return {this->Width + Other.Width, this->Height + Other.Height};
    }
};


struct point
{
    float X;
    float Y;

    constexpr point operator+(point Other) const noexcept
    {
        return {this->X + Other.X, this->Y + Other.Y};
    }

    constexpr point operator-(point Other) const noexcept
    {
        return {this->X - Other.X, this->Y - Other.Y};
    }

    constexpr point operator+(dimensions Dimensions) const noexcept
    {
        return {this->X + Dimensions.Width, this->Y + Dimensions.Height};
    }

    constexpr point operator-(dimensions Dimensions) const noexcept
    {
        return {this->X - Dimensions.Width, this->Y - Dimensions.Height};
    }
};


struct translation
{
    float X;
    float Y;

    translation(point A, point B)
    {
        X = A.X - B.X;
        Y = A.Y - B.Y;
    }
};


struct bounding_box
{
    float Left;
    float Top;
    float Right;
    float Bottom;

    bounding_box()
    {
        Left   = 0;
        Top    = 0;
        Right  = 0;
        Bottom = 0;
    }

    bounding_box(point Point, dimensions Dimensions)
    {
        Left   = Point.X;
        Top    = Point.Y;
        Right  = Point.X + Dimensions.Width;
        Bottom = Point.Y + Dimensions.Height;
    }
};

}

// [Headers]

#include "base/base_core.h"
#include "ui/ui_inc.h"

// [Source Files]

#include "ui/ui_inc.cpp"
