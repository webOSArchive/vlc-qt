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
)

# Link VLC-Qt libraries and libvlc
set(DEVICE_DIR "${WEBOS_DIR}/device")
link_directories(
    ${VLCQt_DIR}/src/core
    ${VLCQt_DIR}/src/widgets
    ${VLC_ARM_DIR}/lib
    ${DEVICE_DIR}/lib
)

# Application sources
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
    VideoProber.cpp
    VideoProber.h
    Transcoder.cpp
    Transcoder.h
    TranscodeDialog.cpp
    TranscodeDialog.h
)

# Build executable
add_executable(vlcplayer ${SOURCES})

# Link libraries
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
