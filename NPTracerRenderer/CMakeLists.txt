set(CURR_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/context.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/app.cpp"
)

set(CURR_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/context.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/app.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils.h"
)

include(slang_utils)

set(SHADER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.slang"
)

find_program(SLANGC_EXECUTABLE slangc REQUIRED)
add_slang_shader_target(CompileShaders SOURCES ${SHADER_SOURCES})

# set shader path
set(NPTRACER_SHADER_PATH "${CMAKE_CURRENT_BINARY_DIR}/shaders/slang.spv")
configure_file(
    ${CMAKE_MODULE_PATH}/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
)

# make it an executable for now for setting up the basic renderer
add_executable(${PROJECT_NAME} ${CURR_SOURCES} ${CURR_HEADERS})

add_dependencies(${PROJECT_NAME} CompileShaders)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Vulkan::Vulkan 
    glfw 
    glm::glm
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})