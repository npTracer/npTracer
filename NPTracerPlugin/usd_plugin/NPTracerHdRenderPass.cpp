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

    HdRenderPassAovBindingVector aovBindings = renderPassState->GetAovBindings();

    NPRendererAovs payload;

    std::vector<NPTracerHdRenderBuffer*> requestedWriters;
    requestedWriters.reserve(aovBindings.size());

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
            payload.color = buffer->RequestImageForWrite(true);
        }
        else if (binding.aovName == HdAovTokens->depth)
        {
            payload.depth = buffer->RequestImageForWrite(true);
        }
        else
        {
            buffer->SetConverged(true);
            continue;  // skip pushing onto buffer vector
        }

        requestedWriters.push_back(buffer);
    }

    NPCameraRecord* cam = app->getScene()->getCamera();
    _SyncCamera(renderPassState, cam);  // fill in camera data after all buffers have been requested

    app->executeDrawCall(payload);

    for (NPTracerHdRenderBuffer* buffer : requestedWriters)
    {
        buffer->EndWrite();  // mark all requested buffers
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
