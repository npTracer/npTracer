#include "usd_plugin/NPTracerHdRenderPass.h"

#include "usd_plugin/debugCodes.h"
#include "usd_plugin/hdMathUtils.h"

#include <pxr/imaging/hd/camera.h>
#include <pxr/imaging/hd/renderPassState.h>
#include <pxr/imaging/hd/renderBuffer.h>

#include <glm/gtc/type_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE

NPTracerHdRenderPass::NPTracerHdRenderPass(HdRenderIndex* index,
                                           const HdRprimCollection& collection,
                                           NPTracerHdRenderDelegate* delegate)
    : HdRenderPass(index, collection), _pCreator(delegate)
{
}

void NPTracerHdRenderPass::_Execute(const HdRenderPassStateSharedPtr& renderPassState,
                                    const TfTokenVector& renderTags)
{
    NP_DBG("Render pass executed.\n");

    this->SetConverged(false);

    App* app = _pCreator->GetRendererApp();

    HdRenderPassAovBindingVector aovBindings = renderPassState->GetAovBindings();
    TF_DEV_AXIOM(!aovBindings.empty());

    NPRendererAovs payload;

    std::vector<NPTracerHdRenderBuffer*> requestedWriters;
    requestedWriters.reserve(aovBindings.size());

    for (const HdRenderPassAovBinding& binding : aovBindings)
    {
        auto buffer = dynamic_cast<NPTracerHdRenderBuffer*>(binding.renderBuffer);
        if (!buffer)  // `dynamic_cast` failed
        {
            continue;
        }

        TF_DEV_AXIOM(buffer->IsConverged() && !buffer->IsMapped());

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
            continue;  // skip pushing onto buffer vector
        }

        requestedWriters.push_back(buffer);
    }

    NPCameraRecord* cam = app->getScene()->getCamera();
    _SyncCamera(renderPassState, cam);  // fill in camera data after all buffers have been requested

    // app->createRenderingResources(payload);
    // app->executeDrawCall(payload);

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

void NPTracerHdRenderPass::_SyncCamera(const HdRenderPassStateSharedPtr& renderPassState,
                                       NPCameraRecord* outCam) const
{
    const HdCamera* hdCam = renderPassState->GetCamera();
    outCam->view = GfMatrix4dToGLM(renderPassState->GetWorldToViewMatrix());
    outCam->proj = GfMatrix4dToGLM(renderPassState->GetProjectionMatrix());
}

PXR_NAMESPACE_CLOSE_SCOPE
