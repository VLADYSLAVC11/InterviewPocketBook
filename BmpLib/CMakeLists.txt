cmake_minimum_required(VERSION 3.16)

add_library(Bmp SHARED
        bmpdefs.h
        bmpexceptions.cpp
        bmpexceptions.h
        bmpproxy.cpp
        bmpproxy.h
        bmputils.h
        dynamicbitset.h
        dynamicbitset.cpp
        bmpproxyimpl.cpp
        bmpproxyimpl.h
        bmprowindex.cpp
        bmprowindex.h

)

install (TARGETS Bmp
        LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}/BmpLib"
        PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_BINDIR}/BmpLib"
        LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}/BmpLib"
)
