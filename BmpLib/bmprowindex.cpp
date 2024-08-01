// Copyright PocketBook - Interview Task

#include "bmprowindex.h"
#include "bmpdefs.h"
#include "bmputils.h"

#include <cassert>
#include <stdexcept>
#include <thread>
#include <memory.h>
#include <cstdlib>

namespace PocketBook {

BmpRowIndex::BmpRowIndex(std::size_t _height)
    : m_height(_height), m_isBitset(true)
{
    auto bitset = std::make_unique<PocketBook::DynamicBitset>(
            PocketBook::DynamicBitset::getNumBlocksRequired(m_height)
        ,   0x00
    );

    m_index.m_bitset = bitset.release();
    assert(m_index.m_bitset);
}

BmpRowIndex::BmpRowIndex(std::size_t _height, std::vector<std::uint8_t> && _source)
    : m_height(_height), m_isBitset(true)
{
    auto bitset = std::make_unique<PocketBook::DynamicBitset>(std::move(_source));
    m_index.m_bitset = bitset.release();
    assert(m_index.m_bitset);
}

BmpRowIndex::BmpRowIndex(std::size_t _height, std::uint8_t * _indexData /* non-owning*/)
    : m_height(_height), m_isBitset(false)
{
    m_index.m_data = _indexData;
    assert(m_index.m_data);
}

BmpRowIndex::~BmpRowIndex()
{
    if(m_isBitset)
        delete m_index.m_bitset;
}

void BmpRowIndex::setRowIsEmpty(std::size_t _row, bool _val)
{
    if(_row >= m_height)
        throw std::out_of_range("Row index is out of range");

    if(m_isBitset)
    {
        m_index.m_bitset->set(_row, _val);
    }
    else
    {
        div_t pos = div(_row, DynamicBitset::BITS_PER_BLOCK);
        m_index.m_data[pos.quot] |= (1 << pos.rem);
    }
}

bool BmpRowIndex::testRowIsEmpty(std::size_t _row) const
{
    if(_row >= m_height)
        throw std::out_of_range("Row index is out of range");

    if(m_isBitset)
        return m_index.m_bitset->test(_row);

    div_t pos = div(_row, DynamicBitset::BITS_PER_BLOCK);
    return (m_index.m_data[pos.quot] & (1 << pos.rem)) != 0;
}

std::size_t BmpRowIndex::getIndexSizeInBytes() const
{
    return PocketBook::DynamicBitset::getNumBlocksRequired(m_height);
}

const std::uint8_t * BmpRowIndex::getData() const
{
    if(m_isBitset)
        return m_index.m_bitset->data();
    else
        return m_index.m_data;
}

std::vector<std::uint8_t> BmpRowIndex::getWhiteRowPattern(int _width)
{
    int padding = PocketBook::RawImageData::calculatePadding(_width);
    std::vector<std::uint8_t> whiteRowPattern(_width + padding, WHITE_PIXEL);
    for(int i = _width; i < _width + padding; ++i)
        whiteRowPattern[i] = BLACK_PIXEL; // Fill whiteRowPattern with zero padding if needed

    return whiteRowPattern;
}

BmpRowIndex BmpRowIndex::createFromRawImageData(
        const PocketBook::RawImageData & _raw
    ,   PocketBook::IProgressNotifier * _progressNotifier
    )
{
    const auto whiteRowPattern = BmpRowIndex::getWhiteRowPattern(_raw.Width);

    BmpRowIndex index(_raw.getActualHeight());
    const std::uint8_t * rowStartPtr = _raw.Data;
    for(int rowIndex = 0; rowIndex < _raw.getActualHeight(); ++rowIndex)
    {
        index.setRowIsEmpty(rowIndex, memcmp(rowStartPtr, whiteRowPattern.data(), whiteRowPattern.size()) == 0);

        rowStartPtr += _raw.getActualWidth();
        if(_progressNotifier)
        {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1ms); // For progress bar demonstration
            _progressNotifier->notifyProgress(rowIndex);
        }
    }

    return index;
}


} // namespace PocketBook
