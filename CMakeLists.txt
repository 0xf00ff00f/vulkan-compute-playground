cmake_minimum_required(VERSION 3.22)

project(test)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)

add_executable(simple main.cpp vc.hpp vc.cpp)
target_link_libraries(simple Vulkan::Vulkan)

add_executable(sha256 sha256.cpp vc.hpp vc.cpp)
target_link_libraries(sha256 Vulkan::Vulkan)

add_executable(miner miner.cpp vc.hpp vc.cpp)
target_link_libraries(miner Vulkan::Vulkan)
