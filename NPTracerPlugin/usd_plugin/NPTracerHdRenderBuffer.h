#pragma once

#include "usd_plugin/npTokens.h"

#include <NPTracerRenderer/context.h>

#include <pxr/imaging/hd/renderBuffer.h>
#include <pxr/base/gf/vec3i.h>

#include <vulkan/vulkan.h>

PXR_NAMESPACE_OPEN_SCOPE

// a block of memory that we are rendering into
class NPTracerHdRenderBuffer final : public HdRenderBuffer
{
public:
    NPTracerHdRenderBuffer(const SdfPath& bprimId, Context* context);

    // allocate a new buffer with the given dimensions and format
    virtual bool Allocate(const GfVec3i& dimensions, HdFormat format, bool multiSampled) override;

    virtual unsigned int GetWidth() const override;
    virtual unsigned int GetHeight() const override;
    virtual unsigned int GetDepth() const override;
    virtual HdFormat GetFormat() const override;
    virtual bool IsMultiSampled() const override;

    // map the buffer for reading/writing
    virtual void* Map() override;

    virtual void Unmap() override;

    // return whether any clients have this buffer mapped currently
    virtual bool IsMapped() const override;

    virtual bool IsConverged() const override;
    void SetConverged(bool converged);

    // resolve the sample buffer into final values
    virtual void Resolve() override;

    virtual VtValue GetResource(bool multiSampled) const override;

    inline NPImage* GetImage()
    {
        return &_image;
    }

    inline void SetLayout(const VkImageLayout& layout)
    {
        _layout = layout;
    }

private:
    // release any allocated resources
    virtual void _Deallocate() override;

    // the actual underlying buffer
    NPImage _image;
    VkImageLayout _layout = VK_IMAGE_LAYOUT_UNDEFINED;

    // reused GPU buffer for image to GPU buffer transfer
    NPBuffer _stagingBuffer;

    // dedicated command buffer for Map() readback, avoids conflicting with frame command buffers
    VkCommandBuffer _mapCommandBuffer = VK_NULL_HANDLE;

    GfVec3i _dimensions = GfVec3i(-1, -1, -1);
    HdFormat _format = HdFormatInvalid;
    bool _multiSampled = false;
    Np::FormatTokens _fmtTokens;

    // the number of callers mapping this buffer
    std::atomic<int> _mappers{ 0 };

    std::atomic<bool> _converged{ false };

    Context* _pCtx;
};

PXR_NAMESPACE_CLOSE_SCOPE
