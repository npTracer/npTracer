cmake_minimum_required(VERSION 3.20)

project(NPTracerRenderer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT DEFINED NPTracerRenderer_ROOT_DIR)
    set(NPTracerRenderer_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "NPTracer Renderer Root Directory")
endif()

set(COMPILE_SHADERS_TARGET CompileShaders) # create a variable for name of shader target
set(SHADER_SOURCE_DIR "${NPTracerRenderer_ROOT_DIR}/shaders")
set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/shaders")

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

set(CURR_SHADER_SPECS
    "${SHADER_SOURCE_DIR}/core.slang|vert|frag"
)

# make it an executable for now for setting up the basic renderer
add_executable(${PROJECT_NAME} ${CURR_SOURCES} ${CURR_HEADERS})

include(slang_utils)

find_program(SLANGC_EXECUTABLE slangc REQUIRED)
foreach(shader_spec ${CURR_SHADER_SPECS})
    ParseShaderSpec("${shader_spec}")
    AddSlangShaderTarget(${COMPILE_SHADERS_TARGET} "${CURR_SHADER_FILE}" "${CURR_SHADER_STAGES}" "${SHADER_OUTPUT_DIR}" ${PROJECT_NAME})
endforeach()

get_property(ALL_SPV_OUTPUTS GLOBAL PROPERTY ALL_SPV_OUTPUTS)
add_custom_target(${COMPILE_SHADERS_TARGET} DEPENDS ${ALL_SPV_OUTPUTS})

# organize into a separate folder
set_target_properties(${COMPILE_SHADERS_TARGET} PROPERTIES FOLDER "utility")

add_dependencies(${PROJECT_NAME} ${COMPILE_SHADERS_TARGET})

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Vulkan::Vulkan 
    glfw 
    glm::glm-header-only # use header-only GLM 
)