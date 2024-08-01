// Copyright PocketBook - Interview Task

#include "bmpproxyimpl.h"
#include "bmprowindex.h"
#include "bmpexceptions.h"
#include "bmputils.h"
#include <vector>
#include <thread>
#include <cassert>
#include <memory.h>


namespace
{

bool RollbackFile(const std::string & _filePath, FILE * _file)
{
    fclose(_file);
    return remove(_filePath.c_str()) == 0;
}

} // namespace

namespace PocketBook {

BmpProxy::BmpProxy(std::unique_ptr<ProxyImpl> _pImpl)
    : m_pImpl(std::move(_pImpl))
{
}

BmpProxy::BmpProxy(BmpProxy && _other) noexcept
    : m_pImpl(std::move(_other.m_pImpl))
{
}

BmpProxy::~BmpProxy() noexcept = default;

BmpProxy BmpProxy::createFromBmp(const std::string& _filePath)
{
    return BmpProxy(ProxyImpl::readFile(_filePath, false));
}

BmpProxy BmpProxy::createFromBarch(const std::string& _filePath)
{
    return BmpProxy(ProxyImpl::readFile(_filePath, true));
}

const std::string& BmpProxy::getFilePath() const
{
    return m_pImpl->getFilePath();
}

std::size_t BmpProxy::getFileSize() const
{
    return m_pImpl->getFileSize();
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
    FILE * resultFile = fopen(_outputFilePath.c_str(), "wb");
    if(!resultFile)
        throw FileCreationError(_outputFilePath);

    auto rollbackFile = [resultFile, &_outputFilePath]()
    {
        bool r = RollbackFile(_outputFilePath, resultFile);
        assert(r && "Unable to rollback file correctly");
        return false;
    };

    // Copy whole file already compressed
    if(isCompressed())
    {
        if(!m_pImpl->copyBytesToFile(resultFile, getFileSize()))
            return rollbackFile();

        fclose(resultFile);
        return true;
    }

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

        auto index = BmpRowIndex::createFromRawImageData(rawImageData, _progressNotifier);
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

        // Copy header bytes up to index offset
        if(!m_pImpl->copyBytesToFile(resultFile, header.IndexOffset))
            return rollbackFile();

        compressedPixelData.shrinkToFit();
        infoHeader.ImageSize = compressedPixelData.numBlocks();
        header.FileSize = ftell(resultFile) + index.getIndexSizeInBytes() + compressedPixelData.numBlocks();

        // Write index and compressed pixel data
        if(fwrite(index.getData(), index.getIndexSizeInBytes(), 1, resultFile) != 1 ||
           fwrite(compressedPixelData.data(), compressedPixelData.numBlocks(), 1, resultFile) != 1 ||
           ftell(resultFile) != header.FileSize)
        {
            return rollbackFile();
        }

        fseek(resultFile, 0, SEEK_SET);
        if(fwrite(&header, sizeof(BmpHeader), 1, resultFile) != 1 ||
           fwrite(&infoHeader, sizeof(BmpInfoHeader), 1, resultFile) != 1)
        {
            return rollbackFile();
        }

        // File completed
        fclose(resultFile);

    } catch( ... )
    {
        return rollbackFile();
    }

    return true;
}

bool BmpProxy::decompress(const std::string& _outputFilePath, IProgressNotifier * _progressNotifier)
{
    FILE * resultFile = fopen(_outputFilePath.c_str(), "wb");
    if(!resultFile)
        throw FileCreationError(_outputFilePath);

    auto rollbackFile = [resultFile, &_outputFilePath]()
    {
        bool r = RollbackFile(_outputFilePath, resultFile);
        assert(r && "Unable to rollback file correctly");
        return false;
    };

    // Copy whole file as decompressed
    if(!isCompressed())
    {
        if(!m_pImpl->copyBytesToFile(resultFile, getFileSize()))
            return rollbackFile();

        fclose(resultFile);
        return true;
    }

    BmpHeader header = getHeader();
    header.Signature = UNCOMPRESSED_SIGNATURE; // Specify 'BM' signature
    header.DataOffset = header.IndexOffset; // Revert Data Offset to position of Index
    header.IndexOffset = 0; // Specify Index Offset to 0 as RSVD for BMP files

    try
    {
        BmpInfoHeader infoHeader = getInfoHeader();
        std::uint32_t compressedImageSize = infoHeader.ImageSize;

        int padding = RawImageData::calculatePadding(infoHeader.Width);
        const auto whiteRowPattern = BmpRowIndex::getWhiteRowPattern(infoHeader.Width);

        DynamicBitset pixelDataCompressed(getPixelData(), compressedImageSize);

        std::size_t resultImageSize = infoHeader.Height * (infoHeader.Width + padding);
        std::vector<std::uint8_t> resultPixelData(resultImageSize, 0x00);
        std::uint8_t * currentRowPtr = resultPixelData.data();
        std::uint32_t currentBitPos = 0;

        if(_progressNotifier)
            _progressNotifier->init(0, infoHeader.Height);

        for(int rowIndex = 0; rowIndex < infoHeader.Height; ++rowIndex)
        {
            const auto * bmpRowIndex = m_pImpl->getRowIndex();
            if(bmpRowIndex && bmpRowIndex->testRowIsEmpty(rowIndex))
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

        if(!m_pImpl->copyBytesToFile(resultFile, header.DataOffset))
            return rollbackFile();

        infoHeader.ImageSize = resultImageSize;
        header.FileSize = ftell(resultFile) + resultImageSize;

        // Write Pixel Data decompressed
        if(fwrite(resultPixelData.data(), resultPixelData.size(), 1, resultFile) != 1 ||
           ftell(resultFile) != header.FileSize)
        {
            return rollbackFile();
        }

        fseek(resultFile, 0, SEEK_SET);
        if(fwrite(&header, sizeof(BmpHeader), 1, resultFile) != 1 ||
           fwrite(&infoHeader, sizeof(BmpInfoHeader), 1, resultFile) != 1)
        {
            return rollbackFile();
        }

        // File completed
        fclose(resultFile);

    } catch ( ... )
    {
        return rollbackFile();
    }

    return true;
}

} // namespace PocketBook
