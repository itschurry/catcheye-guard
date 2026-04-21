set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

find_program(AARCH64_GCC
    NAMES
        aarch64-linux-gnu-gcc
        aarch64-linux-gnu-gcc-13
)

find_program(AARCH64_GXX
    NAMES
        aarch64-linux-gnu-g++
        aarch64-linux-gnu-g++-13
)

if(NOT AARCH64_GCC OR NOT AARCH64_GXX)
    message(FATAL_ERROR
        "AArch64 cross compiler not found. Install aarch64-linux-gnu-gcc/g++ "
        "or provide their full paths via CMAKE_C_COMPILER and CMAKE_CXX_COMPILER."
    )
endif()

set(CMAKE_C_COMPILER "${AARCH64_GCC}")
set(CMAKE_CXX_COMPILER "${AARCH64_GXX}")

set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)

# Debian/Ubuntu multiarch packages place CMake configs under /usr/lib/aarch64-linux-gnu/cmake.
# Pin package config directories explicitly for cross-compile dependency resolution.
set(OpenCV_DIR    "/usr/lib/aarch64-linux-gnu/cmake/opencv4"  CACHE PATH "OpenCV CMake package directory"   FORCE)
set(ncnn_DIR      "/opt/ncnn-aarch64/lib/cmake/ncnn"          CACHE PATH "ncnn CMake package directory"     FORCE)
set(yaml-cpp_DIR  "/usr/lib/aarch64-linux-gnu/cmake/yaml-cpp" CACHE PATH "yaml-cpp CMake package directory" FORCE)
set(spdlog_DIR    "/usr/lib/aarch64-linux-gnu/cmake/spdlog"   CACHE PATH "spdlog CMake package directory"   FORCE)
set(fmt_DIR       "/usr/lib/aarch64-linux-gnu/cmake/fmt"      CACHE PATH "fmt CMake package directory"      FORCE)

set(MEDIAPIPE_ROOT_DEFAULT "/opt/mediapipe-aarch64")
if(DEFINED ENV{MEDIAPIPE_ROOT} AND NOT "$ENV{MEDIAPIPE_ROOT}" STREQUAL "")
    set(MEDIAPIPE_ROOT_DEFAULT "$ENV{MEDIAPIPE_ROOT}")
endif()

set(MEDIAPIPE_ROOT
    "${MEDIAPIPE_ROOT_DEFAULT}"
    CACHE PATH "MediaPipe installation root (include/ + lib/)"
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
