cmake_minimum_required(VERSION 3.24)

set(CMAKE_CONFIGURATION_TYPES "Release;RelWithDebInfo;MinSizeRel" CACHE INTERNAL "Remove `Debug` from multi-config generators due to Houdini restrictions. Also, in general, OpenUSD source code is difficult to build in `Debug` configuration regardless of OS.")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(FATAL_ERROR "`Debug` build is not supported.") # error out for single config generators
endif()

project(NPTracerPlugin)

# cache here as internal variables in case of cross-project usage
set(NPTracerPlugin_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "NPTracer Plugin Root Directory")
set(NPTracerPlugin_PROJECT_NAME "${PROJECT_NAME}"  CACHE INTERNAL "NPTracer Plugin CMake Project Name")

HouUtilsValidateConfig()

set(Houdini_DIR "${HOUDINI_INSTALL_PATH}/toolkit/cmake") # help CMake find `HoudiniConfig.cmake`
find_package(Houdini CONFIG REQUIRED GLOBAL)
HouUtilsValidatePackage() # additional validation of CMake state after `find_package`
HouUtilsAnnounceState() # announce which variables are now available after `find_package`

# Create Houdini plugin target
set(CURR_SOURCES
    "${NPTracerPlugin_ROOT_DIR}/plugin.cpp"
    "${NPTracerPlugin_ROOT_DIR}/LOP_NPTracerRenderSettings.cpp"
)

set(CURR_HEADERS
    "${NPTracerPlugin_ROOT_DIR}/LOP_NPTracerRenderSettings.h"
)

add_library(${PROJECT_NAME} SHARED ${CURR_SOURCES} ${CURR_HEADERS})
target_link_libraries(${PROJECT_NAME} PRIVATE Houdini)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    VERSION="${Houdini_VERSION}"
    _USE_MATH_DEFINES
    SESI_LITTLE_ENDIAN
    SIZEOF_VOID_P=8
    MAKING_DSO
    HBOOST_ALL_NO_LIB
)
houdini_configure_target(${PROJECT_NAME} INSTDIR "${CUSTOM_DSO_PATH}")

add_subdirectory(usdPlugin)

add_dependencies(${PROJECT_NAME} ${USD_PLUGIN_TARGET_NAME})
ConfigureDebugger(${PROJECT_NAME} "${HOUDINI_DEBUGGER_CMD}" "${HOUDINI_DEBUGGER_CMD_ARGS}" "")
