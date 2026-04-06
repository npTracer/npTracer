#pragma once

#include "usdPlugin/NPTracerHdRenderDelegate.h"
#include "usdPlugin/NPTracerHdRenderBuffer.h"

#include <NPTracerRenderer/structs.h>

#include <pxr/imaging/hd/renderPass.h>

PXR_NAMESPACE_OPEN_SCOPE

class NPTracerHdRenderPass : public HdRenderPass
{
public:
    NPTracerHdRenderPass(HdRenderIndex* index, const HdRprimCollection& collection,
                         NPTracerHdRenderDelegate* delegate);

protected:
    void _Execute(const HdRenderPassStateSharedPtr& renderPassState,
                  const TfTokenVector& renderTags) override;

    bool IsConverged() const override;

    // set the convergence state
    void SetConverged(bool converged);

private:
    NPTracerHdRenderDelegate* _pCreator;

    std::atomic<bool> _bConverged{ false };

    // camera should not be synced if we are overriding
    static constexpr bool _bSyncCameraPerPass = !ASSIMP_OVERRIDE;
    static void sSyncCameraToState(const HdRenderPassStateSharedPtr& renderPassState,
                                   np::CameraRecord* outCam);

    // TEMP: only create rendering resources once
    std::atomic<bool> _resourcesCreatedFlag{ false };
};

PXR_NAMESPACE_CLOSE_SCOPE
