set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

find_program(AARCH64_GCC
    NAMES
        aarch64-linux-gnu-gcc-13
)

find_program(AARCH64_GXX
    NAMES
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

if(DEFINED ENV{TARGET_SYSROOT} AND NOT "$ENV{TARGET_SYSROOT}" STREQUAL "")
    set(TARGET_SYSROOT "$ENV{TARGET_SYSROOT}" CACHE PATH "Target sysroot" FORCE)
else()
    set(TARGET_SYSROOT "/opt/sysroots/raspi" CACHE PATH "Target sysroot" FORCE)
endif()

set(CMAKE_SYSROOT "${TARGET_SYSROOT}")
set(CMAKE_SYSROOT_COMPILE "${TARGET_SYSROOT}")
set(CMAKE_SYSROOT_LINK "${TARGET_SYSROOT}")

set(CMAKE_FIND_ROOT_PATH
    "${TARGET_SYSROOT}"
    "/usr/aarch64-linux-gnu"
)
set(CMAKE_PREFIX_PATH
    "${TARGET_SYSROOT}/usr"
    CACHE STRING "Cross-compile package prefixes" FORCE
)

# Package configs are resolved from the extracted target sysroot.
set(OpenCV_DIR    "${TARGET_SYSROOT}/usr/lib/aarch64-linux-gnu/cmake/opencv4"  CACHE PATH "OpenCV CMake package directory"   FORCE)
set(ncnn_DIR      "${TARGET_SYSROOT}/usr/lib/aarch64-linux-gnu/cmake/ncnn"     CACHE PATH "ncnn CMake package directory"     FORCE)
set(yaml-cpp_DIR  "${TARGET_SYSROOT}/usr/lib/aarch64-linux-gnu/cmake/yaml-cpp" CACHE PATH "yaml-cpp CMake package directory" FORCE)
set(spdlog_DIR    "${TARGET_SYSROOT}/usr/lib/aarch64-linux-gnu/cmake/spdlog"   CACHE PATH "spdlog CMake package directory"   FORCE)
set(fmt_DIR       "${TARGET_SYSROOT}/usr/lib/aarch64-linux-gnu/cmake/fmt"      CACHE PATH "fmt CMake package directory"      FORCE)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# vision sdk uses pkg-config for libcamera/GStreamer, so point it at the target
# sysroot metadata extracted in the Docker image.
set(ENV{PKG_CONFIG_DIR} "")
set(ENV{PKG_CONFIG_PATH} "")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${TARGET_SYSROOT}")
set(ENV{PKG_CONFIG_LIBDIR}
    "${TARGET_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${TARGET_SYSROOT}/usr/share/pkgconfig"
)
