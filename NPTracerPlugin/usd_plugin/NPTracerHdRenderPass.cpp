#include "usd_plugin/NPTracerHdRenderPass.h"

#include "usd_plugin/NPTracerIntegrator.h"
#include "usd_plugin/NPTracerHdRenderBuffer.h"

#include <pxr/imaging/hd/renderPassState.h>
#include <pxr/imaging/hd/renderBuffer.h>

#include <cstdint>

PXR_NAMESPACE_OPEN_SCOPE

NPTracerHdRenderPass::NPTracerHdRenderPass(HdRenderIndex* index,
                                           HdRprimCollection const& collection,
                                           NPTracerHdRenderDelegate* delegate)
    : HdRenderPass(index, collection), _delegate(delegate)
{
}

void NPTracerHdRenderPass::_Execute(HdRenderPassStateSharedPtr const& renderPassState,
                                    TfTokenVector const&)
{
    this->SetConverged(false);
    
    HdRenderPassAovBindingVector aovBindings = renderPassState->GetAovBindings();

    for (HdRenderPassAovBinding const& aov : aovBindings)
    {
        NPTracerHdRenderBuffer* buffer = dynamic_cast<NPTracerHdRenderBuffer*>(aov.renderBuffer);
        if (!buffer) // `dynamic_cast` failed
        {
            continue;
        }
        buffer->SetConverged(false);
        
        float* data = static_cast<float*>(buffer->Map());

        uint32_t width = buffer->GetWidth();
        uint32_t height = buffer->GetHeight();
        
        if (aov.aovName == HdAovTokens->color) {
            
        }

        buffer->Unmap();
        
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


PXR_NAMESPACE_CLOSE_SCOPE
