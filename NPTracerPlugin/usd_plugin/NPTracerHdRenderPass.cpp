#include "usd_plugin/NPTracerHdRenderPass.h"

#include "usd_plugin/debugCodes.h"
#include "usd_plugin/hdMathUtils.h"
#include "usd_plugin/NPTracerHdRenderBufferGPU.h"

#include <pxr/imaging/hd/camera.h>
#include <pxr/imaging/hd/renderPassState.h>
#include <pxr/imaging/hd/renderBuffer.h>

#include <glm/gtc/type_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE

NPTracerHdRenderPass::NPTracerHdRenderPass(HdRenderIndex* index,
                                           HdRprimCollection const& collection,
                                           NPTracerHdRenderDelegate* delegate)
    : HdRenderPass(index, collection), _pCreator(delegate)
{
}

void NPTracerHdRenderPass::_Execute(HdRenderPassStateSharedPtr const& renderPassState,
                                    TfTokenVector const& renderTags)
{
    this->SetConverged(false);

    App* app = _pCreator->GetRendererApp();
    NPCameraRecord* cam = app->getScene()->getCamera();
    _SyncCamera(renderPassState, cam);

    this->SetConverged(false);

    HdRenderPassAovBindingVector aovBindings = renderPassState->GetAovBindings();

    for (HdRenderPassAovBinding const& aov : aovBindings)
    {
        NPTracerHdRenderBuffer* buffer = dynamic_cast<NPTracerHdRenderBuffer*>(aov.renderBuffer);
        if (!buffer)  // `dynamic_cast` failed
        {
            continue;
        }
        buffer->SetConverged(false);

        if (aov.aovName == HdAovTokens->color)
        {
        }

        buffer->SetConverged(true);
    }
    this->SetConverged(true);
}

bool NPTracerHdRenderPass::IsConverged() const
{
    return _converged.load();
}

void NPTracerHdRenderPass::SetConverged(bool converged)
{
    _converged.store(converged);
}

void NPTracerHdRenderPass::_SyncCamera(HdRenderPassStateSharedPtr const& renderPassState,
                                       NPCameraRecord* outCam) const
{
    HdCamera const* hdCam = renderPassState->GetCamera();
    outCam->model = GfMatrix4dToGLM(hdCam->GetTransform());
    outCam->view = GfMatrix4dToGLM(renderPassState->GetWorldToViewMatrix());
    outCam->proj = GfMatrix4dToGLM(renderPassState->GetProjectionMatrix());
}

PXR_NAMESPACE_CLOSE_SCOPE
