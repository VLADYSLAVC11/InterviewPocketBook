
# Pocket Book Interview Task

PocketBook is a crossplatform utility written with C++ language as an interview task for Vladyslav Chelborakh 

Currently utility is tested on Linux (Debian) and MAC OS systems.

The main purpose of the application is the compressing and decompressing 8bit BMP pictures using algorithm provided in scope of customer requirements.

The algorithm is based on collecting specific index where each white line is encoded as 1'b and each non-white line as 0'b. The size of the index in bits is equal to image height( number of rows ) padded with 0b for byte alignment. Each non-white line encodes additionaly with 4 bytes pattern folowing next idea:
    - each 4 white pixels -> encoded 0
    - each 4 black pixels -> encoded 10
    - other 4 pixels -> encoed 11 + pix0 + pix1 + pix2 + pix3.

Such algorithm works good for 8bit BMP pictures which have a lot of white space and black points (for example text files).

The encoded file has *.barch extension. File format is specified as original
8bit BMP file format saving original header and color info. The main difference with BMP format is Signature = 'BA' instead of 'BM' and BmpHeader.Reserved = IndexOffset instead of 0 for BMP pictures. The IndexOffset locates where original PixelData was located, and compressed PixelData starts immediately after Index.

Format Idea:

BmpHeader  (with Signature 'BA' and IndexOffset)
InfoHeader (Containing other header specific information)
ColorInfo  (saved from original BMP picture)
IndexData  (Size of index in bits = Height)
PixelData  (Compressed PixelData)

# Build and Run

To build application it was used CMake build system configured with Ninja binaries.
Main directory PocketBook contains:
    - build.sh <ninja_path> <cmake_prefix_path>
    - run.sh
    - clean.sh scripts

The project is compiled with Qt 6.7.2 Version. Ninja and CMAKE_PREFIX_PATH should be specified for build(the path to the Qt biniaries should be added to CMAKE_PREFIX_PATH for example /home/user_name/Qt/6.7.2/gcc_64)

PocketBook Usage: ./pocketbook [options]
Description: Pocket Book application for compressing/decompressing 8bit '*.bmp' files

Options:
  -h, --help             Displays help on commandline options.
  --help-all             Displays help, including generic Qt options.
  -v, --version          Displays version information.
  -d, --dir <directory>  Scan bmp, barch and png files in <directory>.

to run application use ./run.sh script which implicitly specify images folder with
test pictures.



