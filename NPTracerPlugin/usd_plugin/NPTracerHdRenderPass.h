#pragma once

#include "usd_plugin/NPTracerHdRenderDelegate.h"

#include <NPTracerRenderer/structs.h>

#include <pxr/imaging/hd/renderPass.h>

PXR_NAMESPACE_OPEN_SCOPE

class NPTracerHdRenderPass : public HdRenderPass
{
public:
    NPTracerHdRenderPass(HdRenderIndex* index, HdRprimCollection const& collection,
                         NPTracerHdRenderDelegate* delegate);

protected:
    virtual void _Execute(HdRenderPassStateSharedPtr const& renderPassState,
                  TfTokenVector const& renderTags) override;
    
    virtual bool IsConverged() const override;
    
    // set the convergence state
    void SetConverged(bool converged);

private:
    NPTracerHdRenderDelegate* _delegate;
    
    std::atomic<bool> _converged{false};

    RendererPayload _BuildRendererPayload(HdRenderPassStateSharedPtr const& state);
    VkRendererAovs _ExtractAovs(HdRenderPassStateSharedPtr const& state);
};

PXR_NAMESPACE_CLOSE_SCOPE
