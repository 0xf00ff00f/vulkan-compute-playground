cmake_minimum_required(VERSION 3.22)

project(test)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)

find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)

include(CMakeParseArguments)

macro(AddDemo)
    set(options)
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES SHADERS)

    cmake_parse_arguments(DEMO "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${DEMO_NAME} ${DEMO_SOURCES})
    target_link_libraries(${DEMO_NAME} Vulkan::Vulkan)

    foreach(SHADER ${DEMO_SHADERS})
        set(OUTPUT_BINARY ${CMAKE_BINARY_DIR}/${SHADER}.spv)
        add_custom_command(
            OUTPUT ${OUTPUT_BINARY}
            COMMAND ${GLSLANG_VALIDATOR} -V ${CMAKE_SOURCE_DIR}/${SHADER} -o ${OUTPUT_BINARY}
            DEPENDS ${PROJECT_SOURCE_DIR}/${SHADER})
        list(APPEND ${DEMO_NAME}_SPIRV_FILES ${OUTPUT_BINARY})
    endforeach(SHADER)
    message(STATUS ${${DEMO_NAME}_SPIRV_FILES})
    add_custom_target(${DEMO_NAME}_compile_shaders DEPENDS ${${DEMO_NAME}_SPIRV_FILES})
    add_dependencies(${DEMO_NAME} ${DEMO_NAME}_compile_shaders)
endmacro()

AddDemo(
    NAME simple
	SOURCES main.cpp vc.cpp vc.hpp
	SHADERS simple.comp
)

AddDemo(
    NAME sha256
    SOURCES sha256 sha256.cpp vc.hpp vc.cpp
    SHADERS sha256.comp
)

AddDemo(
    NAME miner
    SOURCES miner.cpp vc.hpp vc.cpp sha256.h sha256.c
    SHADERS sha256-miner.comp
)
