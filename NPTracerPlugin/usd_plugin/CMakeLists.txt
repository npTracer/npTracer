set(USD_PLUGIN_TARGET_NAME "NPTracerHdRendererPlugin" CACHE INTERNAL "")
set(STANDALONE_USD_PLUGIN_TARGET_NAME "${USD_PLUGIN_TARGET_NAME}_standalone" CACHE INTERNAL "")

set(NPTracerPlugin_USD_PLUG_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "NPTracer Plugin USD Plugin Directory")

# options
option(SHOULD_CREATE_STANDALONE_USD_PLUGIN FALSE)
option(OVERRIDE_USD_PLUGIN_SCENE_WITH_ASSIMP FALSE)

if(SHOULD_CREATE_STANDALONE_USD_PLUGIN AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    message(FATAL_ERROR "To create a standalone USD plugin specify an installation prefix. This can be done through the `CMAKE_INSTALL_PREFIX` variable.")
endif()
if(OVERRIDE_USD_PLUGIN_SCENE_WITH_ASSIMP AND DEFINED NPTRACER_PLUGIN_ASSIMP_OVERRIDE_FILE_PATH)
    set(NPTRACER_PLUGIN_ASSIMP_OVERRIDE_FILE_PATH "${NPTRACER_PLUGIN_ASSIMP_OVERRIDE_FILE_PATH}" CACHE PATH "Path to a 3D model file on disk that will override the actual USD scene and load the file via Assimp.")
elseif(OVERRIDE_USD_PLUGIN_SCENE_WITH_ASSIMP)
    message(FATAL_ERROR "To override USD plugin scene with a 3D scene loaded from Assimp, specify a path to a valid 3D file using the `NPTRACER_PLUGIN_ASSIMP_OVERRIDE_FILE_PATH` CMake variable.")
endif()

# target configuration
add_subdirectory(primitives)

set(NPTracerPlugin_PLUG_INFO_IN_PATH "${NPTracerPlugin_USD_PLUG_DIR}/plugInfo.json.in")

set(USD_PLUG_SOURCES
    "${NPTracerPlugin_USD_PLUG_DIR}/debugCodes.cpp"
    "${NPTracerPlugin_USD_PLUG_DIR}/NPTracerHdRendererPlugin.cpp"
    "${NPTracerPlugin_USD_PLUG_DIR}/NPTracerHdRenderDelegate.cpp"
    "${NPTracerPlugin_USD_PLUG_DIR}/NPTracerHdRenderPass.cpp"
    "${NPTracerPlugin_USD_PLUG_DIR}/NPTracerHdRenderBuffer.cpp"
    "${NPTracerPlugin_USD_PLUG_DIR}/NPTracerHdRenderParam.cpp"
    ${USD_PLUG_PRIM_SOURCES}
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
)

set(COMMON_USD_CONFIGURATION_ARGS
    USD_SOURCES ${USD_PLUG_SOURCES}
    USD_HEADERS ${USD_PLUG_HEADERS}
    DEFINES
        "NPTRACER_DEBUG=$<BOOL:${CMAKE_NPTRACER_DEBUG}>"
        "ASSIMP_OVERRIDE=$<BOOL:${OVERRIDE_USD_PLUGIN_SCENE_WITH_ASSIMP}>"
        "ASSIMP_OVERRIDE_FILE_PATH=\"${NPTRACER_PLUGIN_ASSIMP_OVERRIDE_FILE_PATH}\""

    PLUG_INFO_IN_PATH "${NPTracerPlugin_PLUG_INFO_IN_PATH}"
)

ConfigureUSDPluginTarget(${USD_PLUGIN_TARGET_NAME} "${CUSTOM_USD_DSO_PATH}"
    ${COMMON_USD_CONFIGURATION_ARGS}

    LIBS ${NPTracerRenderer_PROJECT_NAME} $<LINK_ONLY:Houdini> $<LINK_ONLY:HoudiniThirdParty> # set as `LINK_ONLY` to avoid pulling in Houdini's interface compile options
    INCLUDES ${Houdini_INCLUDES} "${NPTracerPlugin_ROOT_DIR}"

    DEBUGGER_CMD "${HOUDINI_DEBUGGER_CMD}"
    DEBUGGER_CMD_ARGS "${HOUDINI_DEBUGGER_CMD_ARGS}"
    INSTALL_COMPONENT "Runtime"
)

if(SHOULD_CREATE_STANDALONE_USD_PLUGIN)
    UsdUtilsValidate()
    set(pxr_DIR "${USD_INSTALL_PATH}") # forces search at given path BEFORE system `PATH`
    find_package(pxr CONFIG REQUIRED)
    UsdUtilsAnnounceState()

    ConfigureUSDPluginTarget(${STANDALONE_USD_PLUGIN_TARGET_NAME} "${CMAKE_INSTALL_PREFIX}"
        ${COMMON_USD_CONFIGURATION_ARGS}

        LIBS ${NPTracerRenderer_PROJECT_NAME} tf hd
        INCLUDES ${PXR_INCLUDE_DIRS} "${NPTracerPlugin_ROOT_DIR}"

        DEBUGGER_CMD "${USD_STANDALONE_DEBUGGER_CMD}"
        DEBUGGER_CMD_ARGS "${USD_STANDALONE_DEBUGGER_CMD_ARGS}"
        INSTALL_COMPONENT "Development"
    )
endif()