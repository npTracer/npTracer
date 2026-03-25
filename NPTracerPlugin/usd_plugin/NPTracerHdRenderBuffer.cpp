#include "usd_plugin/NPTracerHdRenderBuffer.h"

#include "usd_plugin/debugCodes.h"
#include "usd_plugin/NPTracerHdRenderParam.h"

PXR_NAMESPACE_OPEN_SCOPE

NPTracerHdRenderBuffer::NPTracerHdRenderBuffer(const SdfPath& bprimId, Context* context)
    : HdRenderBuffer(bprimId), _pCtx(context)
{
}

bool NPTracerHdRenderBuffer::Allocate(const GfVec3i& dimensions, HdFormat format, bool multiSampled)
{
    NP_DBG("[%s] Allocate render buffer: id=%s, dimensions=(%i, %i, %i), "
           "format=%i\n",
           TF_FUNC_NAME().c_str(), GetId().GetText(), dimensions[0], dimensions[1], dimensions[2],
           format);

    _Deallocate();

    _dimensions = dimensions;
    _format = format;
    _multiSampled = multiSampled;
    _fmtTokens = Np::GetFormatTokens(format);

    if (format == HdFormatInvalid)
    {
        return false;
    }

    // enforce 2D buffer for now
    TF_VERIFY(dimensions[2] == 1);

    _dimensions = dimensions;
    _format = format;
    _buffer.resize(_dimensions[0] * _dimensions[1] * _dimensions[2] * HdDataSizeOfFormat(format));

    NP_DBG("[%s] Render buffer allocated: id=%s\n", TF_FUNC_NAME().c_str(), GetId().GetText());

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

    return _buffer.data();
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

void NPTracerHdRenderBuffer::_Deallocate()
{
    // reset to default/empty values
    _dimensions = GfVec3i(-1);
    _format = HdFormatInvalid;

    _mappers.store(0);
    _converged.store(false);
}

PXR_NAMESPACE_CLOSE_SCOPE
