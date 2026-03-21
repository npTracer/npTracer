cmake_minimum_required(VERSION 3.18)

project(NPTracerRenderer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT DEFINED NPTracerRenderer_ROOT_DIR)
    set(NPTracerRenderer_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "NPTracer Renderer Root Directory")
endif()

set(SHADER_TARGET CompileShaders) # create a variable for name of shader target

find_package(Vulkan REQUIRED)

set(GLFW_BUILD_DOCS OFF)
set(GLFW_INSTALL OFF)

add_subdirectory(${NPTracerRenderer_ROOT_DIR}/external/glfw)
add_subdirectory(${NPTracerRenderer_ROOT_DIR}/external/glm)

# ensure external targets are not top-level
set_target_properties(glfw PROPERTIES FOLDER "external/glfw")
set_target_properties(update_mappings PROPERTIES FOLDER "external/glfw")
set_target_properties(glm PROPERTIES FOLDER "external/glm")

set(CURR_SOURCES
    "${NPTracerRenderer_ROOT_DIR}/main.cpp"
    "${NPTracerRenderer_ROOT_DIR}/context.cpp"
    "${NPTracerRenderer_ROOT_DIR}/app.cpp"
)

set(CURR_HEADERS
    "${NPTracerRenderer_ROOT_DIR}/context.h"
    "${NPTracerRenderer_ROOT_DIR}/app.h"
    "${NPTracerRenderer_ROOT_DIR}/utils.h"
)

include(slang_utils)

set(SHADER_SOURCES
    "${NPTracerRenderer_ROOT_DIR}/shaders/shader.slang"
)

find_program(SLANGC_EXECUTABLE slangc REQUIRED)
add_slang_shader_target(${SHADER_TARGET} SOURCES ${SHADER_SOURCES})
# organize into a separate folder
set_target_properties(${SHADER_TARGET} PROPERTIES FOLDER "utility")

# set shader path
set(NPTRACER_SHADER_PATH "${CMAKE_CURRENT_SOURCE_DIR}/shaders/slang.spv")
configure_file(
    ${CMAKE_MODULE_PATH}/config.h.in
    ${CMAKE_CURRENT_BiNARY_DIR}/include/config.h
)

# make it an executable for now for setting up the basic renderer
add_executable(${PROJECT_NAME} ${CURR_SOURCES} ${CURR_HEADERS})

add_dependencies(${PROJECT_NAME} ${SHADER_TARGET})

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Vulkan::Vulkan 
    glfw 
    glm::glm-header-only # use header-only GLM 
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include)