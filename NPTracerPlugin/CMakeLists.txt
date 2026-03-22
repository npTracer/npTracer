cmake_minimum_required(VERSION 3.20)

project(NPTracerPlugin)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED NPTracerPlugin_ROOT_DIR)
    set(NPTracerPlugin_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "NPTracer Plugin Root Directory")
endif()

set(NPTracerPlugin_USD_PLUG_DIR "${NPTracerPlugin_ROOT_DIR}/usd_plugin")
set(PLUG_INFO_JSON_IN_PATH "${NPTracerPlugin_USD_PLUG_DIR}/plugInfo.json.in")
set(PLUG_INFO_JSON_OUT_PATH "${CUSTOM_USD_DSO_PATH}/plugInfo.json")

add_subdirectory(usd_plugin)

set(CURR_SOURCES
    "${NPTracerPlugin_ROOT_DIR}/plugin.cpp"
    ${USD_PLUG_SOURCES}
)

set(CURR_HEADERS
    ${USD_PLUG_HEADERS}
)

add_library(${PROJECT_NAME} SHARED ${CURR_SOURCES} ${CURR_HEADERS})

target_link_libraries(${PROJECT_NAME} Houdini)
# install directly to `CUSTOM_USD_DSO_PATH`
houdini_configure_target(${PROJECT_NAME} INSTDIR "${CUSTOM_USD_DSO_PATH}")
ConfigureFilePostBuild(${PROJECT_NAME}
    IN_PATH
        "${PLUG_INFO_JSON_IN_PATH}"
    OUT_PATH
        "${PLUG_INFO_JSON_OUT_PATH}"
    GENERATOR_MODULE_PATH
        "${NPTracer_CMAKE_MODULE_PATH}/GenerateFilePostBuild.cmake"
    CONFIGURE_VARIABLES
        PLUG_INFO_LIBRARY_PATH=$<TARGET_FILE_NAME:${PROJECT_NAME}>
        PLUG_INFO_LIBRARY_NAME=$<TARGET_FILE_BASE_NAME:NPTracerPlugin>
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    VERSION="${Houdini_VERSION}"
    _USE_MATH_DEFINES
    SESI_LITTLE_ENDIAN
    SIZEOF_VOID_P=8
    MAKING_DSO
    HBOOST_ALL_NO_LIB
)

# include the root directory so that we can use `#include` with namespaces
target_include_directories(${PROJECT_NAME} PRIVATE "${NPTracerPlugin_ROOT_DIR}")

# set some useful VS settings for QOL
if(CMAKE_GENERATOR MATCHES "Visual Studio")
    set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_COMMAND "${HOUDINI_INSTALL_PATH}/bin/houdini.exe") # set houdini executable as debug target
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME}) # set as startup project (for root source directory, not current source directory)
endif()