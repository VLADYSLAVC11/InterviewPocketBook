// Copyright PocketBook - Interview Task

#pragma once

#include "bmpproxy.h"
#include "bmpdefs.h"

#ifdef __unix__
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#elif _WIN32
#include <windows.h>
#endif

namespace PocketBook {

class BmpRowIndex;

class BmpProxy::ProxyImpl
{
public:
    ProxyImpl();
    ~ProxyImpl();

    static std::unique_ptr<ProxyImpl> readFile(const std::string & _filePath, bool _isCompressed);

    const std::string & getFilePath() const;
    std::size_t getFileSize() const;

    const BmpHeader * getBmpHeader() const;
    const BmpInfoHeader * getInfoHeader() const;

    const std::uint8_t * getPixelData() const;
    const BmpRowIndex * getRowIndex() const;

    bool copyBytesToFile(FILE * _dest, std::size_t _bytesCount);

private:
    static void validateHeader(ProxyImpl& _impl, std::size_t _fileSize, bool _isCompressed);
    static void validateInfoHeader(ProxyImpl& _impl, std::size_t _fileSize);

    std::string m_filePath;
    std::size_t m_fileSize = 0;
    std::unique_ptr<BmpRowIndex> m_index;

#ifdef __unix__
    int m_fileHandle = 0;
#elif _WIN32
    HANDLE m_fileHandle = INVALID_HANDLE_VALUE;
    HANDLE m_fileMappingHandle = 0;
#endif

    void * m_pHeader = nullptr;
    std::uint8_t* getHeaderStart();
    const std::uint8_t* getHeaderStart() const;
};

} // namespace PocketBook
