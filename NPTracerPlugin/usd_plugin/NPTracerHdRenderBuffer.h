#pragma once

#include "NPTracerRenderer/tokens.h"
#include <NPTracerRenderer/context.h>

#include <pxr/imaging/hd/renderBuffer.h>
#include <pxr/base/gf/vec3i.h>

PXR_NAMESPACE_OPEN_SCOPE

// a block of memory that we are rendering into
class NPTracerHdRenderBuffer final : public HdRenderBuffer
{
public:
    NPTracerHdRenderBuffer(const SdfPath& bprimId, np::Context* context);

    // allocate a new buffer with the given dimensions and format
    bool Allocate(const GfVec3i& dimensions, HdFormat format, bool multiSampled) override;

    unsigned int GetWidth() const override;
    unsigned int GetHeight() const override;
    unsigned int GetDepth() const override;
    HdFormat GetFormat() const override;
    size_t GetSize() const;
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
    np::NPImage* RequestImageForWrite(bool waitUntilSuccess = true);

    // TODO: technically, we cannot assume aov type based on format along. but for now we will make this assumption
    static np::NPAovType sHdFormatToNPAovType(const HdFormat format);

private:
    // release any allocated resources
    void _Deallocate() override;

    // the actual underlying buffer
    std::unique_ptr<np::NPImage> _pImage;
    np::AovTokens _aovTokens;

    // reused GPU buffer for image to GPU buffer transfer
    std::unique_ptr<np::NPBuffer> _pStagingBuffer;

    GfVec3i _dimensions = GfVec3i(-1);
    HdFormat _format = HdFormatInvalid;
    bool _bMultiSampled = false;

    // the number of entities that are reading this buffer
    std::atomic<int> _readers{ 0 };
    std::atomic<bool> _bHasWriter{ false };

    std::atomic<bool> _bConverged{ true };

    np::Context* _pCtx;
    VkCommandBuffer _transferCmdBuffer = VK_NULL_HANDLE;

    std::vector<uint8_t> _cpuDebugBuffer;
};

PXR_NAMESPACE_CLOSE_SCOPE
