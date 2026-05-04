#pragma once

#include "usdPlugin/NPTracerHdRenderDelegate.h"
#include "usdPlugin/NPTracerHdRenderBuffer.h"

#include <NPTracerRenderer/structs.h>

#include <pxr/imaging/hd/renderPass.h>

PXR_NAMESPACE_OPEN_SCOPE

class NPTracerHdRenderPass final : public HdRenderPass
{
public:
    NPTracerHdRenderPass(HdRenderIndex* index, const HdRprimCollection& collection,
                         NPTracerHdRenderDelegate* delegate);

    bool IsConverged() const override;

protected:
    void _Execute(const HdRenderPassStateSharedPtr& renderPassState,
                  const TfTokenVector& renderTags) override;

    // set the convergence state
    void SetConverged(bool converged);

private:
    NPTracerHdRenderDelegate* _pCreator;

    std::atomic<bool> _bConverged{ false };
    std::atomic<uint32_t> _numRenderPassesExecuted{ 0u };

    // camera should not be synced if we are overriding
    static constexpr bool _bSyncCameraPerPass = !ASSIMP_OVERRIDE;
    static void sSyncCameraToState(const HdRenderPassStateSharedPtr& renderPassState,
                                   np::Camera* outCam);

    // TEMP: only create rendering resources once
    std::atomic<bool> _bResourcesCreatedFlag{ false };
};

PXR_NAMESPACE_CLOSE_SCOPE
