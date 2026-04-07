#include "usdPlugin/NPTracerHdRenderBuffer.h"

#include "usdPlugin/debugCodes.h"
#include "usdPlugin/NPTracerHdRenderParam.h"

#define PREPARE_UNIQUE_PTR(_ptr, _ptrType, _destroyer)                                             \
    do                                                                                             \
    {                                                                                              \
        if (_ptr == nullptr)                                                                       \
        {                                                                                          \
            _ptr = std::make_unique<_ptrType>();                                                   \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            _destroyer();                                                                          \
            _ptr.reset();                                                                          \
            _ptr = std::make_unique<_ptrType>();                                                   \
        }                                                                                          \
    } while (0)

PXR_NAMESPACE_OPEN_SCOPE

NPTracerHdRenderBuffer::NPTracerHdRenderBuffer(const SdfPath& bprimId, np::Context* context)
    : HdRenderBuffer(bprimId)
    , _pCtx(context)
    , _pImage(nullptr)
    , _pStagingBuffer(nullptr)
    , _aovTokens(np::getAovTokens(np::eAovType::INVALID))
{
}

bool NPTracerHdRenderBuffer::Allocate(const GfVec3i& dimensions, HdFormat format, bool multiSampled)
{
    NP_DBG("Requested allocation of render buffer: id=%s, dimensions=(%i, %i, %i), format=%i\n",
           GetId().GetText(), dimensions[0], dimensions[1], dimensions[2], format);

    TF_DEV_AXIOM(dimensions[2] == 1);  // TEMP: only support 2D buffers

    _Deallocate();

    _dimensions = dimensions;
    _format = format;
    _bMultiSampled = multiSampled;
    _aovTokens = np::getAovTokens(sHdFormatToNPAovType(format));

    const VkDeviceSize size = GetSize();

    if (format == HdFormatInvalid)
    {
        return false;
    }

    VkFormat vkFormat = _aovTokens.format;
    if (vkFormat == VK_FORMAT_UNDEFINED)
    {
        return false;
    }

    PREPARE_UNIQUE_PTR(_pImage, np::Image,
                       [this]() { _pImage->destroy(_pCtx->device, _pCtx->allocator); });
    _pCtx->createImage(*_pImage, VK_IMAGE_TYPE_2D, vkFormat, dimensions[0], dimensions[1],
                       _aovTokens.imageUsage, 0, _aovTokens.imageAspect, true);

    VkCommandBuffer commandBuffer;
    _pCtx->createCommandBuffer(commandBuffer, np::QueueType::GRAPHICS);
    _pCtx->beginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    // transition into transfer src optimal for renderer
    // TEMP: set all access and stage for ease-of-use
    _pImage->transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0,
                              VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT
                                  | VK_ACCESS_2_SHADER_WRITE_BIT,
                              VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);

    _pCtx->endCommandBuffer(commandBuffer, np::QueueType::GRAPHICS);

    // TEMP: cannot submit depth work to `TRANSFER` family
    vkQueueWaitIdle(_pCtx->queues[np::QueueType::GRAPHICS].queue);
    _pCtx->freeCommandBuffer(commandBuffer, np::QueueType::GRAPHICS);

    PREPARE_UNIQUE_PTR(_pStagingBuffer, np::Buffer,
                       [this]() { _pStagingBuffer->destroy(_pCtx->allocator); });
    _pCtx->createBuffer(*_pStagingBuffer, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        VMA_ALLOCATION_CREATE_MAPPED_BIT);

    NP_DBG("Allocated render buffer: id=%s, dimensions=(%i, %i, %i), format=%i\n",
           GetId().GetText(), dimensions[0], dimensions[1], dimensions[2], format);

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

size_t NPTracerHdRenderBuffer::GetSize() const
{
    TF_DEV_AXIOM(_dimensions[0] > 0 && _dimensions[1] > 0 && _dimensions[2]);
    return _aovTokens.bytesPerPixel * _dimensions[0] * _dimensions[1];
}

bool NPTracerHdRenderBuffer::IsMultiSampled() const
{
    return _bMultiSampled;
}

// copy underlying image data to a staging buffer for i/o mapping
void* NPTracerHdRenderBuffer::Map()
{
    if (!_pImage)
    {
        TF_FATAL_CODING_ERROR("Map called before allocation for buffer '%s'.\n", GetId().GetText());
    }

    NP_DBG("Render buffer '%s' requested for map.\n", GetId().GetText());

    _readers.fetch_add(1);  // add first to signal there is an intended reader

    while (HasWriter())
    {
        std::this_thread::yield();  // for now ensure reading can only occur during writing
    }

    if (_transferCmdBuffer != VK_NULL_HANDLE) vkResetCommandBuffer(_transferCmdBuffer, 0);

    _pCtx->createCommandBuffer(_transferCmdBuffer, np::QueueType::GRAPHICS);
    _pCtx->beginCommandBuffer(_transferCmdBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    _pCtx->copyImageToBuffer(_transferCmdBuffer, *_pImage, *_pStagingBuffer,
                             static_cast<uint32_t>(_dimensions[0]),
                             static_cast<uint32_t>(_dimensions[1]), _aovTokens.imageAspect);

    _pCtx->endCommandBuffer(_transferCmdBuffer, np::QueueType::GRAPHICS);
    vkQueueWaitIdle(_pCtx->queues[np::QueueType::GRAPHICS].queue);

    return _pStagingBuffer->allocInfo.pMappedData;  // zero-copy op
}

void NPTracerHdRenderBuffer::Unmap()
{
    _readers.fetch_sub(1);

    NP_DBG("Unmapped render buffer: id=%s\n", GetId().GetText());
}

bool NPTracerHdRenderBuffer::IsMapped() const
{
    return _readers.load() > 0;
}

bool NPTracerHdRenderBuffer::IsConverged() const
{
    return _bConverged.load();
}

void NPTracerHdRenderBuffer::SetConverged(bool converged)
{
    _bConverged.store(converged);
}

void NPTracerHdRenderBuffer::Resolve()
{
    // TODO
}

bool NPTracerHdRenderBuffer::HasWriter() const
{
    return _bHasWriter.load();
}

np::Image* NPTracerHdRenderBuffer::RequestImageForWrite(bool waitUntilSuccess)
{
    if (waitUntilSuccess)
    {
        while (HasWriter() || IsMapped())
        {
            std::this_thread::yield();  // for now ensure reading can only occur during writing
        }
    }

    if (!HasWriter() && !IsMapped())
    {
        SetConverged(false);  // first mark not converged
        _bHasWriter.store(true);
        return _pImage.get();
    }
    return nullptr;
}

void NPTracerHdRenderBuffer::EndWrite()
{
    SetConverged(true);
    _bHasWriter.store(false);
}

void NPTracerHdRenderBuffer::_Deallocate()
{
    NP_DBG("Requested deallocate of render buffer: id=%s\n", GetId().GetText());

    TF_DEV_AXIOM(!HasWriter() && !IsMapped() && IsConverged());

    _pCtx->waitIdle();  // TEMP: we should use fences instead

    // reset to default/empty values
    if (_pImage)
    {
        _pImage->destroy(_pCtx->device, _pCtx->allocator);
        _pImage.reset(nullptr);  // reset to nullptr to signal non-allocated state
    }
    if (_pStagingBuffer)
    {
        _pStagingBuffer->destroy(_pCtx->allocator);
        _pStagingBuffer.reset(nullptr);
    }

    if (_transferCmdBuffer != VK_NULL_HANDLE)
    {
        _pCtx->freeCommandBuffer(_transferCmdBuffer, np::QueueType::GRAPHICS);
        _transferCmdBuffer = VK_NULL_HANDLE;
    }

    _dimensions = GfVec3i(-1);
    _format = HdFormatInvalid;

    _readers.store(0);
    _bConverged.store(false);

    NP_DBG("Deallocate complete of render buffer: id=%s\n", GetId().GetText());
}

np::eAovType NPTracerHdRenderBuffer::sHdFormatToNPAovType(const HdFormat format)
{
    switch (format)
    {
        case HdFormatUNorm8Vec4: return np::eAovType::COLOR;
        case HdFormatFloat32: return np::eAovType::DEPTH;
        default: return np::eAovType::INVALID;
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
