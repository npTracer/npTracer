#pragma once

#include "usd_plugin/NPTracerHdRenderDelegate.h"

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

private:
    NPTracerHdRenderDelegate* _delegate;
};

PXR_NAMESPACE_CLOSE_SCOPE
