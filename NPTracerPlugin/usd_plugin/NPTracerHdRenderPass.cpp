#include "usd_plugin/NPTracerHdRenderPass.h"

#include "usd_plugin/debugCodes.h"
#include "usd_plugin/hdMathUtils.h"

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

    HdRenderPassAovBindingVector aovBindings = renderPassState->GetAovBindings();

    NPRendererAovs payload;

    std::vector<NPTracerHdRenderBuffer*> dirtyBuffers;
    dirtyBuffers.clear();

    for (HdRenderPassAovBinding const& binding : aovBindings)
    {
        NPTracerHdRenderBuffer* buffer = dynamic_cast<NPTracerHdRenderBuffer*>(binding.renderBuffer);
        if (!buffer)  // `dynamic_cast` failed
        {
            continue;
        }
        buffer->SetConverged(false);

        if (binding.aovName == HdAovTokens->color)
        {
            payload.color = buffer->GetImage();
        }
        else if (binding.aovName == HdAovTokens->depth)
        {
            payload.depth = buffer->GetImage();
        }
        else
        {
            buffer->SetConverged(true);
            continue;  // skip pushing onto `dirtyBuffers`
        }

        dirtyBuffers.push_back(buffer);
    }

    app->executeDrawCall(payload);

    for (NPTracerHdRenderBuffer* buffer : dirtyBuffers)
    {
        buffer->SetConverged(true);  // mark all dirtied buffers
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
    outCam->model = FLOAT4X4(1.0f);  // identity; per-mesh transforms not yet supported
    outCam->view = GfMatrix4dToGLM(renderPassState->GetWorldToViewMatrix());
    outCam->proj = GfMatrix4dToGLM(renderPassState->GetProjectionMatrix());
}

PXR_NAMESPACE_CLOSE_SCOPE
