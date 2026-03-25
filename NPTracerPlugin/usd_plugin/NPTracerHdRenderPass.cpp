#include "usd_plugin/NPTracerHdRenderPass.h"

#include "usd_plugin/debugCodes.h"
#include "usd_plugin/hdMathUtils.h"
#include "usd_plugin/NPTracerHdRenderBuffer.h"

#include <pxr/imaging/hd/camera.h>
#include <pxr/imaging/hd/renderPassState.h>
#include <pxr/imaging/hd/renderBuffer.h>

#include <glm/gtc/type_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE

struct ScreenVertex
{
    glm::vec3 pos;   // screen space (x, y, z)
    glm::vec3 color; // using normals as color
};

static glm::vec3 ToNDC(const glm::vec4& clip)
{
    return glm::vec3(clip) / clip.w;
}

static glm::vec3 ToScreen(const glm::vec3& ndc, int width, int height)
{
    return {
        (ndc.x * 0.5f + 0.5f) * width,
        (1.0f - (ndc.y * 0.5f + 0.5f)) * height,
        ndc.z
    };
}

constexpr float EPSILON = 0.001f;

static float EdgeFunction(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c)
{
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

void RenderSceneToBuffer(Scene* scene,
                         NPCameraRecord* cam,
                         int width,
                         int height,
                         void* buffer)
{
    std::vector<float> depth(width * height, std::numeric_limits<float>::infinity());
    uint8_t* pixels = reinterpret_cast<uint8_t*>(buffer);

    // clear color
    memset(pixels, 0, width * height * 4);

    glm::mat4 V = cam->view;
    glm::mat4 P = cam->proj;

    for (size_t m = 0; m < scene->getMeshCount(); m++)
    {
        const NPMesh* mesh = scene->getMeshAtIndex(m);
        if (!mesh) continue;

        glm::mat4 M = mesh->objectToWorld;

        size_t vcount = mesh->vertices.size();

        // triangle list
        for (size_t i = 0; i + 2 < vcount; i += 3)
        {
            ScreenVertex sv[3];

            for (int k = 0; k < 3; k++)
            {
                const NPVertex& v = mesh->vertices[i + k];

                FLOAT4 world = M * FLOAT4(v.pos, 1.0f);
                FLOAT4 clip  = P * V * world;
                if (clip.w <= EPSILON) continue;

                glm::vec3 ndc   = ToNDC(clip);
                glm::vec3 screen = ToScreen(ndc, width, height);

                sv[k].pos = screen;
                sv[k].color = v.color;
            }

            // bounding box
            int minX = (int)std::floor(std::min({sv[0].pos.x, sv[1].pos.x, sv[2].pos.x}));
            int maxX = (int)std::ceil (std::max({sv[0].pos.x, sv[1].pos.x, sv[2].pos.x}));
            int minY = (int)std::floor(std::min({sv[0].pos.y, sv[1].pos.y, sv[2].pos.y}));
            int maxY = (int)std::ceil (std::max({sv[0].pos.y, sv[1].pos.y, sv[2].pos.y}));

            minX = std::max(minX, 0);
            minY = std::max(minY, 0);
            maxX = std::min(maxX, width - 1);
            maxY = std::min(maxY, height - 1);

            glm::vec2 p0 = glm::vec2(sv[0].pos);
            glm::vec2 p1 = glm::vec2(sv[1].pos);
            glm::vec2 p2 = glm::vec2(sv[2].pos);

            float area = EdgeFunction(p0, p1, p2);
            if (area <= EPSILON) continue;

            for (int y = minY; y <= maxY; y++)
            {
                for (int x = minX; x <= maxX; x++)
                {
                    glm::vec2 p(x + 0.5f, y + 0.5f);

                    float w0 = EdgeFunction(p1, p2, p);
                    float w1 = EdgeFunction(p2, p0, p);
                    float w2 = EdgeFunction(p0, p1, p);

                    if ((w0 >= 0 && w1 >= 0 && w2 >= 0) ||
                        (w0 <= 0 && w1 <= 0 && w2 <= 0))
                    {
                        float invArea = 1.0f / std::max(EPSILON, area);
                        w0 *= invArea;
                        w1 *= invArea;
                        w2 *= invArea;

                        glm::vec3 color =
                            w0 * sv[0].color +
                            w1 * sv[1].color +
                            w2 * sv[2].color;

                        // convert [-1,1] normal range to [0,1]
                        color = color * 0.5f + 0.5f;

                        int idx = (y * width + x);
                        
                        float z =
                            w0 * sv[0].pos.z +
                            w1 * sv[1].pos.z +
                            w2 * sv[2].pos.z;

                        if (z < depth[idx])
                        {
                            depth[idx] = z; // basic depth filtering

                            int pixelIdx = idx * 4;

                            pixels[pixelIdx + 0] = (uint8_t)(color.r * 255.f);
                            pixels[pixelIdx + 1] = (uint8_t)(color.g * 255.f);
                            pixels[pixelIdx + 2] = (uint8_t)(color.b * 255.f);
                            pixels[pixelIdx + 3] = 255;
                            glm::vec3 rgb = glm::vec3(pixels[pixelIdx + 0], pixels[pixelIdx + 1], pixels[pixelIdx + 2]);
                            rgb = color;
                            if (rgb.length() > EPSILON)
                            {
                                NP_DBG("(%d, %d, %d)", rgb[0], rgb[1], rgb[2]);
                            }
                        }
                    }
                }
            }
        }
    }
}

NPTracerHdRenderPass::NPTracerHdRenderPass(HdRenderIndex* index,
                                           HdRprimCollection const& collection,
                                           NPTracerHdRenderDelegate* delegate)
    : HdRenderPass(index, collection), _pCreator(delegate)
{
}

void NPTracerHdRenderPass::_Execute(HdRenderPassStateSharedPtr const& renderPassState,
                                    TfTokenVector const& renderTags)
{
    this->SetConverged(false);

    App* app = _pCreator->GetRendererApp();
    NPCameraRecord* cam = app->getScene()->getCamera();
    _SyncCamera(renderPassState, cam);

    this->SetConverged(false);

    HdRenderPassAovBindingVector aovBindings = renderPassState->GetAovBindings();

    for (HdRenderPassAovBinding const& aov : aovBindings)
    {
        NPTracerHdRenderBuffer* buffer = dynamic_cast<NPTracerHdRenderBuffer*>(aov.renderBuffer);
        if (!buffer)  // `dynamic_cast` failed
        {
            continue;
        }
        buffer->SetConverged(false);

        if (aov.aovName == HdAovTokens->color)
        {
            void* data = buffer->Map();

            RenderSceneToBuffer(app->getScene(), cam,
                                buffer->GetWidth(),
                                buffer->GetHeight(),
                                data);

            buffer->Unmap();
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

void NPTracerHdRenderPass::_SyncCamera(HdRenderPassStateSharedPtr const& renderPassState,
                                       NPCameraRecord* outCam) const
{
    HdCamera const* hdCam = renderPassState->GetCamera();
    outCam->model = GfMatrix4dToGLM(hdCam->GetTransform());
    outCam->view = GfMatrix4dToGLM(renderPassState->GetWorldToViewMatrix());
    outCam->proj = GfMatrix4dToGLM(renderPassState->GetProjectionMatrix());
}

PXR_NAMESPACE_CLOSE_SCOPE
