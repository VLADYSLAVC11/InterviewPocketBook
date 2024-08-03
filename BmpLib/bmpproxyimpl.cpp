// Copyright PocketBook - Interview Task

#include "bmpproxyimpl.h"
#include "bmprowindex.h"
#include "bmpexceptions.h"

namespace PocketBook {

// Bmp Proxy Impl
BmpProxy::ProxyImpl::ProxyImpl() = default;


BmpProxy::ProxyImpl::~ProxyImpl()
{
#ifdef __unix__
    if(m_pHeader)
        munmap(m_pHeader, m_fileSize);
    if(m_fileHandle)
        close(m_fileHandle);
#elif _WIN32
    if(m_pHeader)
        UnmapViewOfFile(m_pHeader);
    if(m_fileMappingHandle)
        CloseHandle(m_fileMappingHandle);
    if (m_fileHandle != INVALID_HANDLE_VALUE)
        CloseHandle(m_fileHandle);
#endif
}


std::unique_ptr<BmpProxy::ProxyImpl>
BmpProxy::ProxyImpl::readFile(const std::string & _filePath, bool _isCompressed)
{
    std::unique_ptr<ProxyImpl> impl = std::make_unique<ProxyImpl>();
    impl->m_filePath = _filePath;

#ifdef __unix__

    // Open file for reading
    auto fileHandle = open(_filePath.c_str(), O_RDONLY);

    struct stat statbuf;

    // Ensure file exists and possible to read properly
    if (fileHandle <= 0 || fstat (fileHandle, &statbuf) < 0)
        throw FileDoesntExistError(_filePath);

    impl->m_fileHandle = fileHandle;
    impl->m_fileSize = statbuf.st_size;

    // Map opened file to memory
    auto headerPtr = mmap(0, impl->m_fileSize, PROT_READ, MAP_PRIVATE, impl->m_fileHandle, 0);
    if (headerPtr == MAP_FAILED)
        throw FileOpeningError(_filePath);

    impl->m_pHeader = headerPtr;

#elif _WIN32

    // Open file for reading
    impl->m_fileHandle = CreateFileA(_filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    // Ensure file exists and possible to read properly
    LARGE_INTEGER file_size;
    if(impl->m_fileHandle == INVALID_HANDLE_VALUE || !GetFileSizeEx(impl->m_fileHandle, &file_size))
        throw FileDoesntExistError(_filePath);

    impl->m_fileSize = static_cast<std::size_t>(file_size.QuadPart);

    // Map opened file to memory
    impl->m_fileMappingHandle = CreateFileMappingA(impl->m_fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
    if(!impl->m_fileMappingHandle)
        throw FileOpeningError(_filePath);
    impl->m_pHeader = MapViewOfFile(impl->m_fileMappingHandle, FILE_MAP_READ, 0, 0, impl->m_fileSize);
    if(!impl->m_pHeader)
        throw FileOpeningError(_filePath);

#endif

    // BMP Header validation
    validateHeader(*impl, impl->m_fileSize, _isCompressed);

    // BMP Info Header validation
    validateInfoHeader(*impl, impl->m_fileSize);

    if(_isCompressed)
    {
        // Read Index Data
        impl->m_index = std::make_unique<BmpRowIndex>(
            impl->getInfoHeader()->Height
            ,   impl->getHeaderStart() + impl->getBmpHeader()->IndexOffset
            );
    }

    return impl;
}

void BmpProxy::ProxyImpl::validateHeader(BmpProxy::ProxyImpl & _impl, std::size_t _fileSize, bool _isCompressed)
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


void BmpProxy::ProxyImpl::validateInfoHeader(BmpProxy::ProxyImpl & _impl, std::size_t _fileSize)
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


const std::string & BmpProxy::ProxyImpl::getFilePath() const
{
    return m_filePath;
}


std::size_t BmpProxy::ProxyImpl::getFileSize() const
{
    return m_fileSize;
}


std::uint8_t * BmpProxy::ProxyImpl::getHeaderStart()
{
    return reinterpret_cast<std::uint8_t *>(m_pHeader);
}


const std::uint8_t * BmpProxy::ProxyImpl::getHeaderStart() const
{
    return reinterpret_cast<const std::uint8_t *>(m_pHeader);
}


const BmpHeader * BmpProxy::ProxyImpl::getBmpHeader() const
{
    return reinterpret_cast<const BmpHeader *>(m_pHeader);
}


const BmpInfoHeader * BmpProxy::ProxyImpl::getInfoHeader() const
{
    const std::uint8_t * headerStart = getHeaderStart();
    return reinterpret_cast<const BmpInfoHeader *>(headerStart + INFO_HEADER_OFFSET);
}


const std::uint8_t * BmpProxy::ProxyImpl::getPixelData() const
{
    return getHeaderStart() + getBmpHeader()->DataOffset;
}


const BmpRowIndex * BmpProxy::ProxyImpl::getRowIndex() const
{
    return m_index.get();
}


bool BmpProxy::ProxyImpl::copyBytesToFile(FILE * _dest, std::size_t _bytesCount)
{
    if(fwrite(getHeaderStart(), _bytesCount, 1, _dest) == 1)
        return true;

    return false;
}

} // namespace PocketBook
