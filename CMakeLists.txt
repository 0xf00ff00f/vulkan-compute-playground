cmake_minimum_required(VERSION 3.22)

project(test)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)

add_executable(test main.cpp vc.h vc.cpp)
target_link_libraries(test Vulkan::Vulkan)
