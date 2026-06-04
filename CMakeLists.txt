cmake_minimum_required(VERSION 3.20)

project(catcheye-guard
    VERSION 0.1.0
    DESCRIPTION "CatchEye Guard"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(CATCHEYE_GUARD_BUILD_TESTS "Build tests" OFF)
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
set(CATCHEYE_VISION_SDK_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/catcheye-vision-sdk")
if(NOT EXISTS "${CATCHEYE_VISION_SDK_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "catcheye-vision-sdk repo not found at ${CATCHEYE_VISION_SDK_SOURCE_DIR}")
endif()

set(CATCHEYE_VISION_SDK_BUILD_TESTS OFF CACHE BOOL "Build tests for SDK libraries" FORCE)
set(CATCHEYE_VISION_DETECTION_ENABLE_HAILO ON CACHE BOOL "Build HailoRT detector backend" FORCE)

find_package(OpenMP REQUIRED COMPONENTS CXX)

add_subdirectory(
    "${CATCHEYE_VISION_SDK_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/catcheye-vision-sdk"
)

find_package(OpenCV REQUIRED COMPONENTS core imgproc dnn imgcodecs videoio)
message(STATUS "OpenCV version: ${OpenCV_VERSION}")

find_package(spdlog REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPIOD REQUIRED IMPORTED_TARGET libgpiod>=2.0)
message(STATUS "libgpiod version: ${GPIOD_VERSION}")

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
        ${OpenCV_INCLUDE_DIRS}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(catcheye_guard PUBLIC cxx_std_20)

target_compile_definitions(catcheye_guard
    PUBLIC
        $<$<CONFIG:Debug>:CATCHEYE_DEBUG>
)

target_link_libraries(catcheye_guard
    PUBLIC
        catcheye::vision_input
        catcheye::vision_detection
        catcheye::vision_roi
        catcheye::vision_protocol
        catcheye::vision_runtime
        catcheye::vision_transport
        catcheye::vision_visualization
        catcheye::vision_http
        ${OpenCV_LIBS}
        lapack
        blas
        spdlog::spdlog
        PkgConfig::GPIOD
)

set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

# ---------------------------------------------------------------------------
# Main executable (optional)
# ---------------------------------------------------------------------------
if(CATCHEYE_GUARD_BUILD_APP AND EXISTS "${CATCHEYE_GUARD_MAIN_SOURCE}")
    add_executable(catcheye-guard "${CATCHEYE_GUARD_MAIN_SOURCE}")

    target_link_libraries(catcheye-guard
        PRIVATE
            catcheye::guard
    )

    target_include_directories(catcheye-guard
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
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
                ${CMAKE_CURRENT_SOURCE_DIR}/src
        )

        target_compile_definitions(catcheye_guard_tests
            PRIVATE
                CATCHEYE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
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

set(CATCHEYE_GUARD_LAUNCHER "${CMAKE_CURRENT_BINARY_DIR}/catcheye-guard")

if(TARGET catcheye-guard)
    set_target_properties(catcheye-guard PROPERTIES OUTPUT_NAME "catcheye-guard-bin")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/catcheye-guard-launcher.sh.in"
        "${CATCHEYE_GUARD_LAUNCHER}"
        @ONLY
    )
endif()

install(TARGETS catcheye_vision_input
    EXPORT catcheye_guard_targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS catcheye_vision_detection
    EXPORT catcheye_guard_targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS catcheye_vision_roi
    EXPORT catcheye_guard_targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS catcheye_vision_protocol
    EXPORT catcheye_guard_targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS catcheye_vision_transport
    EXPORT catcheye_guard_targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS catcheye_vision_runtime
    EXPORT catcheye_guard_targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS catcheye_vision_visualization
    EXPORT catcheye_guard_targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS catcheye_vision_http
    EXPORT catcheye_guard_targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

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
    install(PROGRAMS "${CATCHEYE_GUARD_LAUNCHER}"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        RENAME catcheye-guard
    )
endif()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${CATCHEYE_VISION_SDK_SOURCE_DIR}/libs/vision-input/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${CATCHEYE_VISION_SDK_SOURCE_DIR}/libs/vision-detection/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${CATCHEYE_VISION_SDK_SOURCE_DIR}/libs/vision-roi/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${CATCHEYE_VISION_SDK_SOURCE_DIR}/libs/vision-protocol/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${CATCHEYE_VISION_SDK_SOURCE_DIR}/libs/vision-transport/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${CATCHEYE_VISION_SDK_SOURCE_DIR}/libs/vision-runtime/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${CATCHEYE_VISION_SDK_SOURCE_DIR}/libs/vision-visualization/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${CATCHEYE_VISION_SDK_SOURCE_DIR}/libs/vision-http/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    DIRECTORY models
    DESTINATION ${CMAKE_INSTALL_DATADIR}/..
    PATTERN "__pycache__" EXCLUDE
    PATTERN "*.pyc" EXCLUDE
)

install(
    DIRECTORY config
    DESTINATION ${CMAKE_INSTALL_DATADIR}/..
    PATTERN "__pycache__" EXCLUDE
    PATTERN "*.pyc" EXCLUDE
)

install(PROGRAMS 
    scripts/run.sh
    scripts/run-hailo.sh
    DESTINATION scripts
)

install(FILES scripts/catcheye-guard-hailo.service
    DESTINATION scripts
)

install(EXPORT catcheye_guard_targets
    FILE catcheye_guard_targets.cmake
    NAMESPACE catcheye::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/catcheye_guard
)
