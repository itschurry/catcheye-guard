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

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
