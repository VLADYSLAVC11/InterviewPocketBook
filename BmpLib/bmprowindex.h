// Copyright PocketBook - Interview Task

#pragma once

#include "dynamicbitset.h"

namespace PocketBook {

struct RawImageData;
struct IProgressNotifier;

// BmpRowIndex encodes each exact row as separate bit [0:n-1]
// The bit is set to 1 when row contains only white pixels
// The bit is set to 0 when row contains not only white pixels
class BmpRowIndex
{
public:
    BmpRowIndex(std::size_t _height);
    BmpRowIndex(std::size_t _height, std::vector<std::uint8_t> && _source);
    BmpRowIndex(std::size_t _height, std::uint8_t * _indexData /* non-owning */);
    ~BmpRowIndex();

    void setRowIsEmpty(std::size_t _row, bool _val);

    bool testRowIsEmpty(std::size_t _row) const;

    std::size_t getIndexSizeInBytes() const;

    const std::uint8_t * getData() const;

    static std::vector<std::uint8_t> getWhiteRowPattern(int _width);

    static BmpRowIndex createFromRawImageData(
            const PocketBook::RawImageData & _raw
        ,   PocketBook::IProgressNotifier * _progressNotifier = nullptr
    );

private:
    union
    {
        PocketBook::DynamicBitset * m_bitset;
        std::uint8_t * m_data;

    } m_index;

    std::size_t m_height;
    bool m_isBitset;
};

} // namespace PocketBook
