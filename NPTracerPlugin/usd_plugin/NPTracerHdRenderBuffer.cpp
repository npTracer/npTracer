#include "usd_plugin/NPTracerHdRenderBuffer.h"

#include "usd_plugin/debugCodes.h"
#include "usd_plugin/NPTracerHdRenderParam.h"

#define PREPARE_UNIQUE_PTR(_ptr, _ptrType, _destroyer)                                             \
    if (_ptr == nullptr)                                                                           \
    {                                                                                              \
        _ptr = std::make_unique<_ptrType>();                                                       \
    }                                                                                              \
    else                                                                                           \
    {                                                                                              \
        _destroyer();                                                                              \
        _ptr.reset();                                                                              \
        _ptr = std::make_unique<_ptrType>();                                                       \
    }

PXR_NAMESPACE_OPEN_SCOPE

NPTracerHdRenderBuffer::NPTracerHdRenderBuffer(const SdfPath& bprimId, Context* context)
    : HdRenderBuffer(bprimId)
    , _pCtx(context)
    , _pImage(nullptr)
    , _pStagingBuffer(nullptr)
    , _fmtTokens(Np::GetFormatTokens(HdFormatInvalid))
{
}

bool NPTracerHdRenderBuffer::Allocate(const GfVec3i& dimensions, HdFormat format, bool multiSampled)
{
    _Deallocate();

    _dimensions = dimensions;
    _format = format;
    _multiSampled = multiSampled;
    _fmtTokens = Np::GetFormatTokens(format);

    uint32_t width = static_cast<uint32_t>(dimensions[0]);
    uint32_t height = static_cast<uint32_t>(dimensions[1]);
    VkDeviceSize size = _fmtTokens.bytesPerPixel * width * height;

    if (format == HdFormatInvalid)
    {
        return false;
    }

    VkFormat vkFormat = _fmtTokens.vkFormat;
    if (vkFormat == VK_FORMAT_UNDEFINED)
    {
        return false;
    }

    PREPARE_UNIQUE_PTR(_pImage, NPImage,
                       [this]() { _pImage->destroy(_pCtx->device, _pCtx->allocator); });
    _pCtx->createImage(*_pImage, VK_IMAGE_TYPE_2D, vkFormat, width, height, _fmtTokens.usage,
                       0  // device local
    );

    PREPARE_UNIQUE_PTR(_pStagingBuffer, NPBuffer,
                       [this]() { _pStagingBuffer->destroy(_pCtx->allocator); });
    _pCtx->createBuffer(*_pStagingBuffer, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                            | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    NP_DBG("[%s] Allocated render buffer: id=%s, dimensions=(%i, %i, %i), "
           "format=%i\n",
           TF_FUNC_NAME().c_str(), GetId().GetText(), dimensions[0], dimensions[1], dimensions[2],
           format);

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
    if (!_pImage)
    {
        TF_FATAL_CODING_ERROR("Map called before allocation for buffer '%s'.", GetId().GetText());
    }

    NP_DBG("Render buffer '%s' requested for map.", GetId().GetText());

    _readers.fetch_add(1);  // add first to signal there is an intended reader

    while (HasWriter())
    {
        std::this_thread::yield();  // for now ensure reading can only occur during writing
    }

    vkResetCommandBuffer(_transferCmdBuffer, 0);
    _pCtx->createCommandBuffer(_transferCmdBuffer, NPQueueType::TRANSFER);
    _pCtx->beginCommandBuffer(_transferCmdBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    _pCtx->transitionImageLayout(_transferCmdBuffer, _pImage->image, VK_IMAGE_LAYOUT_GENERAL,
                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _fmtTokens.writeAccess,
                                 VK_ACCESS_2_TRANSFER_READ_BIT, _fmtTokens.writeStage,
                                 VK_PIPELINE_STAGE_2_TRANSFER_BIT, _fmtTokens.aspect);

    _pCtx->copyImageToBuffer(_transferCmdBuffer, *_pImage, *_pStagingBuffer,
                             static_cast<uint32_t>(_dimensions[0]),
                             static_cast<uint32_t>(_dimensions[1]));

    // restore image layout for future passes
    _pCtx->transitionImageLayout(_transferCmdBuffer, _pImage->image,
                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                                 VK_ACCESS_2_TRANSFER_READ_BIT, _fmtTokens.writeAccess,
                                 VK_PIPELINE_STAGE_2_TRANSFER_BIT, _fmtTokens.writeStage,
                                 _fmtTokens.aspect);

    _pCtx->endCommandBuffer(_transferCmdBuffer, NPQueueType::TRANSFER);
    vkQueueWaitIdle(_pCtx->queues[NPQueueType::TRANSFER].queue);

    return _pStagingBuffer->allocInfo.pMappedData;  // zero-copy op
}

void NPTracerHdRenderBuffer::Unmap()
{
    _readers.fetch_sub(1);
}

bool NPTracerHdRenderBuffer::IsMapped() const
{
    return _readers.load() > 0;
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

bool NPTracerHdRenderBuffer::HasWriter() const
{
    return _hasWriter.load();
}

NPImage* NPTracerHdRenderBuffer::RequestImageForWrite(bool waitUntilSuccess)
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
        _hasWriter.store(true);
        return _pImage.get();
    }
    return nullptr;
}

void NPTracerHdRenderBuffer::EndWrite()
{
    SetConverged(true);
    _hasWriter.store(false);
}

void NPTracerHdRenderBuffer::_Deallocate()
{
    TF_DEV_AXIOM(!HasWriter() && !IsMapped() && IsConverged());

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

    _dimensions = GfVec3i(-1);
    _format = HdFormatInvalid;

    _readers.store(0);
    _converged.store(false);
}

PXR_NAMESPACE_CLOSE_SCOPE
