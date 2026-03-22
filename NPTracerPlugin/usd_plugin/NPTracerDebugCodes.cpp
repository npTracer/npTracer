#include "usd_plugin/NPTracerDebugCodes.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfDebug)
{
    TF_DEBUG_ENVIRONMENT_SYMBOL(NPTRACER_RENDER, "NPTracer debug logging for rendering.")
}

PXR_NAMESPACE_CLOSE_SCOPE