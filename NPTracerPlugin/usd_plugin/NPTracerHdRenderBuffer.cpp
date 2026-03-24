#include "usd_plugin/NPTracerHdRenderBuffer.h"

#include "usd_plugin/debugCodes.h"
#include "usd_plugin/NPTracerHdRenderParam.h"

PXR_NAMESPACE_OPEN_SCOPE

NPTracerHdRenderBuffer::NPTracerHdRenderBuffer(const SdfPath& bprimId, Context* context)
    : HdRenderBuffer(bprimId), _context(context)
{
}

bool NPTracerHdRenderBuffer::Allocate(const GfVec3i& dimensions, HdFormat format, bool multiSampled)
{
    TF_DEBUG(NPTRACER_RENDER)
        .Msg("[%s] Allocate render buffer: id=%s, dimensions=(%i, %i, %i), "
             "format=%i\n",
             TF_FUNC_NAME().c_str(), GetId().GetText(), dimensions[0], dimensions[1], dimensions[2],
             format);

    _dimensions = dimensions;
    _format = format;
    _multiSampled = multiSampled;
    _fmtTokens = Np::GetFormatTokens(format);

    uint32_t width = static_cast<uint32_t>(dimensions[0]);
    uint32_t height = static_cast<uint32_t>(dimensions[1]);

    VkDeviceSize size = _fmtTokens.bytesPerPixel * width * height;

    _Deallocate();

    if (format == HdFormatInvalid)
    {
        return false;
    }

    VkFormat vkFormat = _fmtTokens.vkFormat;
    if (vkFormat == VK_FORMAT_UNDEFINED)
    {
        return false;
    }

    _context->createBuffer(*_stagingBuffer, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                           VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                               | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    _context->createImage(_image, VK_IMAGE_TYPE_2D, vkFormat, width, height, _fmtTokens.usage,
                          0  // device local
    );

    TF_DEBUG(NPTRACER_RENDER)
        .Msg("[%s] Render buffer allocated: id=%s\n", TF_FUNC_NAME().c_str(), GetId().GetText());

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

// copy underlying image data to a staging buffer for i/o mapping
void* NPTracerHdRenderBuffer::Map()
{
    _mappers.fetch_add(1);

    VkCommandBuffer cmd = _context->transferCommandBuffer;

    VkImageLayout currLayout = _layout;

    vkResetCommandBuffer(cmd, 0);
    _context->beginCommandBuffer(cmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    _context->transitionImageLayout(cmd, _image.image, currLayout,
                                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _fmtTokens.writeAccess,
                                    VK_ACCESS_2_TRANSFER_READ_BIT, _fmtTokens.writeStage,
                                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, _fmtTokens.aspect);

    _context->copyImageToBuffer(cmd, _image, *_stagingBuffer, static_cast<uint32_t>(_dimensions[0]),
                                static_cast<uint32_t>(_dimensions[1]));

    // restore image layout for future passes
    _context->transitionImageLayout(cmd, _image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    currLayout, VK_ACCESS_2_TRANSFER_READ_BIT, _fmtTokens.writeAccess,
                                    VK_PIPELINE_STAGE_2_TRANSFER_BIT, _fmtTokens.writeStage,
                                    _fmtTokens.aspect);

    _context->endCommandBuffer(cmd, NPQueueType::TRANSFER);
    vkQueueWaitIdle(_context->queues[NPQueueType::TRANSFER].queue);

    return _stagingBuffer->allocInfo.pMappedData;  // zero-copy op
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

void NPTracerHdRenderBuffer::_Deallocate()
{
    // reset to default/empty values
    _image.destroy(_context->device, _context->allocator);
    _stagingBuffer->destroy(_context->allocator);

    _layout = VK_IMAGE_LAYOUT_UNDEFINED;
    _dimensions = GfVec3i(-1);
    _format = HdFormatInvalid;

    _mappers.store(0);
    _converged.store(false);
}

PXR_NAMESPACE_CLOSE_SCOPE
