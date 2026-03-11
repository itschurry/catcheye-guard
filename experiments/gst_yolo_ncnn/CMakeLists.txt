cmake_minimum_required(VERSION 3.10)
project(gst_yolo_ncnn_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)
find_package(ncnn REQUIRED)
find_package(yaml-cpp REQUIRED)

add_executable(gst_yolo_ncnn_test gst_yolo_ncnn_test.cpp)

target_link_libraries(gst_yolo_ncnn_test
    PRIVATE
    ${OpenCV_LIBS}
    ncnn
    yaml-cpp
)
