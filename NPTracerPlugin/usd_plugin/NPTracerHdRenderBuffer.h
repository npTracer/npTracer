#pragma once

#include "usd_plugin/npTokens.h"

#include <NPTracerRenderer/context.h>

#include <pxr/imaging/hd/renderBuffer.h>
#include <pxr/base/gf/vec3i.h>

PXR_NAMESPACE_OPEN_SCOPE

// a block of memory that we are rendering into
class NPTracerHdRenderBuffer final : public HdRenderBuffer
{
public:
    NPTracerHdRenderBuffer(const SdfPath& bprimId, Context* context);

    // allocate a new buffer with the given dimensions and format
    bool Allocate(const GfVec3i& dimensions, HdFormat format, bool multiSampled) override;

    unsigned int GetWidth() const override;
    unsigned int GetHeight() const override;
    unsigned int GetDepth() const override;
    HdFormat GetFormat() const override;
    bool IsMultiSampled() const override;

    // map the buffer for reading/writing
    void* Map() override;

    void Unmap() override;

    // return whether any clients have this buffer mapped currently
    bool IsMapped() const override;

    bool IsConverged() const override;
    void SetConverged(bool converged);

    // resolve the sample buffer into final values
    void Resolve() override;

    // will wait until write is available
    bool HasWriter() const;
    void EndWrite();
    // returns nullptr if `waitForSuccess` is false and another entity is already writing
    NPImage* RequestImageForWrite(bool waitUntilSuccess = true);

private:
    // release any allocated resources
    void _Deallocate() override;

    // the actual underlying buffer
    std::unique_ptr<NPImage> _pImage;
    Np::FormatTokens _fmtTokens;

    // reused GPU buffer for image to GPU buffer transfer
    std::unique_ptr<NPBuffer> _pStagingBuffer;

    GfVec3i _dimensions = GfVec3i(-1);
    HdFormat _format = HdFormatInvalid;
    bool _multiSampled = false;

    // the number of entities that are reading this buffer
    std::atomic<int> _readers{ 0 };
    std::atomic<bool> _hasWriter{ false };

    std::atomic<bool> _converged{ true };

    Context* _pCtx;
    VkCommandBuffer _transferCmdBuffer = VK_NULL_HANDLE;
};

PXR_NAMESPACE_CLOSE_SCOPE
