#include "usd_plugin/NPTracerHdRenderPass.h"

#include "usd_plugin/NPTracerIntegrator.h"

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
    HdRenderPassAovBindingVector aovBindings = renderPassState->GetAovBindings();

    for (HdRenderPassAovBinding const& aov : aovBindings)
    {
        HdRenderBuffer* buffer = static_cast<HdRenderBuffer*>(aov.renderBuffer);

        if (!buffer)
        {
            continue;
        }

        float* data = static_cast<float*>(buffer->Map());

        uint32_t width = buffer->GetWidth();
        uint32_t height = buffer->GetHeight();

        NPTracerIntegrator* integrator = _delegate->GetIntegrator();

        integrator->RenderToBuffer(data, width, height);

        buffer->Unmap();
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
