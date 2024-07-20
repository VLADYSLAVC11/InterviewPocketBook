// Copyright PocketBook - Interview Task

#include "bmpproxy.h"
#include "bmpdefs.h"
#include "bmputils.h"
#include "bmpexceptions.h"
#include "dynamicbitset.h"
#include <stdexcept>
#include <vector>
#include <thread>
#include <chrono>
#include <cassert>
#include <memory.h>

// TODO : Implement mmap
// For unix systems we may use mmap implementation instead of simple FILE
#if __unix__ and defined(_BMP_MMAP_OPTIMIZATION_)
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#endif

namespace
{
// Constants
static constexpr std::size_t BPM_HEADER_OFFSET = 0x00;
static constexpr std::size_t INFO_HEADER_OFFSET = sizeof(PocketBook::BmpHeader);
static constexpr std::uint16_t UNCOMPRESSED_SIGNATURE = 0x4D42;
static constexpr std::uint16_t COMPRESSED_SIGNATURE = 0x4142;

// RowIndex encodes each exact row as separate bit [0:n-1]
// The bit is set to 1 when row contains only white pixels
// The bit is set to 0 when row contains not only white pixels
struct RowIndex
{
    RowIndex(std::size_t _height)
        : m_index(_height / PocketBook::DynamicBitset::BITS_PER_BLOCK, 0x00)
    {
    }

    RowIndex(std::vector<std::uint8_t> && _source)
        : m_index(std::move(_source))
    {
    }

    void setRowIsEmpty(std::size_t _row, bool _val)
    {
        m_index.set(_row, _val);
    }

    bool testRowIsEmpty(std::size_t _row) const
    {
        return m_index.test(_row);
    }

    std::size_t getIndexSizeInBytes() const
    {
        return m_index.numBlocks();
    }

    const std::uint8_t * getData() const
    {
        return m_index.data();
    }

    std::uint8_t * getData()
    {
        return m_index.data();
    }

    static RowIndex createFromRawImageData(
        PocketBook::RawImageData _raw
        , 	PocketBook::IProgressNotifier * _progressNotifier = nullptr
    )
    {
        std::vector<std::uint8_t> whiteRowPattern(_raw.width, 0xFF);
        if(_raw.padding > 0)
            for(int i = _raw.width - _raw.padding; i < _raw.width; ++i )
                whiteRowPattern[i] = 0x00; // Fill whiteRowPattern with zero padding if needed

        RowIndex index(_raw.height);
        const std::uint8_t * rowStartPtr = _raw.data;
        for(int rowIndex = 0; rowIndex < _raw.height; ++rowIndex)
        {
            index.setRowIsEmpty(rowIndex, memcmp(rowStartPtr, whiteRowPattern.data(), _raw.width) == 0);

            rowStartPtr += _raw.width;
            if(_progressNotifier)
            {
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(2ms); // For progress bar demonstration
                _progressNotifier->notifyProgress(rowIndex);
            }
        }

        return index;
    }

    PocketBook::DynamicBitset m_index;
};

bool RollbackFile(const std::string & _filePath, FILE * _file)
{
    fclose(_file);
    return remove(_filePath.c_str()) == 0;
}
}

namespace PocketBook {

struct BmpProxy::ProxyImpl
{
    ProxyImpl()
        : m_fileHandle( nullptr )
    {
    }

    ~ProxyImpl()
    {
        if( m_fileHandle )
            fclose(m_fileHandle);
    }

    static std::unique_ptr<ProxyImpl> readBmp(const std::string& _filePath)
    {
        std::unique_ptr<ProxyImpl> impl = std::make_unique<ProxyImpl>();
        impl->m_filePath = _filePath;
        impl->m_fileHandle = fopen( _filePath.c_str(), "rb" );

        if ( !impl->m_fileHandle )
            throw FileDoesntExistError(_filePath);

        fseek(impl->m_fileHandle, 0L, SEEK_END);
        const std::size_t actualFileSize = ftell(impl->m_fileHandle);
        fseek(impl->m_fileHandle, 0L, SEEK_SET);

        // BMP Header validation
        const bool headerReadCorrectly = (fread(&impl->m_header, sizeof(m_header), 1, impl->m_fileHandle) == 1);
        if(!headerReadCorrectly)
            throw InvalidBmpHeaderError();
        else if(impl->m_header.Signature != UNCOMPRESSED_SIGNATURE) // Check Signature = 'BM'
            throw InvalidBmpHeaderError(std::string("Unexpected signature: ") + std::to_string(impl->m_header.Signature));
        else if(impl->m_header.FileSize != actualFileSize)
            throw InvalidBmpHeaderError(std::string("File size mismatch: ") +
                "actual[" + std::to_string(actualFileSize) + "] != expected[" + std::to_string(impl->m_header.FileSize) + "]"
            );
        else if(impl->m_header.DataOffset < INFO_HEADER_OFFSET + sizeof(BmpInfoHeader))
            throw InvalidBmpHeaderError(std::string("Invalid Data Offset: ") + std::to_string(impl->m_header.DataOffset));

        // BMP Info Header validation
        const bool infoHeaderReadCorrectly = (fread(&impl->m_infoHeader, sizeof(m_infoHeader), 1, impl->m_fileHandle) == 1);
        if(!infoHeaderReadCorrectly)
            throw InvalidInfoHeaderError();
        else if(impl->m_infoHeader.Size < sizeof(BmpInfoHeader)) // Can be > sizeof(BmpInfoHeader) in new bmp versions
            throw InvalidInfoHeaderError(std::string("Incorrect InfoHeader Size: " + std::to_string(impl->m_infoHeader.Size)));
        else if(impl->m_infoHeader.BitsPerPixel != 8)
            throw InvalidInfoHeaderError(std::string("Only 8bit Bmp pictures are supported"));
        else if(impl->m_infoHeader.ImageSize != 0 && impl->m_infoHeader.Height * impl->m_infoHeader.Width > impl->m_infoHeader.ImageSize)
            throw InvalidInfoHeaderError(std::string("Unexpected Image Size: " + std::to_string(impl->m_infoHeader.ImageSize)));

        std::size_t colorTableOffset = INFO_HEADER_OFFSET + impl->m_infoHeader.Size;
        std::size_t bmpColorInfoSize = sizeof(std::uint32_t ); // Bmp Color Structure

        if(impl->m_header.DataOffset < colorTableOffset + impl->m_infoHeader.ColorsUsed * bmpColorInfoSize)
            throw InvalidBmpHeaderError(std::string("Invalid Data Offset: ") + std::to_string(impl->m_header.DataOffset));

        // Read Pixel Data
        std::size_t realPixelDataSize = impl->m_infoHeader.ImageSize
                                        ? impl->m_infoHeader.ImageSize
                                        : impl->m_infoHeader.Height * impl->m_infoHeader.Width;

        impl->m_pixelData.resize(realPixelDataSize, 0x00);
        fseek(impl->m_fileHandle, impl->m_header.DataOffset, SEEK_SET);
        const bool pixelDataReadCorrectly = (fread(impl->m_pixelData.data(), impl->m_pixelData.size(), 1, impl->m_fileHandle) == 1);
        if(!pixelDataReadCorrectly)
            throw InvalidPixelDataError("Unable to read Pixel Data");

        return impl;
    }

    static std::unique_ptr<ProxyImpl> readBarch(const std::string& _filePath)
    {
        std::unique_ptr<ProxyImpl> impl = std::make_unique<ProxyImpl>();
        impl->m_filePath = _filePath;
        impl->m_fileHandle = fopen( _filePath.c_str(), "rb" );

        if ( !impl->m_fileHandle )
            throw FileDoesntExistError(_filePath);

        fseek(impl->m_fileHandle, 0L, SEEK_END);
        const std::size_t actualFileSize = ftell(impl->m_fileHandle);
        fseek(impl->m_fileHandle, 0L, SEEK_SET);

        // BMP Header validation
        const bool headerReadCorrectly = (fread(&impl->m_header, sizeof(m_header), 1, impl->m_fileHandle) == 1);
        if(!headerReadCorrectly)
            throw InvalidBmpHeaderError();
        else if(impl->m_header.Signature != COMPRESSED_SIGNATURE) // Check Signature = 'BA'
            throw InvalidBmpHeaderError(std::string("Unexpected signature: ") + std::to_string(impl->m_header.Signature));
        else if(impl->m_header.FileSize != actualFileSize)
            throw InvalidBmpHeaderError(std::string("File size mismatch: ") +
                "actual[" + std::to_string(actualFileSize) + "] != expected[" + std::to_string(impl->m_header.FileSize) + "]"
            );
        else if(impl->m_header.IndexOffset < INFO_HEADER_OFFSET + sizeof(BmpInfoHeader))
            throw InvalidBmpHeaderError(std::string("Invalid Index Offset: ") + std::to_string(impl->m_header.IndexOffset));
        else if(impl->m_header.DataOffset < impl->m_header.IndexOffset)
            throw InvalidBmpHeaderError(std::string("Invalid Data Offset: ") + std::to_string(impl->m_header.DataOffset));

        // BMP Info Header validation
        const bool infoHeaderReadCorrectly = (fread(&impl->m_infoHeader, sizeof(m_infoHeader), 1, impl->m_fileHandle) == 1);
        if(!infoHeaderReadCorrectly)
            throw InvalidInfoHeaderError();
        else if(impl->m_infoHeader.Size < sizeof(BmpInfoHeader)) // Can be > sizeof(BmpInfoHeader) in new bmp versions
            throw InvalidInfoHeaderError(std::string("Incorrect InfoHeader Size: " + std::to_string(impl->m_infoHeader.Size)));
        else if(impl->m_infoHeader.BitsPerPixel != 8)
            throw InvalidInfoHeaderError(std::string("Only 8bit Bmp pictures are supported"));

        std::size_t colorTableOffset = INFO_HEADER_OFFSET + impl->m_infoHeader.Size;
        std::size_t bmpColorInfoSize = sizeof(std::uint32_t ); // Bmp Color Structure

        if(impl->m_header.IndexOffset < colorTableOffset + impl->m_infoHeader.ColorsUsed * bmpColorInfoSize)
            throw InvalidBmpHeaderError(std::string("Invalid Index Offset: ") + std::to_string(impl->m_header.IndexOffset));

        // Read Index Data
        std::vector<std::uint8_t> indexData(impl->m_infoHeader.Height / PocketBook::DynamicBitset::BITS_PER_BLOCK, 0x00);
        fseek(impl->m_fileHandle, impl->m_header.IndexOffset, SEEK_SET);
        if(fread(indexData.data(), indexData.size(), 1, impl->m_fileHandle) != 1)
            throw InvalidPixelDataError("Unable to read Index Data");
        impl->m_index = std::make_unique<RowIndex>(std::move(indexData));

        // Read Pixel Data Compressed
        impl->m_pixelData.resize(impl->m_infoHeader.ImageSize, 0x00);
        fseek(impl->m_fileHandle, impl->m_header.DataOffset, SEEK_SET);
        const bool pixelDataReadCorrectly = (fread(impl->m_pixelData.data(), impl->m_pixelData.size(), 1, impl->m_fileHandle) == 1);
        if(!pixelDataReadCorrectly)
            throw InvalidPixelDataError("Unable to read Pixel Data");

        return impl;
    }

    std::string const & getFilePath() const
    {
        return m_filePath;
    }

    const BmpHeader * getBmpHeader() const
    {
        return &m_header;
    }

    const BmpInfoHeader * getInfoHeader() const
    {
        return &m_infoHeader;
    }

    std::uint8_t * getPixelData()
    {
        return m_pixelData.data();
    }

    const std::uint8_t * getPixelData() const
    {
        return m_pixelData.data();
    }

    const RowIndex * getRowIndex() const
    {
        return m_index.get();
    }

    bool copyUpToOffset(FILE * _dest, long _offset)
    {
        fseek(m_fileHandle, 0, SEEK_SET);
        std::vector<std::uint8_t> dataCopied(_offset, 0x00);
        if(fread(dataCopied.data(), dataCopied.size(), 1, m_fileHandle) == 1)
            if(fwrite(dataCopied.data(), dataCopied.size(), 1, _dest) == 1)
                return true;

        return false;
    }

    std::string m_filePath;
    FILE * m_fileHandle;
    BmpHeader m_header;
    BmpInfoHeader m_infoHeader;
    std::unique_ptr<RowIndex> m_index;
    std::vector<std::uint8_t> m_pixelData;
};

BmpProxy::BmpProxy(std::unique_ptr<ProxyImpl> _pImpl)
    : m_pImpl(std::move(_pImpl))
{
}

BmpProxy BmpProxy::createFromBmp(const std::string& _filePath)
{
    return BmpProxy(ProxyImpl::readBmp(_filePath));
}

BmpProxy BmpProxy::createFromBarch(const std::string& _filePath)
{
    return BmpProxy(ProxyImpl::readBarch(_filePath));
}

BmpProxy::BmpProxy(BmpProxy && _other) noexcept
    : m_pImpl(std::move(_other.m_pImpl))
{
}

BmpProxy::~BmpProxy() noexcept = default;

const std::string& BmpProxy::getFilePath() const
{
    return m_pImpl->getFilePath();
}

const BmpHeader& BmpProxy::getHeader() const
{
    return *m_pImpl->getBmpHeader();
}

const BmpInfoHeader& BmpProxy::getInfoHeader() const
{
    return *m_pImpl->getInfoHeader();
}

bool BmpProxy::isCompressed() const
{
    return m_pImpl->m_header.Signature == COMPRESSED_SIGNATURE;
}

std::size_t BmpProxy::getWidth() const
{
    return getInfoHeader().Width;
}

std::size_t BmpProxy::getHeight() const
{
    return getInfoHeader().Height;
}

std::uint8_t * BmpProxy::getPixelData()
{
    return m_pImpl->getPixelData();
}

const std::uint8_t * BmpProxy::getPixelData() const
{
    return m_pImpl->getPixelData();
}

bool BmpProxy::provideRawImageData(RawImageData & _out) const
{
    if(isCompressed())
        return false;

    const auto * pixelData = getPixelData();
    if(!pixelData)
        return false;

    memset(&_out, 0, sizeof(RawImageData));

    // Width should be multiple of 4bytes
    _out.width = getWidth();
    if(_out.width % 4 != 0)
    {
        _out.padding = 4 - (_out.width % 4);
        _out.width += _out.padding;
    }

    _out.height = getHeight();
    _out.data = pixelData;

    return true;
}

bool BmpProxy::compress(const std::string& _outputFilePath, IProgressNotifier * _progressNotifier)
{
    if(isCompressed())
        throw std::logic_error("File is already compressed");

    FILE * resultFile = fopen(_outputFilePath.c_str(), "wb");
    if(!resultFile)
        throw FileCreationError(_outputFilePath);

    auto rollbackFile = [resultFile, &_outputFilePath]() {
        bool r = RollbackFile(_outputFilePath, resultFile);
        assert(r && "Unable to rollback file correctly");
        return false;
    };

    BmpHeader header = getHeader();
    header.Signature = COMPRESSED_SIGNATURE; // Specify 'BA' signature
    header.IndexOffset = header.DataOffset; // Specify Index Offset at DataOffset

    RawImageData rawImageData;
    if(!provideRawImageData(rawImageData))
        return rollbackFile();

    try
    {
        if(_progressNotifier)
            _progressNotifier->init(0, rawImageData.height << 1);

        RowIndex index = RowIndex::createFromRawImageData(rawImageData, _progressNotifier);
        header.DataOffset = header.IndexOffset + index.getIndexSizeInBytes();

        BmpInfoHeader infoHeader = getInfoHeader();
        DynamicBitset compressedPixelData(infoHeader.ImageSize, 0x00);

        std::size_t currentBitPos = 0;
        for(int rowIndex = 0; rowIndex < rawImageData.height; ++rowIndex)
        {
            const auto * rawPixels = reinterpret_cast<const std::uint32_t*>(rawImageData.data + (rowIndex * rawImageData.width));
            if(!index.testRowIsEmpty(rowIndex))
            {
                for(int blockIndex = 0; blockIndex < rawImageData.width / sizeof(std::uint32_t); ++blockIndex)
                {
                    std::uint32_t blockValue = *rawPixels;
                    switch(blockValue)
                    {
                    case 0x00000000:
                        compressedPixelData.set(currentBitPos++, true);
                        compressedPixelData.set(currentBitPos++, false);
                        break;

                    case 0xFFFFFFFF:
                        compressedPixelData.set(currentBitPos++, false);
                        break;

                    default:
                        compressedPixelData.set(currentBitPos++, true);
                        compressedPixelData.set(currentBitPos++, true);
                        for(int bitIndex = 0; bitIndex < sizeof(std::uint32_t) * DynamicBitset::BITS_PER_BLOCK; ++bitIndex)
                        {
                            bool bitValue = (blockValue & (1 << bitIndex)) != 0;
                            compressedPixelData.set(currentBitPos++, bitValue);
                        }
                        break;
                    }

                    ++rawPixels;
                }
            }

            if(_progressNotifier)
            {
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(3ms); // For progress bar demonstration
                _progressNotifier->notifyProgress(rawImageData.height + rowIndex);
            }

        }

        if(!m_pImpl->copyUpToOffset(resultFile, header.IndexOffset))
            return rollbackFile();

        compressedPixelData.shrinkToFit();
        infoHeader.ImageSize = compressedPixelData.numBlocks();
        header.FileSize = ftell(resultFile) + index.getIndexSizeInBytes() + compressedPixelData.numBlocks();

        if(fwrite(index.getData(), index.getIndexSizeInBytes(), 1, resultFile) != 1)
            return rollbackFile();
        if(fwrite(compressedPixelData.data(), compressedPixelData.numBlocks(), 1, resultFile) != 1)
            return rollbackFile();
        if(ftell(resultFile) != header.FileSize)
            return rollbackFile();

        fseek(resultFile, 0, SEEK_SET);
        if(fwrite(&header, sizeof(BmpHeader), 1, resultFile) != 1)
            return rollbackFile();
        if(fwrite(&infoHeader, sizeof(BmpInfoHeader), 1, resultFile) != 1)
            return rollbackFile();

        // File Completed
        fclose(resultFile);

    } catch( ... )
    {
        return rollbackFile();
    }

    return true;
}

bool BmpProxy::decompress(const std::string& _outputFilePath, IProgressNotifier * _progressNotifier)
{
    if(!isCompressed())
        throw std::logic_error("File is already uncompressed");

    FILE * resultFile = fopen(_outputFilePath.c_str(), "wb");
    if(!resultFile)
        throw FileCreationError(_outputFilePath);

    auto rollbackFile = [resultFile, &_outputFilePath]() {
        bool r = RollbackFile(_outputFilePath, resultFile);
        assert(r && "Unable to rollback file correctly");
        return false;
    };

    BmpHeader header = getHeader();
    header.Signature = UNCOMPRESSED_SIGNATURE; // Specify 'BM' signature
    header.DataOffset = header.IndexOffset; // Revert Data Offset to position of Index
    header.IndexOffset = 0; // Specify Index Offset to 0 as RSVD for BMP files

    try
    {
        BmpInfoHeader infoHeader = getInfoHeader();
        std::uint32_t compressedImageSize = infoHeader.ImageSize;

        int padding = 0;
        if(infoHeader.Width % 4 != 0)
            padding = 4 - (infoHeader.Width % 4);

        std::vector<std::uint8_t> whiteRowPattern(infoHeader.Width + padding, 0xFF);
        if(padding > 0)
            for(int i = infoHeader.Width; i < infoHeader.Width + padding; ++i )
                whiteRowPattern[i] = 0x00; // Fill whiteRowPattern with zero padding if needed

        DynamicBitset pixelDataCompressed(getPixelData(), compressedImageSize);

        std::size_t resultImageSize = infoHeader.Height * (infoHeader.Width + padding);
        std::vector<std::uint8_t> resultPixelData(resultImageSize, 0x00);
        std::uint8_t * currentRowPtr = resultPixelData.data();
        std::uint32_t currentBitPos = 0;

        if(_progressNotifier)
            _progressNotifier->init(0, infoHeader.Height);

        for(int rowIndex = 0; rowIndex < infoHeader.Height; ++rowIndex)
        {
            if(m_pImpl->getRowIndex()->testRowIsEmpty(rowIndex))
            {
                memcpy(currentRowPtr, whiteRowPattern.data(), whiteRowPattern.size());
                currentRowPtr += whiteRowPattern.size();
            }
            else
            {
                int numBytesRestored = 0;
                while(numBytesRestored < infoHeader.Width + padding)
                {
                    std::uint32_t block = 0x00000000;
                    bool b0 = pixelDataCompressed.test(currentBitPos++);
                    if(b0 == false)
                    {
                        block = 0xFFFFFFFF;
                    }
                    else
                    {
                        bool b1 = pixelDataCompressed.test(currentBitPos++);
                        if(b1 == true)
                        {
                            for(int i = 0; i < sizeof(std::uint32_t) * DynamicBitset::BITS_PER_BLOCK; ++i)
                            {
                                if(pixelDataCompressed.test(currentBitPos++))
                                    block |= 1 << i;
                            }
                        }
                    }

                    memcpy(currentRowPtr, &block, sizeof(block));
                    currentRowPtr += sizeof(block);
                    numBytesRestored += sizeof(block);
                }
            }

            if(_progressNotifier)
            {
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(3ms); // For progress bar demonstration
                _progressNotifier->notifyProgress(rowIndex);
            }
        }

        pixelDataCompressed.clear();

        if(!m_pImpl->copyUpToOffset(resultFile, header.DataOffset))
            return rollbackFile();

        infoHeader.ImageSize = resultImageSize;
        header.FileSize = ftell(resultFile) + resultImageSize;

        // Write Pixel Data uncompressed
        if(fwrite(resultPixelData.data(), resultPixelData.size(), 1, resultFile) != 1)
            return rollbackFile();
        if(ftell(resultFile) != header.FileSize)
            return rollbackFile();

        fseek(resultFile, 0, SEEK_SET);
        if(fwrite(&header, sizeof(BmpHeader), 1, resultFile) != 1)
            return rollbackFile();
        if(fwrite(&infoHeader, sizeof(BmpInfoHeader), 1, resultFile) != 1)
            return rollbackFile();

        // File Completed
        fclose(resultFile);

    } catch ( ... )
    {
        return rollbackFile();
    }

    return true;
}

} // namespace PocketBook
