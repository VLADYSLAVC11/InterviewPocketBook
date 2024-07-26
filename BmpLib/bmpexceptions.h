// Copyright PocketBook - Interview Task

#pragma once

#include <stdexcept>
#include <string>

namespace PocketBook {

class FileError
    : public std::runtime_error
{
public:
    FileError(const std::string & _message);
};

class FileDoesntExistError
    : public FileError
{
public:
    FileDoesntExistError(const std::string & _filePath);
};

class FileOpeningError
    : public FileError
{
public:
    FileOpeningError(const std::string & _filePath);
};

class FileCreationError
    : public FileError
{
public:
    FileCreationError(const std::string & _filePath);
};

class InvalidBmpHeaderError
    : public FileError
{
public:
    InvalidBmpHeaderError(const std::string & _message = std::string());
};

class InvalidInfoHeaderError
    : public FileError
{
public:
    InvalidInfoHeaderError(const std::string & _message = std::string());
};

class InvalidPixelDataError
    : public FileError
{
public:
    InvalidPixelDataError(const std::string & _message = std::string());
};

} // namespace PocketBook
