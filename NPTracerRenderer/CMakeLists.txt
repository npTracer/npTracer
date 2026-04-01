cmake_minimum_required(VERSION 3.20)

project(NPTracerRenderer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT DEFINED NPTracerRenderer_ROOT_DIR)
    set(NPTracerRenderer_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "NPTracer Renderer Root Directory")
endif()
set(RENDERER_TARGET_NAME ${PROJECT_NAME} CACHE INTERNAL "Name of renderer CMake target")

include(slang_utils)

set(STANDALONE_EXECUTABLE_TARGET "${PROJECT_NAME}_standalone")
set(COMPILE_SHADERS_TARGET CompileShaders) # create a variable for name of shader target
set(SHADER_SOURCE_DIR "${NPTracerRenderer_ROOT_DIR}/shaders")
set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/shaders")

find_package(Vulkan REQUIRED)

set(CURR_INSTALL_DIR "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}")
set(LIB_SOURCES
    "${NPTracerRenderer_ROOT_DIR}/context.cpp"
    "${NPTracerRenderer_ROOT_DIR}/app.cpp"
    "${NPTracerRenderer_ROOT_DIR}/scene.cpp"
)

set(LIB_HEADERS
    "${NPTracerRenderer_ROOT_DIR}/context.h"
    "${NPTracerRenderer_ROOT_DIR}/app.h"
    "${NPTracerRenderer_ROOT_DIR}/scene.h"
    "${NPTracerRenderer_ROOT_DIR}/utils.h"
    "${NPTracerRenderer_ROOT_DIR}/structs.h"
)

set(CURR_SHADER_SPECS
    "${SHADER_SOURCE_DIR}/core.slang|vert|frag"
)

set(VMA_INSTALL_DIR "${CMAKE_INSTALL_INCLUDEDIR}/vma")
set(VMA_HEADERS
    "${NPTracer_EXTERNAL_DIR}/vma/include/vk_mem_alloc.h"
)
set(STB_INSTALL_DIR "${CMAKE_INSTALL_INCLUDEDIR}/stb")
set(STB_HEADERS
    "${NPTracer_EXTERNAL_DIR}/stb/stb_image.h"
)

add_library(${PROJECT_NAME} ${LIB_SOURCES} ${LIB_HEADERS})

target_include_directories(${PROJECT_NAME}
    PUBLIC
    "$<BUILD_INTERFACE:${NPTracer_ROOT_DIR}>"
    "$<BUILD_INTERFACE:${NPTracer_EXTERNAL_DIR}/vma/include>"
    "$<BUILD_INTERFACE:${NPTracer_EXTERNAL_DIR}/stb>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    "$<INSTALL_INTERFACE:${VMA_INSTALL_DIR}>"
    "$<INSTALL_INTERFACE:${STB_INSTALL_DIR}>"
) # set the include directories based on interface

find_program(SLANGC_EXECUTABLE slangc REQUIRED)

set(ALL_SPV_OUTPUTS "")
foreach(shader_spec IN LISTS CURR_SHADER_SPECS)
    ParseShaderSpec("${shader_spec}")
    target_sources(${PROJECT_NAME} PRIVATE "${CURR_SHADER_FILE}") # add as source beforehand
    AddSlangShaderTarget(
        "${CURR_SHADER_FILE}"
        "${CURR_SHADER_STAGES}"
        "${SHADER_OUTPUT_DIR}"
        "${PROJECT_NAME}"
    )
    list(APPEND ALL_SPV_OUTPUTS ${CURR_SPV_OUTPUTS})
endforeach()

target_sources(${PROJECT_NAME} PRIVATE ${ALL_SPV_OUTPUTS})
add_custom_target(${COMPILE_SHADERS_TARGET} 
    DEPENDS ${ALL_SPV_OUTPUTS}
    COMMENT "Compiling all shaders."
)

# organize into a separate folder
set_target_properties(${COMPILE_SHADERS_TARGET} PROPERTIES FOLDER "utility")

add_dependencies(${PROJECT_NAME} ${COMPILE_SHADERS_TARGET})

target_link_libraries(${PROJECT_NAME} PUBLIC 
    glfw
    Vulkan::Vulkan 
    glm::glm-header-only # use header-only GLM
    assimp
)

target_compile_definitions(${PROJECT_NAME} PRIVATE TEXTURE_PATH="${NPTracerRenderer_ROOT_DIR}/textures")

# create a separate executable target for testing
set(EXE_SOURCES
    "${NPTracerRenderer_ROOT_DIR}/main.cpp"
)

add_executable(${STANDALONE_EXECUTABLE_TARGET} ${EXE_SOURCES})
target_link_libraries(${STANDALONE_EXECUTABLE_TARGET} PRIVATE ${PROJECT_NAME})
add_dependencies(${STANDALONE_EXECUTABLE_TARGET} ${PROJECT_NAME})

# installation of renderer as library

FilterInstallableTargets("INSTALLABLE_EXTERNAL_TARGETS" ${EXTERNAL_TARGETS})

set(PROJECT_TARGETS_NAME "${PROJECT_NAME}Targets")
set(PACKAGE_CONFIG_FILES_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
# set destination directories
install(TARGETS ${INSTALLABLE_EXTERNAL_TARGETS} ${PROJECT_NAME}
    EXPORT ${PROJECT_TARGETS_NAME}
    LIBRARY DESTINATION .
    ARCHIVE DESTINATION .
    RUNTIME DESTINATION .
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# install headers to includes directory
install(FILES ${LIB_HEADERS} DESTINATION ${CURR_INSTALL_DIR})
install(FILES ${VMA_HEADERS} DESTINATION ${VMA_INSTALL_DIR})
install(FILES ${STB_HEADERS} DESTINATION ${STB_INSTALL_DIR})

# generate targets file for downstream import
install(EXPORT ${PROJECT_TARGETS_NAME}
        FILE ${PROJECT_TARGETS_NAME}.cmake
        NAMESPACE ${PROJECT_NAME}::
        DESTINATION ${PACKAGE_CONFIG_FILES_INSTALL_DIR}
)

configure_package_config_file(${NPTracer_CMAKE_MODULE_PATH}/Config.cmake.in
  "${NPTracer_CMAKE_BINARY_PREFIX_PATH}/${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION ${PACKAGE_CONFIG_FILES_INSTALL_DIR}
)

install(FILES "${NPTracer_CMAKE_BINARY_PREFIX_PATH}/${PROJECT_NAME}Config.cmake"
    DESTINATION ${PACKAGE_CONFIG_FILES_INSTALL_DIR}
)

option(NPTRACER_STANDALONE "Compile NPTracer with standalone swapchain runtime." OFF)

target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        NPTRACER_DEBUG=$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>
        NPTRACER_STANDALONE=$<BOOL:${NPTRACER_STANDALONE}>
)