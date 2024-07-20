// Copyright PocketBook - Interview Task

#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

namespace PocketBook {

class DynamicBitset
{
public:
	static constexpr std::size_t BITS_PER_BLOCK = 8;
	using block_t = std::uint8_t;

	DynamicBitset();
	DynamicBitset(std::size_t _numBlocks, block_t _blockValue);
	DynamicBitset(std::vector<block_t> && _source) noexcept;
	DynamicBitset(const block_t * _blockPtr, std::size_t _numBlocks);

	void set(std::size_t _bitIndex, bool _val = true);

	void clear();
	void shrinkToFit();

	std::size_t size() const;
	std::size_t numBlocks() const;
	bool empty() const;

	bool test(std::size_t _bitIndex) const;
	block_t getBlockValue(std::size_t _blockIndex) const;

	const block_t * data() const;
	block_t * data();

	static std::size_t getNumBlocksRequired(std::size_t _bitsCount);

private:
	std::vector<block_t> m_buffer;
	std::size_t m_size;
};

} // namespace PocketBook