#pragma once

#include <NPTracerRenderer/structs.h>

#include <pxr/imaging/hd/renderBuffer.h>
#include <pxr/base/gf/vec3i.h>

#include <vulkan/vulkan.h>

PXR_NAMESPACE_OPEN_SCOPE

// a block of memory that we are rendering into
class NPTracerHdRenderBuffer final : public HdRenderBuffer
{
public:
    NPTracerHdRenderBuffer(const SdfPath& bprimId);

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
    
    VkImage GetVkImage() const;
    VkDeviceMemory GetVkDeviceMemory() const;

private:
    // release any allocated resources
    virtual void _Deallocate() override;
    
    // the actual underlying buffer
    VkImage _image = VK_NULL_HANDLE;
    VkDeviceMemory _memory = VK_NULL_HANDLE;
    VkFormat _vkFormat = VK_FORMAT_UNDEFINED;

    // staging buffer for CPU readback
    std::vector<uint8_t> _cpuBuffer;
    
    GfVec3i _dimensions = GfVec3i(-1, -1, -1);
    HdFormat _format = HdFormatInvalid;
    bool _multiSampled = false;

    // the number of callers mapping this buffer
    std::atomic<int> _mappers{ 0 };

    std::atomic<bool> _converged{ false };
};

PXR_NAMESPACE_CLOSE_SCOPE
