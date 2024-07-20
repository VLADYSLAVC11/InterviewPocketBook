// Copyright PocketBook - Interview Task

#pragma once

#include <string>
#include <memory>

namespace PocketBook {

struct IProgressNotifier;
struct BmpHeader;
struct BmpInfoHeader;
struct RawImageData;

class BmpProxy
{
public:
    static BmpProxy createFromBmp(const std::string& _filePath);
    static BmpProxy createFromBarch(const std::string& _filePath);

    BmpProxy(BmpProxy && _other) noexcept;
    BmpProxy& operator = (const BmpProxy & _other) = delete;
    ~BmpProxy() noexcept;

    const std::string& getFilePath() const;
    const BmpHeader& getHeader() const;
    const BmpInfoHeader& getInfoHeader() const;

    bool isCompressed() const;

    std::size_t getWidth() const;
    std::size_t getHeight() const;

    std::uint8_t * getPixelData();
    const std::uint8_t * getPixelData() const;
    bool provideRawImageData(RawImageData & _out) const;

    bool compress(const std::string& _outputFilePath, IProgressNotifier * _progressNotifier = nullptr);
    bool decompress(const std::string& _outputFilePath, IProgressNotifier * _progressNotifier = nullptr);

private:
    struct ProxyImpl;
    std::unique_ptr<ProxyImpl> m_pImpl;
    BmpProxy(std::unique_ptr<ProxyImpl> _pImpl);
};

} // namespace PocketBook
