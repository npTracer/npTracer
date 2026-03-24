#include "usd_plugin/NPTracerHdRenderPass.h"

#include "usd_plugin/debugCodes.h"

#include <pxr/imaging/hd/camera.h>
#include <pxr/base/gf/camera.h>
#include <pxr/imaging/hd/renderPassState.h>
#include <pxr/imaging/hd/renderBuffer.h>

#include <glm/gtc/type_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE

NPTracerHdRenderPass::NPTracerHdRenderPass(HdRenderIndex* index,
                                           HdRprimCollection const& collection,
                                           NPTracerHdRenderDelegate* delegate)
    : HdRenderPass(index, collection), _delegate(delegate)
{
}

void NPTracerHdRenderPass::_Execute(HdRenderPassStateSharedPtr const& renderPassState,
                                    TfTokenVector const& renderTags)
{
    this->SetConverged(false);

    HdRenderPassAovBindingVector aovBindings = renderPassState->GetAovBindings();

    for (HdRenderPassAovBinding const& aov : aovBindings)
    {
        NPTracerHdRenderBuffer* buffer = dynamic_cast<NPTracerHdRenderBuffer*>(aov.renderBuffer);
        if (!buffer)  // `dynamic_cast` failed
        {
            continue;
        }
        buffer->SetConverged(false);

        _ExtractAovs(renderPassState, buffer);

        if (aov.aovName == HdAovTokens->color)
        {
        }

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

NPRendererAovs NPTracerHdRenderPass::_ExtractAovs(HdRenderPassStateSharedPtr const& state,
                                                  NPTracerHdRenderBuffer* buffer)
{
    return {};
}

PXR_NAMESPACE_CLOSE_SCOPE
