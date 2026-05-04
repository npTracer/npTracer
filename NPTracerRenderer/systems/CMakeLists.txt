set(NPTracerRenderer_SYSTEMS_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "NPTracer Renderer ECS Systems Directory")

set(NPTracerRenderer_SYSTEMS_HEADERS
    "${NPTracerRenderer_SYSTEMS_DIR}/render_system.h"
    PARENT_SCOPE
)

set(NPTracerRenderer_SYSTEMS_SOURCES
    "${NPTracerRenderer_SYSTEMS_DIR}/render_system.cpp"
    PARENT_SCOPE
)