cmake_minimum_required(VERSION 3.18)

project(NPTracer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake") # add `cmake` subdirectory to module search path
mark_as_advanced(CMAKE_INSTALL_PREFIX) # for now, prefix has no usage

include(hou_utils)

HouUtilsEnsureVariables()

set(Houdini_DIR "${HOUDINI_INSTALL_PATH}/toolkit/cmake")
find_package(Houdini CONFIG REQUIRED)
HouUtilsAnnounceState() # announce which variables are now available after `find_package`

# define cache variables here in case of need for cross-project interaction later
set(NPTracerPlugin_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/NPTracerPlugin" CACHE PATH "NPTracer Plugin Root Directory")
set(NPTracerRenderer_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/NPTracerRenderer" CACHE PATH "NPTracer Renderer Root Directory")

add_subdirectory(NPTracerPlugin)
add_subdirectory(NPTracerRenderer)