cmake_minimum_required(VERSION 3.18)

project(NPTracerPlugin)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT DEFINED NPTracerPlugin_ROOT_DIR)
    set(NPTracerPlugin_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "NPTracer Plugin Root Directory")
endif()

set(CURR_SOURCES
    "${NPTracerPlugin_ROOT_DIR}/plugin.cpp"
)

set(CURR_HEADERS
    ""
)

add_library(${PROJECT_NAME} SHARED ${CURR_SOURCES} ${CURR_HEADERS})

target_link_libraries(${PROJECT_NAME} Houdini)
houdini_configure_target(${PROJECT_NAME} INSTDIR ${CUSTOM_DSO_PATH})

target_compile_definitions(${PROJECT_NAME} PRIVATE
    VERSION="${Houdini_VERSION}"
    _USE_MATH_DEFINES
    SESI_LITTLE_ENDIAN
    SIZEOF_VOID_P=8
    MAKING_DSO
    HBOOST_ALL_NO_LIB
)

# set some useful VS settings for QOL
if(CMAKE_GENERATOR MATCHES "Visual Studio")
    set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_COMMAND "${HOUDINI_INSTALL_PATH}/bin/houdini.exe") # set houdini executable as debug target
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME}) # set as startup project (for root source directory, not current source directory)
endif()