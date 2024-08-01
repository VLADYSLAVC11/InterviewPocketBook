// Copyright PocketBook - Interview Task

#include "bmpproxyimpl.h"
#include "bmprowindex.h"
#include "bmpexceptions.h"

namespace PocketBook {

// This class is used to validate Bmp/Barch files during opening process
struct BmpProxy::ProxyValidator
{
    static void validateHeader(ProxyImpl& _impl, std::size_t _fileSize, bool _isCompressed);
    static void validateInfoHeader(ProxyImpl& _impl, std::size_t _fileSize);
};


void BmpProxy::ProxyValidator::validateHeader(BmpProxy::ProxyImpl & _impl, std::size_t _fileSize, bool _isCompressed)
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

    if(_isCompressed && !isBarch)
        throw std::logic_error("Compressed (*.barch) file expected");
    if(!_isCompressed && !isBmp)
        throw std::logic_error("Non compressed (*.bmp) file expected");

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


// Bmp Proxy Impl
BmpProxy::ProxyImpl::ProxyImpl() = default;


BmpProxy::ProxyImpl::~ProxyImpl()
{
#ifdef _USE_MMAP_IMPL_
    if(m_pHeader)
        munmap(m_pHeader, m_fileSize);
    if(m_fileHandle)
        close(m_fileHandle);
#else
    if(m_fileHandle)
        fclose(m_fileHandle);
#endif
}


std::unique_ptr<BmpProxy::ProxyImpl>
BmpProxy::ProxyImpl::readFile(const std::string & _filePath, bool _isCompressed)
{
    std::unique_ptr<ProxyImpl> impl = std::make_unique<ProxyImpl>();
    impl->m_filePath = _filePath;

#ifdef _USE_MMAP_IMPL_

     // Open file for reading
    impl->m_fileHandle = open(_filePath.c_str(), O_RDONLY);

    struct stat statbuf;

    // Ensure file exists and possible to read properly
    if ( impl->m_fileHandle <= 0 || fstat (impl->m_fileHandle, &statbuf) < 0 )
        throw FileDoesntExistError(_filePath);

    impl->m_fileSize = statbuf.st_size;

    // Map opened file to memory
    impl->m_pHeader = mmap(0, impl->m_fileSize, PROT_READ, MAP_PRIVATE, impl->m_fileHandle, 0);
    if (impl->m_pHeader == MAP_FAILED)
        throw FileOpeningError(_filePath);

    // BMP Header validation
    ProxyValidator::validateHeader(*impl, impl->m_fileSize, _isCompressed);

    // BMP Info Header validation
    ProxyValidator::validateInfoHeader(*impl, impl->m_fileSize);

    if(_isCompressed)
    {
        // Read Index Data
        impl->m_index = std::make_unique<BmpRowIndex>(
                impl->getInfoHeader()->Height
            ,   impl->getHeaderStart() + impl->getBmpHeader()->IndexOffset
        );
    }
#else

    // Open file in read mode
    impl->m_fileHandle = fopen( _filePath.c_str(), "rb" );
    if ( !impl->m_fileHandle )
        throw FileDoesntExistError(_filePath);

    fseek(impl->m_fileHandle, 0L, SEEK_END);
    impl->m_fileSize = ftell(impl->m_fileHandle);
    fseek(impl->m_fileHandle, 0L, SEEK_SET);

    // BMP Header validation
    const bool headerReadCorrectly = (fread(&impl->m_header, sizeof(m_header), 1, impl->m_fileHandle) == 1);
    if(!headerReadCorrectly)
        throw InvalidBmpHeaderError();
    else
        ProxyValidator::validateHeader(*impl, impl->m_fileSize, _isCompressed);

    // BMP Info Header validation
    const bool infoHeaderReadCorrectly = (fread(&impl->m_infoHeader, sizeof(m_infoHeader), 1, impl->m_fileHandle) == 1);
    if(!infoHeaderReadCorrectly)
        throw InvalidInfoHeaderError();
    else
        ProxyValidator::validateInfoHeader(*impl, impl->m_fileSize);

    if(_isCompressed)
    {
        // Read Index Data
        std::vector<std::uint8_t> indexData(DynamicBitset::getNumBlocksRequired(impl->m_infoHeader.Height), 0x00);
        fseek(impl->m_fileHandle, impl->m_header.IndexOffset, SEEK_SET);
        if(fread(indexData.data(), indexData.size(), 1, impl->m_fileHandle) != 1)
            throw InvalidPixelDataError("Unable to read Index Data");
        impl->m_index = std::make_unique<BmpRowIndex>(impl->m_infoHeader.Height, std::move(indexData));
    }

    // Read Pixel Data
    impl->m_pixelData.resize(impl->m_infoHeader.ImageSize, 0x00);
    fseek(impl->m_fileHandle, impl->m_header.DataOffset, SEEK_SET);
    const bool pixelDataReadCorrectly = (fread(impl->m_pixelData.data(), impl->m_pixelData.size(), 1, impl->m_fileHandle) == 1);
    if(!pixelDataReadCorrectly)
        throw InvalidPixelDataError("Unable to read Pixel Data");

#endif

    return impl;

}


const std::string & BmpProxy::ProxyImpl::getFilePath() const
{
    return m_filePath;
}


std::size_t BmpProxy::ProxyImpl::getFileSize() const
{
    return m_fileSize;
}


#ifdef _USE_MMAP_IMPL_
std::uint8_t * BmpProxy::ProxyImpl::getHeaderStart()
{
    return reinterpret_cast<std::uint8_t *>(m_pHeader);
}


const std::uint8_t * BmpProxy::ProxyImpl::getHeaderStart() const
{
    return reinterpret_cast<const std::uint8_t *>(m_pHeader);
}
#endif


const BmpHeader * BmpProxy::ProxyImpl::getBmpHeader() const
{
#ifdef _USE_MMAP_IMPL_
    return reinterpret_cast<const BmpHeader *>(m_pHeader);
#else
    return &m_header;
#endif
}


const BmpInfoHeader * BmpProxy::ProxyImpl::getInfoHeader() const
{
#ifdef _USE_MMAP_IMPL_
    const std::uint8_t * headerStart = getHeaderStart();
    return reinterpret_cast<const BmpInfoHeader *>(headerStart + INFO_HEADER_OFFSET);
#else
    return &m_infoHeader;
#endif
}


const std::uint8_t * BmpProxy::ProxyImpl::getPixelData() const
{
#ifdef _USE_MMAP_IMPL_
    return getHeaderStart() + getBmpHeader()->DataOffset;
#else
    return m_pixelData.data();
#endif
}


const BmpRowIndex * BmpProxy::ProxyImpl::getRowIndex() const
{
    return m_index.get();
}


bool BmpProxy::ProxyImpl::copyBytesToFile(FILE * _dest, std::size_t _bytesCount)
{
#ifdef _USE_MMAP_IMPL_
    if(fwrite(getHeaderStart(), _bytesCount, 1, _dest) == 1)
        return true;

#else
    fseek(m_fileHandle, 0, SEEK_SET);
    std::vector<std::uint8_t> dataCopied(_bytesCount, 0x00);
    if(fread(dataCopied.data(), dataCopied.size(), 1, m_fileHandle) == 1)
        if(fwrite(dataCopied.data(), dataCopied.size(), 1, _dest) == 1)
            return true;
#endif

    return false;
}







} // namespace PocketBook
