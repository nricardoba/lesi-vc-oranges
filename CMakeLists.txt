cmake_minimum_required(VERSION 3.10)
project(ProjetoVC)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_C_STANDARD 99)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenCV REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(src)

file(GLOB SOURCES "src/*.cpp" "src/*.c")

add_executable(app ${SOURCES})

target_link_libraries(app ${OpenCV_LIBS})
