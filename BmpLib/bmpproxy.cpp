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

#define _BMP_MMAP_OPTIMIZATION_

#if __unix__ and defined(_BMP_MMAP_OPTIMIZATION_)
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#define _USE_MMAP_IMPL_
#endif


namespace
{

// Constants
static constexpr std::size_t    BPM_HEADER_OFFSET        = 0x00;
static constexpr std::size_t    INFO_HEADER_OFFSET       = sizeof(PocketBook::BmpHeader);
static constexpr std::uint16_t  UNCOMPRESSED_SIGNATURE   = 0x4D42;
static constexpr std::uint16_t  COMPRESSED_SIGNATURE     = 0x4142;
static constexpr std::uint8_t   WHITE_PIXEL              = 0xFF;
static constexpr std::uint8_t   BLACK_PIXEL              = 0x00;
static constexpr std::uint32_t  WHITE_4PIXELS            = 0xFFFFFFFF;
static constexpr std::uint32_t  BLACK_4PIXELS            = 0x00000000;

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

    static std::vector<std::uint8_t> getWhiteRowPattern(int _width)
    {
        int padding = PocketBook::RawImageData::calculatePadding(_width);
        std::vector<std::uint8_t> whiteRowPattern(_width + padding, WHITE_PIXEL);
        for(int i = _width; i < _width + padding; ++i )
            whiteRowPattern[i] = BLACK_PIXEL; // Fill whiteRowPattern with zero padding if needed

        return whiteRowPattern;
    }

    static RowIndex createFromRawImageData(
            PocketBook::RawImageData _raw
        ,   PocketBook::IProgressNotifier * _progressNotifier = nullptr
        )
    {
        const auto whiteRowPattern = RowIndex::getWhiteRowPattern(_raw.Width);

        RowIndex index(_raw.getActualHeight());
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

    PocketBook::DynamicBitset m_index;
};

bool RollbackFile(const std::string & _filePath, FILE * _file)
{
    fclose(_file);
    return remove(_filePath.c_str()) == 0;
}

} // namespace

namespace PocketBook {


// This class is used to validate Bmp/Barch files during opening process
struct BmpProxy::ProxyValidator
{
    static void validateHeader(ProxyImpl& _impl, std::size_t _fileSize);
    static void validateInfoHeader(ProxyImpl& _impl, std::size_t _fileSize);
};


#ifdef _USE_MMAP_IMPL_
struct BmpProxy::ProxyImpl
{
    ProxyImpl()
        : m_fileHandle(0), m_pHeader(nullptr)
    {
    }

    ~ProxyImpl()
    {
        if(m_pHeader)
            munmap(m_pHeader, getBmpHeader()->FileSize);

        if(m_fileHandle)
            close(m_fileHandle);
    }

    static std::unique_ptr<ProxyImpl> readImpl(const std::string& _filePath)
    {
        std::unique_ptr<ProxyImpl> impl;
        std::size_t actualFileSize = 0;

        try
        {
            impl = std::make_unique<ProxyImpl>();
            impl->m_filePath = _filePath;

            // Open file for reading
            impl->m_fileHandle = open(_filePath.c_str(), O_RDONLY);
            impl->m_fileHandle = std::max(0, impl->m_fileHandle);

            struct stat statbuf;

            // Ensure file exists and possible to read properly
            if ( !impl->m_fileHandle || fstat (impl->m_fileHandle, &statbuf) < 0 )
                throw FileDoesntExistError(_filePath);

            actualFileSize = statbuf.st_size;

            // Map opened file to memory
            impl->m_pHeader = mmap(0, actualFileSize, PROT_READ, MAP_PRIVATE, impl->m_fileHandle, 0);
            if (impl->m_pHeader == MAP_FAILED)
                throw FileOpeningError(_filePath);

            // BMP Header validation
            ProxyValidator::validateHeader(*impl, actualFileSize);

            // BMP Info Header validation
            ProxyValidator::validateInfoHeader(*impl, actualFileSize);
        }
        catch( ... )
        {
            if( impl->m_pHeader != 0 && impl->m_pHeader != MAP_FAILED )
                munmap(impl->m_pHeader, actualFileSize);

            if( impl->m_fileHandle != 0 )
                close(impl->m_fileHandle);

            impl->m_pHeader = nullptr;
            impl->m_fileHandle = 0;

            throw;
        }

        return impl;
    }

    static std::unique_ptr<ProxyImpl> readBmp(const std::string& _filePath)
    {
        return readImpl(_filePath);
    }

    static std::unique_ptr<ProxyImpl> readBarch(const std::string& _filePath)
    {
        return readImpl(_filePath);
    }

    std::string const & getFilePath() const
    {
        return m_filePath;
    }

    const std::uint8_t * getHeaderStart() const
    {
        return reinterpret_cast<const std::uint8_t *>(m_pHeader);
    }

    const BmpHeader * getBmpHeader() const
    {
        return reinterpret_cast<const BmpHeader *>(m_pHeader);
    }

    const BmpInfoHeader * getInfoHeader() const
    {
        const std::uint8_t * headerStart = getHeaderStart();
        return reinterpret_cast<const BmpInfoHeader *>(headerStart + INFO_HEADER_OFFSET);
    }

    const std::uint8_t * getPixelData() const
    {
        if(const auto * header = getBmpHeader())
            return getHeaderStart() + header->DataOffset;

        return nullptr;
    }

    const std::uint8_t * getIndexData() const
    {
        if(const auto * bmpHeader = getBmpHeader())
            return getHeaderStart() + bmpHeader->IndexOffset;

        return nullptr;
    }

    bool checkCompressedRowIsEmpty(std::size_t _rowIndex) const
    {
        const auto * infoHeader = getInfoHeader();
        if(!infoHeader)
            return false;

        if(_rowIndex >= infoHeader->Height)
            return false;

        if(const auto * rowIndex = getIndexData())
        {
            div_t pos = div(_rowIndex, DynamicBitset::BITS_PER_BLOCK);
            return (rowIndex[pos.quot] & (1 << pos.rem)) != 0;
        }

        return false;
    }

    bool copyUpToOffset(FILE * _dest, long _offset)
    {
        if(fwrite(getHeaderStart(), _offset, 1, _dest) == 1)
            return true;

        return false;
    }

    std::string m_filePath;
    int m_fileHandle;
    void * m_pHeader;
};
#else
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
        else
            ProxyValidator::validateHeader(*impl, actualFileSize);

        // BMP Info Header validation
        const bool infoHeaderReadCorrectly = (fread(&impl->m_infoHeader, sizeof(m_infoHeader), 1, impl->m_fileHandle) == 1);
        if(!infoHeaderReadCorrectly)
            throw InvalidInfoHeaderError();
        else
            ProxyValidator::validateInfoHeader(*impl, actualFileSize);


        const int padding = RawImageData::calculatePadding(impl->m_infoHeader.Width);

        // Read Pixel Data
        std::size_t realPixelDataSize = impl->m_infoHeader.ImageSize
            ? impl->m_infoHeader.ImageSize
            : impl->m_infoHeader.Height * (impl->m_infoHeader.Width + padding);

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
        else
            ProxyValidator::validateHeader(*impl, actualFileSize);

        // BMP Info Header validation
        const bool infoHeaderReadCorrectly = (fread(&impl->m_infoHeader, sizeof(m_infoHeader), 1, impl->m_fileHandle) == 1);
        if(!infoHeaderReadCorrectly)
            throw InvalidInfoHeaderError();
        else
            ProxyValidator::validateInfoHeader(*impl, actualFileSize);

        // Read Index Data
        std::vector<std::uint8_t> indexData(DynamicBitset::getNumBlocksRequired(impl->m_infoHeader.Height), 0x00);
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

    bool checkCompressedRowIsEmpty(std::size_t _rowIndex) const
    {
        const auto * index = getRowIndex();
        if(!index)
            return false;

        return index->testRowIsEmpty(_rowIndex);
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
#endif

void BmpProxy::ProxyValidator::validateHeader(BmpProxy::ProxyImpl & _impl, std::size_t _fileSize)
{
    const auto * bmpHeader = _impl.getBmpHeader();
    if(!bmpHeader)
        throw InvalidBmpHeaderError("Unable to read Header");

    const bool isBmp = (bmpHeader->Signature == UNCOMPRESSED_SIGNATURE);
    const bool isBarch = (bmpHeader->Signature == COMPRESSED_SIGNATURE);

    if(!isBmp && !isBarch) // Check Signature = 'BM'
        throw InvalidBmpHeaderError(std::string("Unexpected signature: ") + std::to_string(bmpHeader->Signature));

    if(bmpHeader->FileSize != _fileSize) // Check files size = BmpHeader.FileSize
        throw InvalidBmpHeaderError(std::string("File size mismatch: ") +
            "actual[" + std::to_string(_fileSize) + "] != expected[" + std::to_string(bmpHeader->FileSize) + "]"
        );

    if(bmpHeader->DataOffset < INFO_HEADER_OFFSET + sizeof(BmpInfoHeader)) // Check PixelData offset is valid
        throw InvalidBmpHeaderError(std::string("Invalid Data Offset: ") + std::to_string(bmpHeader->DataOffset));

    if(isBarch && bmpHeader->IndexOffset == 0) // Check barch index offset is valid
        throw InvalidBmpHeaderError(std::string("Invalid Index Offset: ") + std::to_string(bmpHeader->IndexOffset));

    if(isBarch && bmpHeader->DataOffset <= bmpHeader->IndexOffset) // Check barch PixelData offset is valid
        throw InvalidBmpHeaderError(std::string("Invalid Data Offset: ") + std::to_string(bmpHeader->DataOffset));
}

void BmpProxy::ProxyValidator::validateInfoHeader(BmpProxy::ProxyImpl & _impl, std::size_t _fileSize)
{
    const auto * bmpHeader = _impl.getBmpHeader();
    if(!bmpHeader)
        throw InvalidBmpHeaderError("Unable to read Header");

    const auto * infoHeader = _impl.getInfoHeader();
    if(!infoHeader)
        throw InvalidInfoHeaderError("Unable to read InfoHeader");

    const std::uint32_t size = infoHeader->Size;
    const std::uint32_t imageSize = infoHeader->ImageSize;
    const std::uint32_t width = infoHeader->Width;
    const std::uint32_t height = infoHeader->Height;

    if(size < sizeof(BmpInfoHeader)) // Can be > sizeof(BmpInfoHeader) in new bmp versions
        throw InvalidInfoHeaderError(std::string("Incorrect InfoHeader Size: " + std::to_string(size)));

    if(infoHeader->BitsPerPixel != 8) // Check bits per pixel = 8
        throw InvalidInfoHeaderError(std::string("Only 8bit Bmp pictures are supported"));

    const int widthPadding = RawImageData::calculatePadding(width);
    const bool isBarch = (bmpHeader->Signature == COMPRESSED_SIGNATURE);

    if(isBarch && imageSize == 0)
        throw InvalidInfoHeaderError(std::string("Unexpected Image Size: " + std::to_string(imageSize)));
    else if(!isBarch && imageSize != 0 && (height * (width + widthPadding)) != imageSize)
        throw InvalidInfoHeaderError(std::string("Unexpected Image Size: " + std::to_string(imageSize)));

    std::size_t colorTableOffset = INFO_HEADER_OFFSET + size;
    std::size_t bmpColorInfoSize = sizeof(std::uint32_t); // Bmp Color Structure

    if(bmpHeader->DataOffset < colorTableOffset + infoHeader->ColorsUsed * bmpColorInfoSize)
        throw InvalidBmpHeaderError(std::string("Invalid Data Offset: ") + std::to_string(bmpHeader->DataOffset));

    if(isBarch && bmpHeader->IndexOffset < colorTableOffset + infoHeader->ColorsUsed * bmpColorInfoSize)
        throw InvalidBmpHeaderError(std::string("Invalid Index Offset: ") + std::to_string(bmpHeader->IndexOffset));
}

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
    return getHeader().Signature == COMPRESSED_SIGNATURE;
}

std::size_t BmpProxy::getWidth() const
{
    return getInfoHeader().Width;
}

std::size_t BmpProxy::getHeight() const
{
    return getInfoHeader().Height;
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

    _out.Width = getWidth();
    _out.Height = getHeight();
    _out.Data = pixelData;

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
            _progressNotifier->init(0, rawImageData.getActualHeight() << 1);

        RowIndex index = RowIndex::createFromRawImageData(rawImageData, _progressNotifier);
        header.DataOffset = header.IndexOffset + index.getIndexSizeInBytes();

        BmpInfoHeader infoHeader = getInfoHeader();
        DynamicBitset compressedPixelData(infoHeader.ImageSize, 0x00);

        std::size_t currentBitPos = 0;
        for(int rowIndex = 0; rowIndex < rawImageData.getActualHeight(); ++rowIndex)
        {
            const auto * rawPixels = reinterpret_cast<const std::uint32_t*>(
                rawImageData.Data + rowIndex * rawImageData.getActualWidth()
            );
            if(!index.testRowIsEmpty(rowIndex))
            {
                for(int blockIndex = 0; blockIndex < rawImageData.getActualWidth() / sizeof(std::uint32_t); ++blockIndex)
                {
                    std::uint32_t blockValue = *rawPixels;
                    switch(blockValue)
                    {
                        case BLACK_4PIXELS:
                            compressedPixelData.set(currentBitPos++, true);
                            compressedPixelData.set(currentBitPos++, false);
                            break;

                        case WHITE_4PIXELS:
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
                std::this_thread::sleep_for(1ms); // For progress bar demonstration
                _progressNotifier->notifyProgress(rawImageData.getActualHeight() + rowIndex);
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

        int padding = RawImageData::calculatePadding(infoHeader.Width);
        const auto whiteRowPattern = RowIndex::getWhiteRowPattern(infoHeader.Width);

        DynamicBitset pixelDataCompressed(getPixelData(), compressedImageSize);

        std::size_t resultImageSize = infoHeader.Height * (infoHeader.Width + padding);
        std::vector<std::uint8_t> resultPixelData(resultImageSize, 0x00);
        std::uint8_t * currentRowPtr = resultPixelData.data();
        std::uint32_t currentBitPos = 0;

        if(_progressNotifier)
            _progressNotifier->init(0, infoHeader.Height);

        for(int rowIndex = 0; rowIndex < infoHeader.Height; ++rowIndex)
        {
            if(m_pImpl->checkCompressedRowIsEmpty(rowIndex))
            {
                memcpy(currentRowPtr, whiteRowPattern.data(), whiteRowPattern.size());
                currentRowPtr += whiteRowPattern.size();
            }
            else
            {
                int numBytesRestored = 0;
                while(numBytesRestored < infoHeader.Width + padding)
                {
                    std::uint32_t block = BLACK_4PIXELS;
                    bool b0 = pixelDataCompressed.test(currentBitPos++);
                    if(b0 == false)
                    {
                        block = WHITE_4PIXELS;
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
                std::this_thread::sleep_for(2ms); // For progress bar demonstration
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
