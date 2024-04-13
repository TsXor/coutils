cmake_minimum_required(VERSION 3.5)

project(coutils_example)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_subdirectory(.. build)

file(GLOB_RECURSE COUTILS_EXAMPLE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
foreach (EXAMPLE_SOURCE ${COUTILS_EXAMPLE_SOURCES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE})
    target_link_libraries(${EXAMPLE_NAME} coutils)
endforeach ()
