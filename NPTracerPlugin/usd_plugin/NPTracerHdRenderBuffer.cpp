#include "usd_plugin/NPTracerHdRenderBuffer.h"

#include "usd_plugin/NPTracerDebugCodes.h"
#include "usd_plugin/NPTracerHdRenderParam.h"

PXR_NAMESPACE_OPEN_SCOPE

extern VkDevice gDevice;
extern VkPhysicalDevice gPhysicalDevice;
extern VkQueue gQueue;
extern VkCommandPool gCommandPool;

static VkFormat ConvertHdFormat(HdFormat fmt)
{
    switch (fmt)
    {
        case HdFormatUNorm8Vec4: return VK_FORMAT_R8G8B8A8_UNORM;
        case HdFormatFloat32: return VK_FORMAT_D32_SFLOAT;
        default: return VK_FORMAT_UNDEFINED;
    }
}

NPTracerHdRenderBuffer::NPTracerHdRenderBuffer(const SdfPath& bprimId) : HdRenderBuffer(bprimId)
{
}

bool NPTracerHdRenderBuffer::Allocate(const GfVec3i& dimensions, HdFormat format, bool multiSampled)
{
    TF_DEBUG(NPTRACER_RENDER)
        .Msg("[%s] Allocate render buffer: id=%s, dimensions=(%i, %i, %i), "
             "format=%i\n",
             TF_FUNC_NAME().c_str(), GetId().GetText(), dimensions[0], dimensions[1], dimensions[2],
             format);

    _Deallocate();

    if (format == HdFormatInvalid)
    {
        return false;
    }

    _dimensions = dimensions;
    _format = format;
    _multiSampled = multiSampled;

    _vkFormat = ConvertHdFormat(format);
    if (_vkFormat == VK_FORMAT_UNDEFINED)
    {
        return false;
    }

    // create VkImage
    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.extent = { static_cast<uint32_t>(dimensions[0]), static_cast<uint32_t>(dimensions[1]), 1 };
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.format = _vkFormat;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                    | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;  // for readback
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    vkCreateImage(gDevice, &imgInfo, nullptr, &_image);

    // allocate memory
    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(gDevice, _image, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;

    // find memory type
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(gPhysicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
    {
        if ((memReq.memoryTypeBits & (1 << i))
            && (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            allocInfo.memoryTypeIndex = i;
            break;
        }
    }

    vkAllocateMemory(gDevice, &allocInfo, nullptr, &_memory);
    vkBindImageMemory(gDevice, _image, _memory, 0);

    TF_DEBUG(NPTRACER_RENDER)
        .Msg("[%s] Render buffer: id=%s, size=%llu\n", TF_FUNC_NAME().c_str(), GetId().GetText(),
             memReq.size);

    return true;
}

unsigned int NPTracerHdRenderBuffer::GetWidth() const
{
    return _dimensions[0];
}

unsigned int NPTracerHdRenderBuffer::GetHeight() const
{
    return _dimensions[1];
}

unsigned int NPTracerHdRenderBuffer::GetDepth() const
{
    return _dimensions[2];
}

HdFormat NPTracerHdRenderBuffer::GetFormat() const
{
    return _format;
}

bool NPTracerHdRenderBuffer::IsMultiSampled() const
{
    return _multiSampled;
}

void* NPTracerHdRenderBuffer::Map()
{
    _mappers.fetch_add(1);

    // create a staging buffer so that CPU-based map is still supported
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = _dimensions[0] * _dimensions[1] * 4;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    vkCreateBuffer(gDevice, &bufInfo, nullptr, &stagingBuffer);

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(gDevice, stagingBuffer, &memReq);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = memReq.size;

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(gPhysicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
    {
        if ((memReq.memoryTypeBits & (1 << i))
            && (memProps.memoryTypes[i].propertyFlags
                & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)))
        {
            alloc.memoryTypeIndex = i;
            break;
        }
    }

    vkAllocateMemory(gDevice, &alloc, nullptr, &stagingMemory);
    vkBindBufferMemory(gDevice, stagingBuffer, stagingMemory, 0);

    // copy VkImage to buffer
    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = gCommandPool;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(gDevice, &cmdAlloc, &cmd);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &begin);

    VkBufferImageCopy region{};
    region.imageExtent = { static_cast<uint32_t>(_dimensions[0]), static_cast<uint32_t>(_dimensions[1]), 1 };
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    
    vkCmdCopyImageToBuffer(
        cmd,
        _image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        stagingBuffer,
        1,
        &region
    );

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    vkQueueSubmit(gQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(gQueue);
    
    // map memory for i/o
    void* data;
    vkMapMemory(gDevice, stagingMemory, 0, VK_WHOLE_SIZE, 0, &data);

    size_t size = _dimensions[0] * _dimensions[1] * 4;
    _cpuBuffer.resize(size);
    memcpy(_cpuBuffer.data(), data, size);

    vkUnmapMemory(gDevice, stagingMemory);

    return _cpuBuffer.data();
}

void NPTracerHdRenderBuffer::Unmap()
{
    _mappers.fetch_sub(1);
}

bool NPTracerHdRenderBuffer::IsMapped() const
{
    return _mappers.load() > 0;
}

bool NPTracerHdRenderBuffer::IsConverged() const
{
    return _converged.load();
}

void NPTracerHdRenderBuffer::SetConverged(bool converged)
{
    _converged.store(converged);
}

void NPTracerHdRenderBuffer::Resolve()
{
    // TODO
}

VtValue NPTracerHdRenderBuffer::GetResource(bool multiSampled) const
{
    return VtValue();
}

VkImage NPTracerHdRenderBuffer::GetVkImage() const
{
    return _image;
}

VkDeviceMemory NPTracerHdRenderBuffer::GetVkDeviceMemory() const
{
    return _memory;
}

void NPTracerHdRenderBuffer::_Deallocate()
{
    // reset to default/empty values.
    if (_image) {
        vkDestroyImage(gDevice, _image, nullptr);
        _image = VK_NULL_HANDLE;
    }

    if (_memory) {
        vkFreeMemory(gDevice, _memory, nullptr);
        _memory = VK_NULL_HANDLE;
    }

    _dimensions = GfVec3i(-1);
    _format = HdFormatInvalid;

    _mappers.store(0);
    _converged.store(false);
}

PXR_NAMESPACE_CLOSE_SCOPE
