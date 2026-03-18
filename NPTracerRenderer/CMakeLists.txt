cmake_minimum_required(VERSION 3.18)

project(NPTracerRenderer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT DEFINED NPTracerRenderer_ROOT_DIR)
    set(NPTracerRenderer_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "NPTracer Renderer Root Directory")
endif()

find_package(Vulkan REQUIRED)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)

add_subdirectory(${NPTracerRenderer_ROOT_DIR}/external/glfw)
add_subdirectory(${NPTracerRenderer_ROOT_DIR}/external/glm)

add_subdirectory(${NPTracerRenderer_ROOT_DIR}/src)