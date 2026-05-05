#include "usdPlugin/NPTracerHdRenderPass.h"

#include "usdPlugin/debugCodes.h"
#include "usdPlugin/library/usdMath.h"

#include <pxr/imaging/hd/camera.h>
#include <pxr/imaging/hd/renderPassState.h>
#include <pxr/imaging/hd/renderBuffer.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_operation.hpp>

PXR_NAMESPACE_OPEN_SCOPE

NPTracerHdRenderPass::NPTracerHdRenderPass(HdRenderIndex* index,
                                           const HdRprimCollection& collection,
                                           NPTracerHdRenderDelegate* delegate)
    : HdRenderPass(index, collection), _pCreator(delegate)
{
}

bool NPTracerHdRenderPass::IsConverged() const
{
    bool result = (_numRenderPassesExecuted.load() < 2) ? false : _bConverged.load();
    return result;
}

void NPTracerHdRenderPass::_Execute(const HdRenderPassStateSharedPtr& renderPassState,
                                    const TfTokenVector& renderTags)
{
    std::lock_guard<std::mutex> lock(_executionMutex);

    this->SetConverged(false);

    np::App* app = _pCreator->GetApp();

    HdRenderPassAovBindingVector aovBindings = renderPassState->GetAovBindings();
    TF_DEV_AXIOM(!aovBindings.empty());

    np::RendererTargets payload;

    std::vector<NPTracerHdRenderBuffer*> aovsRequestedForWrite;
    aovsRequestedForWrite.reserve(aovBindings.size());

    for (const HdRenderPassAovBinding& binding : aovBindings)
    {
        auto buffer = dynamic_cast<NPTracerHdRenderBuffer*>(binding.renderBuffer);
        if (!buffer) continue;  // `dynamic_cast` failed

        TF_DEV_AXIOM(buffer->IsConverged() && !buffer->IsMapped());

        if (binding.aovName == HdAovTokens->color)
            payload.color = buffer->RequestImageForWrite(true);
        else if (binding.aovName == HdAovTokens->depth)
            payload.depth = buffer->RequestImageForWrite(true);
        else continue;  // skip pushing onto buffer vector

        aovsRequestedForWrite.push_back(buffer);
    }

    // fill in camera data after all buffers have been requested
    if (_bSyncCameraPerPass)
    {
        np::Camera* cam = app->getScene()->getCamera();
        sSyncCameraToState(renderPassState, cam);
    }

    if (!_bResourcesCreatedFlag.load())
    {
        app->createRenderingResources(payload);
        _bResourcesCreatedFlag.store(true);
    }

    app->executeDrawCall(payload);

    for (NPTracerHdRenderBuffer* buffer : aovsRequestedForWrite)
        buffer->EndWrite();  // mark all requested buffers

    this->SetConverged(true);

    _numRenderPassesExecuted.fetch_add(1u);

    NP_DBG("Render pass executed.\n");
}

void NPTracerHdRenderPass::SetConverged(bool converged)
{
    _bConverged.store(converged);
}

void NPTracerHdRenderPass::sSyncCameraToState(const HdRenderPassStateSharedPtr& renderPassState,
                                              np::Camera* outCam)
{
    outCam->view = GfToGLMMat4f(renderPassState->GetWorldToViewMatrix());
    outCam->proj = GfToGLMMat4f(renderPassState->GetProjectionMatrix());
    outCam->invView = glm::inverse(outCam->view);
    outCam->invProj = glm::inverse(outCam->proj);
}

PXR_NAMESPACE_CLOSE_SCOPE
