#pragma once

#include "usd_plugin/NPTracerHdRenderDelegate.h"
#include "usd_plugin/NPTracerHdRenderBuffer.h"

#include <NPTracerRenderer/structs.h>

#include <pxr/imaging/hd/renderPass.h>

PXR_NAMESPACE_OPEN_SCOPE

class NPTracerHdRenderPass : public HdRenderPass
{
public:
    NPTracerHdRenderPass(HdRenderIndex* index, HdRprimCollection const& collection,
                         NPTracerHdRenderDelegate* delegate);

protected:
    void _Execute(HdRenderPassStateSharedPtr const& renderPassState,
                  TfTokenVector const& renderTags) override;

    bool IsConverged() const override;

    // set the convergence state
    void SetConverged(bool converged);

private:
    NPTracerHdRenderDelegate* _pCreator;

    std::atomic<bool> _bConverged{ false };

    // camera should not be synced if we are overriding
    static constexpr bool _bSyncCameraPerPass = !ASSIMP_OVERRIDE;
    static void sSyncCameraToState(HdRenderPassStateSharedPtr const& renderPassState,
                                   NPCameraRecord* outCam);

    // TEMP: only create rendering resources once
    std::atomic<bool> _resourcesCreatedFlag{ false };
};

PXR_NAMESPACE_CLOSE_SCOPE
