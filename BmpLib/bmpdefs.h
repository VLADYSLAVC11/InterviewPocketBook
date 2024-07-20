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
	int width; // image width in pixels
	int height; // image height in pixels
	int padding; // zero padding for each row
	const unsigned char * data; // Pointer to image data. data[j * width + i] is color of pixel in row j and column i.
};

} // namespace PocketBook
