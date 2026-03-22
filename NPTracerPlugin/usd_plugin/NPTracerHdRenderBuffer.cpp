#include "NPTracerHdRenderBuffer.h"
#include "NPTracerDebugCodes.h"

PXR_NAMESPACE_OPEN_SCOPE

NPTracerHdRenderBuffer::NPTracerHdRenderBuffer(const SdfPath& bprimId)
    : HdRenderBuffer(bprimId)
{
}

bool NPTracerHdRenderBuffer::Allocate(const GfVec3i& dimensions,
                                 HdFormat format,
                                 bool multiSampled)
{
    TF_DEBUG(NPTRACER_GENERAL)
        .Msg("[%s] Allocate render buffer: id=%s, dimensions=(%i, %i, %i), "
             "format=%i\n",
             TF_FUNC_NAME().c_str(),
             GetId().GetText(),
             dimensions[0],
             dimensions[1],
             dimensions[2],
             format);

    _Deallocate();

    // enforce 2D buffer for now
    TF_VERIFY(dimensions[2] == 1);

    _dimensions = dimensions;
    _format = format;
    _buffer.resize(_dimensions[0] * _dimensions[1] * _dimensions[2] *
                   HdDataSizeOfFormat(format));

    TF_DEBUG(NPTRACER_GENERAL)
        .Msg("[%s] Render buffer: id=%s, size=%llu\n",
             TF_FUNC_NAME().c_str(),
             GetId().GetText(),
             _buffer.size());

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
    return false;
}

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
    // nothing to do, there is only a single internal buffer for read/write
    return;
}

void NPTracerHdRenderBuffer::_Deallocate()
{
    TF_VERIFY(!IsMapped());

    // reset to default/empty values.
    _dimensions = GfVec3i(-1, -1, -1);
    _format = HdFormatInvalid;
    _buffer.resize(0);
    _mappers.store(0);
    _converged.store(false);
}

PXR_NAMESPACE_CLOSE_SCOPE
