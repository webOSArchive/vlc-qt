#############################################################################
# VLC-Qt - webOS ARMv7 Cross-Compilation Toolchain
#
# Usage:
#   cmake .. -DCMAKE_TOOLCHAIN_FILE=../webos/cmake/webos-arm-toolchain.cmake
#############################################################################

# Target system
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Linaro GCC 4.9.4 Toolchain
set(TOOLCHAIN_ROOT /opt/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabi)
set(TOOLCHAIN_PREFIX arm-linux-gnueabi)

# Compilers
set(CMAKE_C_COMPILER ${TOOLCHAIN_ROOT}/bin/${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_ROOT}/bin/${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_AR ${TOOLCHAIN_ROOT}/bin/${TOOLCHAIN_PREFIX}-ar CACHE FILEPATH "Archiver")
set(CMAKE_RANLIB ${TOOLCHAIN_ROOT}/bin/${TOOLCHAIN_PREFIX}-ranlib CACHE FILEPATH "Ranlib")
set(CMAKE_STRIP ${TOOLCHAIN_ROOT}/bin/${TOOLCHAIN_PREFIX}-strip CACHE FILEPATH "Strip")

# Project paths - relative to source directory
get_filename_component(WEBOS_BASE_PATH "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(WEBOS_SDK_PATH ${WEBOS_BASE_PATH}/sdk/qt5-arm)
set(WEBOS_DEVICE_PATH ${WEBOS_BASE_PATH}/device)
set(WEBOS_SYSROOT_PATH ${WEBOS_DEVICE_PATH}/sysroot/usr)

# ARM optimization flags for Cortex-A8 (HP TouchPad)
set(ARM_FLAGS "-march=armv7-a -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp")

# Compiler flags
set(CMAKE_C_FLAGS_INIT "${ARM_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${ARM_FLAGS} -std=gnu++11")

# Include KHR platform header for OpenGL ES type definitions
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -include ${WEBOS_DEVICE_PATH}/include/KHR/khrplatform.h")

# Search paths for libraries and headers
set(CMAKE_FIND_ROOT_PATH
    ${WEBOS_SDK_PATH}
    ${WEBOS_DEVICE_PATH}
    ${WEBOS_SYSROOT_PATH}
)

# Search behavior
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Include directories
include_directories(SYSTEM
    ${WEBOS_SDK_PATH}/include
    ${WEBOS_DEVICE_PATH}/include
    ${WEBOS_SYSROOT_PATH}/include
)

# Library directories
link_directories(
    ${WEBOS_SDK_PATH}/lib
    ${WEBOS_DEVICE_PATH}/lib
    ${WEBOS_SYSROOT_PATH}/lib
)

# Qt5 configuration - point to the ARM SDK
set(QT_QMAKE_EXECUTABLE ${WEBOS_SDK_PATH}/bin/qmake)

# Qt5 runtime library path (actual ARM libraries)
set(WEBOS_QT5_RUNTIME_PATH "${WEBOS_BASE_PATH}/qt5-runtime/usr/palm/applications/com.nizovn.qt5/lib")

# CMAKE_PREFIX_PATH for find_package
set(CMAKE_PREFIX_PATH ${WEBOS_SDK_PATH}/lib/cmake ${CMAKE_PREFIX_PATH})

set(Qt5_DIR ${WEBOS_SDK_PATH}/lib/cmake/Qt5)
set(Qt5Core_DIR ${WEBOS_SDK_PATH}/lib/cmake/Qt5Core)
set(Qt5Gui_DIR ${WEBOS_SDK_PATH}/lib/cmake/Qt5Gui)
set(Qt5Widgets_DIR ${WEBOS_SDK_PATH}/lib/cmake/Qt5Widgets)
set(Qt5Quick_DIR ${WEBOS_SDK_PATH}/lib/cmake/Qt5Quick)
set(Qt5Qml_DIR ${WEBOS_SDK_PATH}/lib/cmake/Qt5Qml)
set(Qt5Network_DIR ${WEBOS_SDK_PATH}/lib/cmake/Qt5Network)
set(Qt5Test_DIR ${WEBOS_SDK_PATH}/lib/cmake/Qt5Test)

# webOS runtime paths
set(WEBOS_APP_ID "org.webosarchive.vlcplayer" CACHE STRING "webOS application ID")
set(WEBOS_APP_PATH "/media/cryptofs/apps/usr/palm/applications/${WEBOS_APP_ID}")
set(WEBOS_QT5_PATH "/media/cryptofs/apps/usr/palm/applications/com.nizovn.qt5/lib")
set(WEBOS_GLIBC_PATH "/media/cryptofs/apps/usr/palm/applications/com.nizovn.glibc/lib")
set(WEBOS_OPENSSL_PATH "/media/cryptofs/apps/usr/palm/applications/com.nizovn.openssl/lib")

# RPATH configuration for webOS deployment
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH
    "${WEBOS_APP_PATH}/lib"
    "${WEBOS_QT5_PATH}"
    "${WEBOS_GLIBC_PATH}"
    "${WEBOS_OPENSSL_PATH}"
)
set(CMAKE_INSTALL_RPATH_USE_LINK_RPATH FALSE)

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-Wl,-rpath-link,${WEBOS_SDK_PATH}/lib"
    "-Wl,-rpath-link,${WEBOS_SYSROOT_PATH}/lib"
    "-Wl,-rpath-link,${WEBOS_QT5_RUNTIME_PATH}"
    "-Wl,--allow-shlib-undefined"
    "-Wl,--dynamic-linker=${WEBOS_GLIBC_PATH}/ld.so"
)
string(REPLACE ";" " " CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT}")

set(CMAKE_SHARED_LINKER_FLAGS_INIT
    "-Wl,-rpath-link,${WEBOS_SDK_PATH}/lib"
    "-Wl,-rpath-link,${WEBOS_SYSROOT_PATH}/lib"
    "-Wl,-rpath-link,${WEBOS_QT5_RUNTIME_PATH}"
    "-Wl,--allow-shlib-undefined"
)
string(REPLACE ";" " " CMAKE_SHARED_LINKER_FLAGS_INIT "${CMAKE_SHARED_LINKER_FLAGS_INIT}")

# Link stub libraries for ABI compatibility
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -L${WEBOS_SDK_PATH}/lib -lctype_stub -lqt_resource_stub")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${CMAKE_SHARED_LINKER_FLAGS_INIT} -L${WEBOS_SDK_PATH}/lib -lctype_stub -lqt_resource_stub")

# Disable X11 (not available on webOS)
set(WITH_X11 OFF CACHE BOOL "Disable X11" FORCE)

# Platform identification
set(WEBOS TRUE)
add_definitions(-DWEBOS)
