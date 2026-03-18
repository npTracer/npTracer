set(CURR_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/context.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/app.cpp"
)

set(CURR_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/context.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/app.h"
)

# make it an executable for now for setting up the basic renderer
add_executable(${PROJECT_NAME} ${CURR_SOURCES} ${CURR_HEADERS})

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Vulkan::Vulkan 
    glfw 
    glm::glm
)