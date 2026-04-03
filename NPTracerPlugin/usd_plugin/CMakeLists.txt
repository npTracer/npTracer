if(NOT DEFINED NPTracerPlugin_USD_PLUG_DIR)
    set(NPTracerPlugin_USD_PLUG_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "NPTracer Plugin USD Plugin Directory")
endif()

add_subdirectory(primitives)

set(USD_PLUG_SOURCES
    "${NPTracerPlugin_USD_PLUG_DIR}/debugCodes.cpp"
    "${NPTracerPlugin_USD_PLUG_DIR}/NPTracerHdRendererPlugin.cpp"
    "${NPTracerPlugin_USD_PLUG_DIR}/NPTracerHdRenderDelegate.cpp"
    "${NPTracerPlugin_USD_PLUG_DIR}/NPTracerHdRenderPass.cpp"
    "${NPTracerPlugin_USD_PLUG_DIR}/NPTracerHdRenderBuffer.cpp"
    "${NPTracerPlugin_USD_PLUG_DIR}/NPTracerHdRenderParam.cpp"
    ${USD_PLUG_PRIM_SOURCES}
    PARENT_SCOPE
)

set(USD_PLUG_HEADERS
    "${NPTracerPlugin_USD_PLUG_DIR}/debugCodes.h"
    "${NPTracerPlugin_USD_PLUG_DIR}/hdMathUtils.h"
    "${NPTracerPlugin_USD_PLUG_DIR}/NPTracerHdRendererPlugin.h"
    "${NPTracerPlugin_USD_PLUG_DIR}/NPTracerHdRenderDelegate.h"
    "${NPTracerPlugin_USD_PLUG_DIR}/NPTracerHdRenderPass.h"
    "${NPTracerPlugin_USD_PLUG_DIR}/NPTracerHdRenderBuffer.h"
    "${NPTracerPlugin_USD_PLUG_DIR}/NPTracerHdRenderParam.h"
    ${USD_PLUG_PRIM_HEADERS}
    PARENT_SCOPE
)