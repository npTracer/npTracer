set(NPTracerPlugin_USD_PLUG_LIBRARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "NPTracer Plugin USD Plugin Library Directory")

set(USD_PLUG_LIBRARY_HEADERS
    "${NPTracerPlugin_USD_PLUG_LIBRARY_DIR}/usdMath.h"
    "${NPTracerPlugin_USD_PLUG_LIBRARY_DIR}/usdMesh.h"
    PARENT_SCOPE
)