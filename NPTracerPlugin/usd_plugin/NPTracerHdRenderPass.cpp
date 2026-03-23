#include "usd_plugin/NPTracerHdRenderPass.h"
#include "usd_plugin/NPTracerDebugCodes.h"

#include "usd_plugin/NPTracerHdRenderBuffer.h"

#include <pxr/imaging/hd/renderPassState.h>
#include <pxr/imaging/hd/renderBuffer.h>

PXR_NAMESPACE_OPEN_SCOPE

// simple point inside triangle test
static bool PointInsideTriangle(const GfVec2f& point, const GfVec2f triangle[3])
{
    float dX = point[0] - triangle[2][0];
    float dY = point[1] - triangle[2][1];
    float dX21 = triangle[2][0] - triangle[1][0];
    float dY12 = triangle[1][1] - triangle[2][1];
    float D = dY12 * (triangle[0][0] - triangle[2][0]) +
              dX21 * (triangle[0][1] - triangle[2][1]);
    float s = dY12 * dX + dX21 * dY;
    float t = (triangle[2][1] - triangle[0][1]) * dX +
              (triangle[0][0] - triangle[2][0]) * dY;
    if (D < 0) {
        return s <= 0 && t <= 0 && s + t >= D;
    } else {
        return s >= 0 && t >= 0 && s + t <= D;
    }
}


static void RenderTriangle2DCPU(NPTracerHdRenderBuffer* colorBuffer)
{
    HdFormat format = colorBuffer->GetFormat();
    size_t pixelSize = HdDataSizeOfFormat(format); // size of each pixel
    size_t componentCount = HdGetComponentCount(format); // component count of each pixl
    
    uint32_t width = colorBuffer->GetWidth();
    float fWidth = static_cast<float>(width);
    uint32_t height = colorBuffer->GetHeight();
    float fHeight = static_cast<float>(height);

    // get internally stored buffer
    uint8_t* data = static_cast<uint8_t*>(colorBuffer->Map());

    // create points for simple hard-coded screen-space triangle
    float sideLength = fWidth * 0.45f;
    GfVec2f centerOffset(fWidth * 0.5f, fHeight * 0.41f);
    GfVec2f trianglePoints[3] = {
        GfVec2f(0.0f, 0.57735027f * sideLength) + centerOffset,
        GfVec2f(-0.5f * sideLength, -0.28867513f * sideLength) + centerOffset,
        GfVec2f(0.5f * sideLength, -0.28867513f * sideLength) + centerOffset
    };

    // iterate over all raster coordinates in buffer
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            // compute the pixel index
            uint32_t pixelIndex = (y * width) + x;
            
            // check if the coordinate is inside the 2D triangle
            GfVec2f coord(static_cast<float>(x), static_cast<float>(y));
            if (PointInsideTriangle(coord, trianglePoints)) {
                // compute the byte offset using pixel byte size
                uint8_t* dst = &data[pixelIndex * pixelSize];
                
                // fill with white color
                for (size_t c = 0; c < componentCount; ++c) {
                    dst[c] = 255;
                }
            }
        }
    }

    // end buffer write
    colorBuffer->Unmap();
}


NPTracerHdRenderPass::NPTracerHdRenderPass(HdRenderIndex* index,
                                           HdRprimCollection const& collection,
                                           NPTracerHdRenderDelegate* delegate)
    : HdRenderPass(index, collection), _delegate(delegate)
{
}

void NPTracerHdRenderPass::_Execute(HdRenderPassStateSharedPtr const& renderPassState,
                                    TfTokenVector const&)
{
    this->SetConverged(false);
    
    HdRenderPassAovBindingVector aovBindings = renderPassState->GetAovBindings();
    
    RendererPayload payload = _BuildRendererPayload(renderPassState);

    for (HdRenderPassAovBinding const& aov : aovBindings)
    {
        NPTracerHdRenderBuffer* buffer = dynamic_cast<NPTracerHdRenderBuffer*>(aov.renderBuffer);
        if (!buffer) // `dynamic_cast` failed
        {
            continue;
        }
        buffer->SetConverged(false);
        
        if (aov.aovName == HdAovTokens->color) {
            RenderTriangle2DCPU(buffer);
        }
        
        buffer->SetConverged(true);
        
    }
     this->SetConverged(true);
}

bool NPTracerHdRenderPass::IsConverged() const
{
    return _converged.load();
}

void NPTracerHdRenderPass::SetConverged(bool converged)
{
    _converged.store(converged);
}

RendererPayload NPTracerHdRenderPass::_BuildRendererPayload(HdRenderPassStateSharedPtr const& state)
{
    return {};
}

VkRendererAovs NPTracerHdRenderPass::_ExtractAovs(HdRenderPassStateSharedPtr const& state)
{
    return {};
}

PXR_NAMESPACE_CLOSE_SCOPE
