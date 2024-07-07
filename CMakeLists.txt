cmake_minimum_required(VERSION 3.28)

project(test)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)

find_path(RENDERDOC_INCLUDE_DIR "renderdoc_app.h" HINTS "$ENV{RENDERDOC_DIR}/include")
if (RENDERDOC_INCLUDE_DIR STREQUAL "RENDERDOC_INCLUDE_DIR-NOTFOUND")
    set(USE_RENDERDOC FALSE)
else()
    set(USE_RENDERDOC TRUE)
endif()

find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)

include(CMakeParseArguments)

add_library(vc)
target_sources(vc PUBLIC FILE_SET CXX_MODULES FILES vc.cpp)
target_link_libraries(vc PUBLIC Vulkan::Vulkan)

macro(AddDemo)
    set(options)
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES SHADERS)

    cmake_parse_arguments(DEMO "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${DEMO_NAME} ${DEMO_SOURCES})
    target_link_libraries(${DEMO_NAME} vc)
    if (USE_RENDERDOC)
        target_include_directories(${DEMO_NAME} PRIVATE ${RENDERDOC_INCLUDE_DIR})
        target_compile_definitions(${DEMO_NAME} PRIVATE USE_RENDERDOC)
    endif()

    foreach(SHADER ${DEMO_SHADERS})
        set(OUTPUT_BINARY ${CMAKE_BINARY_DIR}/${SHADER}.spv)
        add_custom_command(
            OUTPUT ${OUTPUT_BINARY}
            COMMAND ${GLSLANG_VALIDATOR} -gVS -V ${CMAKE_SOURCE_DIR}/${SHADER} -o ${OUTPUT_BINARY}
            DEPENDS ${PROJECT_SOURCE_DIR}/${SHADER})
        list(APPEND ${DEMO_NAME}_SPIRV_FILES ${OUTPUT_BINARY})
    endforeach(SHADER)
    message(STATUS ${${DEMO_NAME}_SPIRV_FILES})
    add_custom_target(${DEMO_NAME}_compile_shaders DEPENDS ${${DEMO_NAME}_SPIRV_FILES})
    add_dependencies(${DEMO_NAME} ${DEMO_NAME}_compile_shaders)
endmacro()

AddDemo(
    NAME simple
	SOURCES simple.cpp
	SHADERS simple.comp
)

AddDemo(
    NAME sha256
    SOURCES sha256 sha256.cpp
    SHADERS sha256.comp
)

AddDemo(
    NAME miner
    SOURCES miner.cpp sha256.h sha256.c
    SHADERS sha256-miner.comp
)
