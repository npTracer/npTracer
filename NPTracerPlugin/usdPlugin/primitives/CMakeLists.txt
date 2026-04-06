set(NPTracerPlugin_USD_PLUG_PRIM_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "NPTracer Plugin USD Plugin Primitives Directory")

set(USD_PLUG_PRIM_SOURCES
    "${NPTracerPlugin_USD_PLUG_PRIM_DIR}/NPTracerHdMesh.cpp"
    "${NPTracerPlugin_USD_PLUG_PRIM_DIR}/NPTracerHdLight.cpp"
    "${NPTracerPlugin_USD_PLUG_PRIM_DIR}/NPTracerHdMaterial.cpp"
    PARENT_SCOPE
)

set(USD_PLUG_PRIM_HEADERS
    "${NPTracerPlugin_USD_PLUG_PRIM_DIR}/NPTracerHdMesh.h"
    "${NPTracerPlugin_USD_PLUG_PRIM_DIR}/NPTracerHdLight.h"
    "${NPTracerPlugin_USD_PLUG_PRIM_DIR}/NPTracerHdMaterial.h"
    PARENT_SCOPE
)