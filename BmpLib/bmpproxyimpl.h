// Copyright PocketBook - Interview Task

#pragma once

#define _BMP_MMAP_OPTIMIZATION_

#include "bmpproxy.h"
#include "bmpdefs.h"

#if __unix__ and defined(_BMP_MMAP_OPTIMIZATION_)
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#define _USE_MMAP_IMPL_
#else
#include <vector>
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

    bool copyBytesToFile(FILE * _dest, std::size_t _bytesCount); // TODO : to remove

private:
    std::string m_filePath;
    std::size_t m_fileSize = 0;

#ifdef _USE_MMAP_IMPL_
    int m_fileHandle = 0;
    void * m_pHeader = nullptr;
    std::uint8_t * getHeaderStart();
    const std::uint8_t * getHeaderStart() const;
#else
    BmpHeader m_header;
    BmpInfoHeader m_infoHeader;
    std::vector<std::uint8_t> m_pixelData;
    FILE * m_fileHandle = nullptr;
#endif

    std::unique_ptr<BmpRowIndex> m_index;
};


} // namespace PocketBook
