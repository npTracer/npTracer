cmake_minimum_required(VERSION 3.24)

set(CMAKE_CONFIGURATION_TYPES "Release;RelWithDebInfo;MinSizeRel" CACHE STRING "Remove `Debug` from multi-config generators due to Houdini restrictions. Also, in general, OpenUSD source code is difficult to build in `Debug` configuration regardless of OS." FORCE) # `

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(FATAL_ERROR "`Debug` build is not supported.") # error out for single config generators
endif()

project(NPTracerPlugin)

set(CMAKE_CXX_STANDARD 17) # houdini libs cannot compile with c++20 currently
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED NPTracerPlugin_ROOT_DIR)
    set(NPTracerPlugin_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "NPTracer Plugin Root Directory")
endif()

option(NPTracerPlugin_USD_PLUG_DEBUG "Enable messages written to debugger output via USD's `TfDebug` class." ON)
option(SHOULD_CREATE_STANDALONE_USD_PLUGIN FALSE)
option(OVERRIDE_USD_PLUGIN_SCENE_WITH_ASSIMP FALSE)

ValidateUSDPluginCMakeOptions()

set(USD_PLUGIN_TARGET_NAME "NPTracerHdRendererPlugin")
set(STANDALONE_USD_PLUGIN_TARGET_NAME "NPTracerHdRendererPlugin_standalone")

set(NPTracerPlugin_USD_PLUG_DIR "${NPTracerPlugin_ROOT_DIR}/usd_plugin")
set(PLUG_INFO_JSON_IN_PATH "${NPTracerPlugin_USD_PLUG_DIR}/plugInfo.json.in")

if(CMAKE_GENERATOR MATCHES "Visual Studio")
    set(HOUDINI_DEBUG_CMD_ARGS ${HOUDINI_DEBUG_CMD_ARGS} CACHE STRING "Optional Command-line Arguments to use for Visual Studio Local Debugger of '${USD_PLUGIN_TARGET_NAME}' target. Command is auto-filled to Houdini executable using `HOUDINI_INSTALL_PATH`." FORCE)
    if(SHOULD_CREATE_STANDALONE_USD_PLUGIN)
        set(STANDALONE_DEBUG_CMD ${STANDALONE_DEBUG_CMD} CACHE STRING "Optional Command to use for Visual Studio Local Debugger of '${STANDALONE_USD_PLUGIN_TARGET_NAME}' target." FORCE)
        set(STANDALONE_DEBUG_CMD_ARGS ${STANDALONE_DEBUG_CMD_ARGS} CACHE STRING "Optional Command-line Arguments to use for Visual Studio Local Debugger of '${STANDALONE_USD_PLUGIN_TARGET_NAME}' target." FORCE)
    endif()
endif()

add_subdirectory(usd_plugin)

# Create Houdini plugin target
# TODO: create CMake options that make Houdini-related config optional
set(PROJECT_SOURCES
    "${NPTracerPlugin_ROOT_DIR}/plugin.cpp"
)

set(PROJECT_HEADERS
    ""
)

set(Houdini_DIR "${HOUDINI_INSTALL_PATH}/toolkit/cmake")
find_package(Houdini CONFIG REQUIRED)
HouUtilsAnnounceState() # announce which variables are now available after `find_package`

add_library(${PROJECT_NAME} SHARED ${PROJECT_SOURCES} ${PROJECT_HEADERS})
target_link_libraries(${PROJECT_NAME} 
    Houdini 
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    VERSION="${Houdini_VERSION}"
    _USE_MATH_DEFINES
    SESI_LITTLE_ENDIAN
    SIZEOF_VOID_P=8
    MAKING_DSO
    HBOOST_ALL_NO_LIB
)
houdini_configure_target(${PROJECT_NAME} INSTDIR "${HOUDINI_USD_DSO_PATH}")

ConfigureUSDPluginTarget(${USD_PLUGIN_TARGET_NAME} "${CUSTOM_USD_DSO_PATH}" 
        LIBS Houdini HoudiniThirdParty
        INCLUDES ${_houdini_include_dir}
        DEBUG_CMD "${HOUDINI_INSTALL_PATH}/bin/houdini.exe"
        DEBUG_CMD_ARGS ${HOUDINI_DEBUG_CMD_ARGS}
        INSTALL_COMPONENT "Runtime"
    )
add_dependencies(${PROJECT_NAME} ${USD_PLUGIN_TARGET_NAME})

if(SHOULD_CREATE_STANDALONE_USD_PLUGIN)
    EnsureVariable("USD_INSTALL_PATH" FALSE TRUE)
    list(PREPEND CMAKE_PREFIX_PATH "${USD_INSTALL_PATH}") # forces search at given path BEFORE system `PATH`
    find_package(pxr CONFIG REQUIRED PATHS "${USD_INSTALL_PATH}")

    ConfigureUSDPluginTarget(${STANDALONE_USD_PLUGIN_TARGET_NAME} "${CMAKE_INSTALL_PREFIX}" 
        LIBS tf hd
        INCLUDES ${PXR_INCLUDE_DIRS}
        DEBUG_CMD "${STANDALONE_DEBUG_CMD}"
        DEBUG_CMD_ARGS ${STANDALONE_DEBUG_CMD_ARGS}
        INSTALL_COMPONENT "Development"
    )
endif()