cmake_minimum_required(VERSION 3.20)

project(catcheye-guard
    VERSION 0.1.0
    DESCRIPTION "CatchEye Guard"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(CATCHEYE_GUARD_BUILD_TESTS "Build tests" ON)
option(CATCHEYE_GUARD_BUILD_APP "Build main executable if src/main.cpp exists" ON)

# ---------------------------------------------------------------------------
# C++ standard
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Build type default
# ---------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug build")
endif()

# ---------------------------------------------------------------------------
# Compiler warnings
# ---------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
    )
elseif(MSVC)
    add_compile_options(/W4)
endif()

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(OpenCV REQUIRED)

message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")

find_package(ncnn REQUIRED)

message(STATUS "ncnn version: ${ncnn_VERSION}")
message(STATUS "ncnn include dirs: ${ncnn_INCLUDE_DIRS}")
message(STATUS "ncnn libraries: ${ncnn_LIBS}")

find_package(yaml-cpp REQUIRED)

message(STATUS "yaml-cpp version: ${yaml-cpp_VERSION}")
message(STATUS "yaml-cpp include dirs: ${yaml-cpp_INCLUDE_DIRS}")
message(STATUS "yaml-cpp libraries: ${yaml-cpp_LIBS}")

# ---------------------------------------------------------------------------
# Source collection
# ---------------------------------------------------------------------------
file(GLOB_RECURSE CATCHEYE_GUARD_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# src/main.cpp 는 라이브러리에서 제외하고, 별도 실행파일용으로 분리
set(CATCHEYE_GUARD_MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
if(EXISTS "${CATCHEYE_GUARD_MAIN_SOURCE}")
    list(REMOVE_ITEM CATCHEYE_GUARD_SOURCES "${CATCHEYE_GUARD_MAIN_SOURCE}")
endif()

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------
add_library(catcheye_guard STATIC ${CATCHEYE_GUARD_SOURCES})
add_library(catcheye::guard ALIAS catcheye_guard)

target_include_directories(catcheye_guard
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(catcheye_guard PUBLIC cxx_std_20)

# 필요하면 전역 define 추가
target_compile_definitions(catcheye_guard
    PUBLIC
        $<$<CONFIG:Debug>:CATCHEYE_DEBUG>
)

# ---------------------------------------------------------------------------
# Main executable (optional)
# ---------------------------------------------------------------------------
if(CATCHEYE_GUARD_BUILD_APP AND EXISTS "${CATCHEYE_GUARD_MAIN_SOURCE}")
    add_executable(catcheye-guard "${CATCHEYE_GUARD_MAIN_SOURCE}")

    target_link_libraries(catcheye-guard
        PRIVATE
            catcheye::guard
    )

    target_compile_features(catcheye-guard PRIVATE cxx_std_20)
endif()

# ---------------------------------------------------------------------------
# Tests (optional)
# ---------------------------------------------------------------------------
if(CATCHEYE_GUARD_BUILD_TESTS)
    include(CTest)
    enable_testing()

    file(GLOB_RECURSE CATCHEYE_GUARD_TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    )

    if(CATCHEYE_GUARD_TEST_SOURCES)
        add_executable(catcheye_guard_tests ${CATCHEYE_GUARD_TEST_SOURCES})

        target_link_libraries(catcheye_guard_tests
            PRIVATE
                catcheye::guard
        )

        target_include_directories(catcheye_guard_tests
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/include
        )

        add_test(
            NAME catcheye_guard_tests
            COMMAND catcheye_guard_tests
        )
    endif()
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS catcheye_guard
    EXPORT catcheye_guard_targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(TARGET catcheye-guard)
    install(TARGETS catcheye-guard
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT catcheye_guard_targets
    FILE catcheye_guard_targets.cmake
    NAMESPACE catcheye::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/catcheye_guard
)
