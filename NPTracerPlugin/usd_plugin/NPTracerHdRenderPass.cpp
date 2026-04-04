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
    this->SetConverged(false);

    np::App* app = _pCreator->GetRendererApp();

    HdRenderPassAovBindingVector aovBindings = renderPassState->GetAovBindings();
    TF_DEV_AXIOM(!aovBindings.empty());

    np::RendererAovs payload;

    std::vector<NPTracerHdRenderBuffer*> aovsRequestedForWrite;
    aovsRequestedForWrite.reserve(aovBindings.size());

    for (const HdRenderPassAovBinding& binding : aovBindings)
    {
        auto buffer = dynamic_cast<NPTracerHdRenderBuffer*>(binding.renderBuffer);
        if (!buffer) continue;  // `dynamic_cast` failed

        TF_DEV_AXIOM(buffer->IsConverged() && !buffer->IsMapped());

        if (binding.aovName == HdAovTokens->color)
        {
            payload.rgb = buffer->RequestImageForWrite(true);
        }
        else if (binding.aovName == HdAovTokens->depth)
        {
            payload.depth = buffer->RequestImageForWrite(true);
        }
        else
        {
            continue;  // skip pushing onto buffer vector
        }

        aovsRequestedForWrite.push_back(buffer);
    }

    // fill in camera data after all buffers have been requested
    if (_bSyncCameraPerPass)
    {
        np::CameraRecord* cam = app->getScene()->getCamera();
        sSyncCameraToState(renderPassState, cam);
    }

    if (!_resourcesCreatedFlag.load())
    {
        app->createRenderingResources(payload);
        _resourcesCreatedFlag.store(true);
    }

    app->executeDrawCall(payload);

    for (NPTracerHdRenderBuffer* buffer : aovsRequestedForWrite)
    {
        buffer->EndWrite();  // mark all requested buffers
    }

    this->SetConverged(true);

    NP_DBG("Render pass executed.\n");
}

bool NPTracerHdRenderPass::IsConverged() const
{
    return _bConverged.load();
}

void NPTracerHdRenderPass::SetConverged(bool converged)
{
    _bConverged.store(converged);
}

void NPTracerHdRenderPass::sSyncCameraToState(const HdRenderPassStateSharedPtr& renderPassState,
                                              np::CameraRecord* outCam)
{
    outCam->view = GfMatrix4dToGLM(renderPassState->GetWorldToViewMatrix());
    outCam->proj = GfMatrix4dToGLM(renderPassState->GetProjectionMatrix());
    outCam->invView = glm::inverse(outCam->view);
    outCam->invProj = glm::inverse(outCam->proj);
}

PXR_NAMESPACE_CLOSE_SCOPE
