#include "usd_plugin/NPTracerHdRenderBuffer.h"

#include "usd_plugin/NPTracerDebugCodes.h"
#include "usd_plugin/NPTracerHdRenderParam.h"

PXR_NAMESPACE_OPEN_SCOPE

NPTracerHdRenderBuffer::NPTracerHdRenderBuffer(const SdfPath& bprimId, Hgi* hgi)
    : HdRenderBuffer(bprimId), _hgi(hgi)
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

    if (!_hgi) {
        TF_WARN("Hgi is null in Allocate");
        return false;
    }

    // build texture descriptor
    HgiTextureDesc desc;
    desc.debugName = GetId().GetString();
    desc.dimensions = _dimensions;
    desc.layerCount = 1;
    desc.mipLevels = 1;

    desc.type = (_dimensions[2] > 1)
        ? HgiTextureType3D
        : HgiTextureType2D;

    // convert HdFormat to HgiFormat
    desc.format = HgiFormatInvalid;

    switch (_format)
    {
        case HdFormatUNorm8Vec4:
            desc.format = HgiFormatUNorm8Vec4;
            break;
        case HdFormatFloat32:
            desc.format = HgiFormatFloat32;
            break;
        default:
            TF_WARN("Unsupported HdFormat for NPTracer: %d", _format);
            return false;
    }

    // extract usage flags
    if (HdAovHasDepthSemantic(GetId().GetNameToken())) {
        desc.usage =
            HgiTextureUsageBitsDepthTarget |
            HgiTextureUsageBitsShaderRead;
    } else {
        desc.usage =
            HgiTextureUsageBitsColorTarget |
            HgiTextureUsageBitsShaderRead;
    }

    desc.sampleCount = HgiSampleCount1;

    // create GPU texture
    _texture = _hgi->CreateTexture(desc);

    if (!_texture) {
        TF_DEBUG(NPTRACER_RENDER).Msg("Failed to create HgiTexture");
        return false;
    }
    
    _dimensions = dimensions;
    _format = format;

    TF_DEBUG(NPTRACER_RENDER)
        .Msg("[%s] Render buffer: id=%s, size=%llu\n", TF_FUNC_NAME().c_str(), GetId().GetText(),
             _texture->GetByteSizeOfResource());

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
    if (!_hgi) {
        return nullptr;
    }

    size_t size = 0;
    _mappedBuffer = HdStTextureUtils::HgiTextureReadback(_hgi, _texture, &size);

    return _mappedBuffer.get();
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
    return VtValue(_texture);
}

void NPTracerHdRenderBuffer::_Deallocate()
{
    TF_VERIFY(!IsMapped());

    // reset to default/empty values.
    if (_texture)
    {
        _texture = HgiTextureHandle();
    }

    _dimensions = GfVec3i(-1);
    _format = HdFormatInvalid;

    _mappers.store(0);
    _converged.store(false);
}

PXR_NAMESPACE_CLOSE_SCOPE
