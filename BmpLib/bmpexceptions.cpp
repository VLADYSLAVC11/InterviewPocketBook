// Copyright PocketBook - Interview Task

#include "bmpexceptions.h"

namespace PocketBook {

FileError::FileError(const std::string &_message)
    : std::runtime_error(_message)
{
}

FileDoesntExistError::FileDoesntExistError(const std::string &_filePath)
    : FileError(std::string("File Error: ") + _filePath + " doesn't exist")
{
}

FileCreationError::FileCreationError(const std::string &_filePath)
    : FileError(std::string("File Error: Unable to create ") + _filePath + " file")
{
}

InvalidBmpHeaderError::InvalidBmpHeaderError(const std::string &_message)
    : FileError(_message.empty()
        ? std::string("Invalid BMP Header")
        : std::string("Invalid BMP Header: " ) + _message
)
{
}

InvalidInfoHeaderError::InvalidInfoHeaderError(const std::string &_message)
    : FileError(_message.empty()
        ? std::string("Invalid Info Header")
        : std::string("Invalid Info Header: ") + _message
)
{
}

InvalidPixelDataError::InvalidPixelDataError(const std::string &_message)
    : FileError(_message.empty()
         ? std::string("Invalid Pixel Data")
         : std::string("Invalid Pixel Data: ") + _message
)
{
}

} // namespace PocketBook