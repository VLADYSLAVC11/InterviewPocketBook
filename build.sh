#!/bin/sh

if [ -z "$1" ]; then
	echo "Ninja executable is not specified as first argument"
elif [ -z "$2" ]; then
    echo "CMAKE_PREFIX_PATH is not specified as second argument"
else
	NINJA=$1
	CMAKE_PREFIX_PATH=$2
	cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=$NINJA -DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH -G Ninja -S ../PocketBook -B ../PocketBook/cmake-build-debug
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=$NINJA -DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH -G Ninja -S ../PocketBook -B ../PocketBook/cmake-build-release
	cmake --build ../PocketBook/cmake-build-debug --target all -j6
	cmake --build ../PocketBook/cmake-build-release --target all -j6

fi