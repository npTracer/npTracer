if(NOT DEFINED NPTracerPlugin_USD_PLUG_PRIM_DIR)
    set(NPTracerPlugin_USD_PLUG_PRIM_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "NPTracer Plugin USD Plugin Primitives Directory")
endif()

set(USD_PLUG_PRIM_SOURCES
    "${NPTracerPlugin_USD_PLUG_PRIM_DIR}/NPTracerHdMesh.cpp"
    "${NPTracerPlugin_USD_PLUG_PRIM_DIR}/NPTracerHdLight.cpp"
    PARENT_SCOPE
)

set(USD_PLUG_PRIM_HEADERS
    "${NPTracerPlugin_USD_PLUG_PRIM_DIR}/NPTracerHdMesh.h"
    "${NPTracerPlugin_USD_PLUG_PRIM_DIR}/NPTracerHdLight.h"
    PARENT_SCOPE
)