// Copyright PocketBook - Interview Task

#include "dynamicbitset.h"
#include <stdexcept>

namespace PocketBook {

DynamicBitset::DynamicBitset()
	: m_size(0)
{
}

DynamicBitset::DynamicBitset(std::size_t _blocksCount, block_t _blockValue)
	: m_buffer(_blocksCount, _blockValue), m_size(0)
{
}

DynamicBitset::DynamicBitset(std::vector<block_t> && _source) noexcept
	: m_buffer(std::move(_source)), m_size(m_buffer.size() * BITS_PER_BLOCK)
{
}

DynamicBitset::DynamicBitset(const block_t * _blockPtr, std::size_t _numBlocks)
	: m_buffer(_blockPtr, _blockPtr + _numBlocks), m_size(_numBlocks * BITS_PER_BLOCK)
{
}

void DynamicBitset::set(std::size_t _bitIndex, bool _val)
{
	div_t pos = div(static_cast<int>(_bitIndex), BITS_PER_BLOCK);
	if(empty())
		m_buffer.resize(1, 0x00);
	if(pos.quot >= numBlocks())
		m_buffer.resize(pos.quot << 1, 0x00); // Resize bitset x2 for required bit

	m_buffer[pos.quot] |= (std::uint8_t )_val << pos.rem;
	if(_bitIndex >= m_size)
		m_size = _bitIndex + 1;
}

void DynamicBitset::clear()
{
	m_buffer.clear();
	m_size = 0;
}

void DynamicBitset::shrinkToFit()
{
	std::size_t numBlocksRequired = getNumBlocksRequired(m_size);
	m_buffer.resize(numBlocksRequired);
}

std::size_t DynamicBitset::size() const
{
	return m_size;
}

std::size_t DynamicBitset::numBlocks() const
{
	return m_buffer.size();
}

bool DynamicBitset::empty() const
{
	return m_buffer.empty();
}

bool DynamicBitset::test(std::size_t _bitIndex) const
{
	if(_bitIndex >= m_size)
		throw std::out_of_range("Bit index is out of range");

	div_t pos = div(static_cast<int>(_bitIndex), BITS_PER_BLOCK);
	return (m_buffer[pos.quot] & (1 << pos.rem)) != 0;
}

DynamicBitset::block_t DynamicBitset::getBlockValue(std::size_t _blockIndex) const
{
	return m_buffer.at(_blockIndex);
}

std::size_t DynamicBitset::getNumBlocksRequired(std::size_t _bitsCount)
{
	return (_bitsCount + BITS_PER_BLOCK - 1) / BITS_PER_BLOCK;
}

const DynamicBitset::block_t * DynamicBitset::data() const
{
	return m_buffer.data();
}

DynamicBitset::block_t * DynamicBitset::data()
{
	return m_buffer.data();
}

} // namespace PocketBook