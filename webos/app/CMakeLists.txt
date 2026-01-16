#############################################################################
# VLC Player for webOS - CMake Configuration
#############################################################################

cmake_minimum_required(VERSION 3.0.2)
project(VLCPlayerWebOS)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Position Independent Code required for Qt with -reduce-relocations
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_compile_options(-fPIC)

# Find Qt5 (OpenGL disabled - lib not available on webOS SDK)
find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)

# Find VLC-Qt (built from parent project)
set(VLCQt_DIR "${CMAKE_SOURCE_DIR}/../../build-webos" CACHE PATH "Path to VLC-Qt build")
set(VLCQt_SRC "${CMAKE_SOURCE_DIR}/../.." CACHE PATH "Path to VLC-Qt source")

# Include VLC-Qt headers and libvlc headers
set(WEBOS_DIR "${CMAKE_SOURCE_DIR}/..")
set(VLC_ARM_DIR "${WEBOS_DIR}/vlc-arm")
set(GLES2_INCLUDE "${WEBOS_DIR}/qtbase-opensource-src-5.9.7/src/3rdparty/angle/include")

# PalmPDK paths for SDL and GLES (critical for no-flicker rendering)
set(PALMPDK_DIR "/opt/PalmPDK" CACHE PATH "Path to PalmPDK")
set(PALMPDK_INCLUDE "${PALMPDK_DIR}/include")
set(PALMPDK_DEVICE_LIB "${PALMPDK_DIR}/device/lib")

include_directories(
    ${VLCQt_DIR}/include
    ${VLCQt_DIR}/src
    ${VLCQt_SRC}/src/core
    ${VLCQt_SRC}/src/widgets
    ${VLCQt_DIR}/src/core
    ${VLCQt_DIR}/src/widgets
    ${VLC_ARM_DIR}/include
    ${VLCQt_SRC}/libvlc-headers/include
    ${GLES2_INCLUDE}
    ${PALMPDK_INCLUDE}
    ${PALMPDK_INCLUDE}/SDL
    ${PALMPDK_INCLUDE}/GLES
)

# Link VLC-Qt libraries and libvlc
set(DEVICE_DIR "${WEBOS_DIR}/device")
link_directories(
    ${VLCQt_DIR}/src/core
    ${VLCQt_DIR}/src/widgets
    ${VLC_ARM_DIR}/lib
    ${DEVICE_DIR}/lib
    ${PALMPDK_DEVICE_LIB}
)

# Application sources
# NOTE: SDLVideoWidget removed - SDL video mode conflicts with Qt on webOS
# (both try to create EGL contexts). Using FBVideoWidget instead.
set(SOURCES
    main.cpp
    MainWindow.cpp
    MainWindow.h
    VideoWidget.cpp
    VideoWidget.h
    GLVideoWidget.cpp
    GLVideoWidget.h
    FBVideoWidget.cpp
    FBVideoWidget.h
    GLESVideoWidget.cpp
    GLESVideoWidget.h
)

# Build executable
add_executable(vlcplayer ${SOURCES})

# Define WEBOS for conditional compilation
target_compile_definitions(vlcplayer PRIVATE WEBOS)

# Link libraries
# NOTE: SDL/GLES_CM removed - they conflict with Qt on webOS and cause linker crashes
# Using FBVideoWidget (direct framebuffer) instead
target_link_libraries(vlcplayer
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    VLCQtCore
    VLCQtWidgets
    vlc
    dl
)

# Install
install(TARGETS vlcplayer DESTINATION bin)
