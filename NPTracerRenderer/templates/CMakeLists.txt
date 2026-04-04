set(NPTracerRenderer_TEMPLATES_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "NPTracer Renderer Inline Templates Directory")

set(NPTracerRenderer_INLINE_TEMPLATES
    "${NPTracerRenderer_TEMPLATES_DIR}/scene.inl"
    PARENT_SCOPE
)