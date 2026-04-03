#pragma once

#include "context.h"
#include "scene.h"

#include <memory>

NP_TRACER_NAMESPACE_BEGIN

template<typename T>
using Ref = std::reference_wrapper<T>;

class App
{
public:
    Context* getContext()
    {
        return &mContext;
    }

    Scene* getScene() const
    {
        return mpScene.get();
    }

    // public interface

    void create(const NPRendererConstants& rendererConstants);
    void destroy();

    void loadSceneFromPath(const char* path);

    void createRenderingResources(std::optional<Ref<NPRendererAovs>> aovsRef = std::nullopt);
    void executeDrawCall(NPRendererAovs& aovs);

    void render();

private:
    static constexpr bool kDebugEnabled = NPTRACER_DEBUG;

    // this is a runtime constant, just update whenever needed
    static constexpr size_t kPushConstantCount = 4;

    Context mContext{};
    GLFWwindow* mpWindow = nullptr;

    // renderer-level constants
    NPRendererConstants mRendererConstants{};

    // rendering resources
    std::unique_ptr<Scene> mpScene;

    NPPipeline mRasterPipeline;
    NPPipeline mRtPipeline;
    NPShaderBindingTable mSbt{};

    std::vector<NPDescriptorSetLayout> mDescriptorSetLayouts;
    std::vector<VkDescriptorSet> mDescriptorSets;
    VkSampler mSampler = VK_NULL_HANDLE;

    uint32_t mCurrentFrameInFlight = 0u;
    uint32_t mNumLights = 0;
    std::vector<uint32_t> mIndexCounts;

    // SET 0: GEOMETRY
    NPBuffer mMeshRecordBuffer;
    NPBuffer mVertexBuffer;
    NPBuffer mIndexBuffer;

    // SET 1: TRANSFORMS
    NPBuffer mGeometryTransformsBuffer;
    NPBuffer mLightTransformsBuffer;

    // SET 2: CAMERA & LIGHTS
    NPBuffer mCameraRecordBuffer;
    NPBuffer mLightRecordBuffer;

    // SET 3: MATERIALS
    NPBuffer mMaterialRecordsBuffer;
    std::vector<NPImage> mTextures;

    // SET 4: RT
    std::vector<NPAccelerationStructure> mBlasses;
    NPAccelerationStructure mTlas{};

    // resource creation
    void createGraphicsPipeline(uint32_t width, uint32_t height, VkFormat format);
    void createRTPipeline();
    void createAccelerationStructures(std::vector<NPMeshRecord>& meshes,
                                      std::vector<FLOAT4X4>& transforms,
                                      VkDeviceAddress vertexAddress, VkDeviceAddress indexAddress);

    // render commands recording
    void populateDrawCallRaster(NPFrame& frame, uint32_t imageIndex);
    void populateDrawCallRT(VkCommandBuffer& commandBuffer, VkImage rgb, VkExtent2D& extent,
                            VkImageLayout dstImageLayout);

    // private execute draw call standalone
    void executeDrawCallSwapchain();
};

NP_TRACER_NAMESPACE_END
