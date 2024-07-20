#!/bin/sh

OS=$(uname)

if [ "$OS" = "Darwin" ]; then
    cd cmake-build-release/pocketbook.app/Contents/MacOS
    ./pocketbook --dir ../../../../images
elif [ "$OS" = "Linux" ]; then
    cd cmake-build-release
    ./pocketbook --dir ../images
else
    echo "Unknown OS: $OS"
fi