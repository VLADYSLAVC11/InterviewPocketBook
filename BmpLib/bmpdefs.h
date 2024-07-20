// Copyright PocketBook - Interview Task

#pragma once

#include <cstdint>

namespace PocketBook {

#pragma pack(push, 1)
struct BmpHeader
{
    std::uint16_t Signature;
    std::uint32_t FileSize;
    std::uint32_t IndexOffset; // Reserved for bmp, active for barch
    std::uint32_t DataOffset;
};

struct BmpInfoHeader
{
    std::uint32_t Size;
    std::uint32_t Width;
    std::uint32_t Height;
    std::uint16_t Planes;
    std::uint16_t BitsPerPixel;
    std::uint32_t Compression;
    std::uint32_t ImageSize;
    std::uint32_t XpixelsPerM;
    std::uint32_t YpixelsPerM;
    std::uint32_t ColorsUsed;
    std::uint32_t NumImportantColors;
};
#pragma pack(pop)

struct RawImageData
{
    int getActualWidth() const
    {
        return Width + getPadding();
    }

    static int calculatePadding(int _width)
    {
        static constexpr std::size_t alignment = sizeof(std::uint32_t);
        return _width % alignment == 0 ? 0 : alignment - (_width % alignment);
    }

    int getPadding() const
    {
        return calculatePadding(Width);
    }

    int getActualHeight() const
    {
        return Height;
    }

    int Width; // image width in pixels
    int Height; // image height in pixels
    const unsigned char * Data; // Pointer to image data. data[j * width + i] is color of pixel in row j and column i.
};

} // namespace PocketBook
