#pragma once

#include <pxr/imaging/hd/renderBuffer.h>
#include <pxr/base/gf/vec3i.h>

PXR_NAMESPACE_OPEN_SCOPE

// a block of memory that we are rendering into
class NPTracerHdRenderBuffer : public HdRenderBuffer
{
public:
    NPTracerHdRenderBuffer(const SdfPath& bprimId);

    // allocate a new buffer with the given dimensions and format
    virtual bool Allocate(const GfVec3i& dimensions, HdFormat format, bool multiSampled) override;

    // accessor for buffer width
    virtual unsigned int GetWidth() const override;

    // accessor for buffer height
    virtual unsigned int GetHeight() const override;

    // accessor for buffer depth
    virtual unsigned int GetDepth() const override;

    // accessor for buffer format
    virtual HdFormat GetFormat() const override;

    // accessor for the buffer multisample state
    virtual bool IsMultiSampled() const override;

    // map the buffer for reading/writing. The control flow should be Map(), before any I/O, followed by memory access, followed by Unmap() when done
    virtual void* Map() override;

    // unmap the buffer
    virtual void Unmap() override;

    // return whether any clients have this buffer mapped currently
    virtual bool IsMapped() const override;

    // checks if the buffer is converged
    virtual bool IsConverged() const override;

    // set the convergence state
    void SetConverged(bool converged);

    // resolve the sample buffer into final values
    virtual void Resolve() override;

private:
    // release any allocated resources
    virtual void _Deallocate() override;

    // buffer dimensions
    GfVec3i _dimensions = GfVec3i(-1, -1, -1);

    // buffer format
    HdFormat _format = HdFormatInvalid;

    // the actual buffer of bytes
    std::vector<uint8_t> _buffer;

    // the number of callers mapping this buffer
    std::atomic<int> _mappers{ 0 };

    // whether the buffer has been marked as converged
    std::atomic<bool> _converged{ false };
};

PXR_NAMESPACE_CLOSE_SCOPE
