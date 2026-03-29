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
    NP_DBG("Requested allocation of render buffer: id=%s, dimensions=(%i, %i, %i), format=%i\n",
           GetId().GetText(), dimensions[0], dimensions[1], dimensions[2], format);
    
    TF_DEV_AXIOM(dimensions[2] == 1); // temp: only support 2D buffers

    _Deallocate();

    _dimensions = dimensions;
    _format = format;
    _multiSampled = multiSampled;
    _fmtTokens = Np::GetFormatTokens(format);
    
    const VkDeviceSize size = GetSize();

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
    _pCtx->createImage(*_pImage, VK_IMAGE_TYPE_2D, vkFormat, dimensions[0], dimensions[1], _fmtTokens.usage,
                       0  // device local
    );

    VkCommandBuffer commandBuffer;
    _pCtx->createCommandBuffer(commandBuffer, NPQueueType::GRAPHICS);
    _pCtx->beginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    // temp: transition into general with all access and stage.
    _pImage->transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, 0,
                              VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT
                                  | VK_ACCESS_2_SHADER_WRITE_BIT,
                              VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);

    _pCtx->endCommandBuffer(commandBuffer, NPQueueType::GRAPHICS);

    vkQueueWaitIdle(_pCtx->queues[NPQueueType::GRAPHICS].queue);
    _pCtx->freeCommandBuffer(commandBuffer, NPQueueType::GRAPHICS);

    PREPARE_UNIQUE_PTR(_pStagingBuffer, NPBuffer,
                       [this]() { _pStagingBuffer->destroy(_pCtx->allocator); });
    _pCtx->createBuffer(*_pStagingBuffer, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                        VMA_ALLOCATION_CREATE_MAPPED_BIT);

    uint8_t* data = static_cast<uint8_t*>(_pStagingBuffer->allocInfo.pMappedData);

    for (size_t i = 0; i < size; i += 1)
    {
        data[i] = 255;
    }

    NP_DBG("Allocated render buffer: id=%s, dimensions=(%i, %i, %i), format=%i\n",
           GetId().GetText(), dimensions[0], dimensions[1], dimensions[2], format);

    _cpuDebugBuffer.resize(size);

    // Fill with solid red depending on format
    if (_format == HdFormatUNorm8Vec4)
    {
        for (size_t i = 0; i < size; i += 4)
        {
            _cpuDebugBuffer[i + 0] = 255;  // R
            _cpuDebugBuffer[i + 1] = 0;  // G
            _cpuDebugBuffer[i + 2] = 0;  // B
            _cpuDebugBuffer[i + 3] = 255;  // A
        }
    }
    else if (_format == HdFormatFloat32)
    {
        float* data = reinterpret_cast<float*>(_cpuDebugBuffer.data());
        size_t count = size / sizeof(float);
        for (size_t i = 0; i < count; i++)
        {
            data[i] = 1.0f;  // depth = 1 (far plane)
        }
    }

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
    return _fmtTokens.bytesPerPixel * _dimensions[0] * _dimensions[1];
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
        TF_FATAL_CODING_ERROR("Map called before allocation for buffer '%s'.\n", GetId().GetText());
    }

    NP_DBG("Render buffer '%s' requested for map.\n", GetId().GetText());

    _readers.fetch_add(1);  // add first to signal there is an intended reader

    while (HasWriter())
    {
        std::this_thread::yield();  // for now ensure reading can only occur during writing
    }

    uint8_t* dbgData = _cpuDebugBuffer.data();

    // print a few pixels
    // if (_format == HdFormatUNorm8Vec4)
    // {
    //     for (int i = 0; i < 5; i++)
    //     {
    //         int idx = i * 4;
    //         NP_DBG("[CPU] Pixel %d: (%u, %u, %u, %u)\n", i, dbgData[idx + 0], dbgData[idx + 1],
    //                dbgData[idx + 2], dbgData[idx + 3]);
    //     }
    // }
    // return dbgData;

    if (_transferCmdBuffer != VK_NULL_HANDLE) vkResetCommandBuffer(_transferCmdBuffer, 0);

    _pCtx->createCommandBuffer(_transferCmdBuffer, NPQueueType::TRANSFER);
    _pCtx->beginCommandBuffer(_transferCmdBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    /*_pCtx->transitionImageLayout(_transferCmdBuffer, _pImage->image, VK_IMAGE_LAYOUT_GENERAL,
                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _fmtTokens.writeAccess,
                                 VK_ACCESS_2_TRANSFER_READ_BIT, _fmtTokens.writeStage,
                                 VK_PIPELINE_STAGE_2_TRANSFER_BIT, _fmtTokens.aspect);*/

    _pCtx->copyImageToBuffer(_transferCmdBuffer, *_pImage, *_pStagingBuffer,
                             static_cast<uint32_t>(_dimensions[0]),
                             static_cast<uint32_t>(_dimensions[1]));

    // restore image layout for future passes
    /*_pCtx->transitionImageLayout(_transferCmdBuffer, _pImage->image,
                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                                 VK_ACCESS_2_TRANSFER_READ_BIT, _fmtTokens.writeAccess,
                                 VK_PIPELINE_STAGE_2_TRANSFER_BIT, _fmtTokens.writeStage,
                                 _fmtTokens.aspect);*/

    _pCtx->endCommandBuffer(_transferCmdBuffer, NPQueueType::TRANSFER);
    vkQueueWaitIdle(_pCtx->queues[NPQueueType::TRANSFER].queue);
    
    uint8_t* data = static_cast<uint8_t*>(_pStagingBuffer->allocInfo.pMappedData);

    const size_t size = GetSize();
    for (size_t i = 0; i < size; i += 4)
    {
        if (size > 23 && i > 20) continue; // only debug log a few
        NP_DBG("[Pixel %d] (%u, %u, %u, %u)\n", i / 4, data[i + 0], data[i + 1], data[i + 2], data[i + 3]);
    }

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
    NP_DBG("Requested deallocate of render buffer: id=%s\n", GetId().GetText());

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

    if (_transferCmdBuffer != VK_NULL_HANDLE)
    {
        _pCtx->freeCommandBuffer(_transferCmdBuffer, NPQueueType::TRANSFER);
        _transferCmdBuffer = VK_NULL_HANDLE;
    }

    _dimensions = GfVec3i(-1);
    _format = HdFormatInvalid;

    _readers.store(0);
    _converged.store(false);

    NP_DBG("Deallocate complete of render buffer: id=%s\n", GetId().GetText());
}

PXR_NAMESPACE_CLOSE_SCOPE