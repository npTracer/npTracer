if(NOT DEFINED NPTracerPlugin_USD_PLUG_DIR)
    set(NPTracerRenderer_TEMPLATES_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "NPTracer Renderer Inline Templates Directory")
endif()

set(NPTracerRenderer_INLINE_TEMPLATES
    "${NPTracerRenderer_TEMPLATES_DIR}/scene.inl"
    PARENT_SCOPE
)