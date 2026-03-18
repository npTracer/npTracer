cmake_minimum_required(VERSION 3.18)

project(NPTracerRenderer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT DEFINED NPTracerRenderer_ROOT_DIR)
    set(NPTracerRenderer_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "NPTracer Renderer Root Directory")
endif()

set(CURR_SOURCES
    "${NPTracerRenderer_ROOT_DIR}/main.cpp"
)

set(CURR_HEADERS
    ""
)

add_library(${PROJECT_NAME} SHARED ${CURR_SOURCES} ${CURR_HEADERS})