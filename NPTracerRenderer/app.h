#pragma once

#include "context.h"
#include "scene.h"

#include <memory>

NP_TRACER_NAMESPACE_BEGIN

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

    void create(const RendererConstants& rendererConstants);
    void destroy();

    void loadSceneFromPath(const char* path);

    void createRenderingResources(std::optional<WRAP_REF<RendererAovs>> aovsRef = std::nullopt);
    void executeDrawCall(RendererAovs& aovs);

    void render();

private:
    // this is a runtime constant, just update whenever needed
    static constexpr size_t kPushConstantCount = 4;

    Context mContext{};
    GLFWwindow* mpWindow = nullptr;

    // renderer-level constants
    RendererConstants mRendererConstants{};
    SpecializationConstants mSpecializationConstants{};  // renderer-level consts exposed to shaders

    // rendering resources
    std::unique_ptr<Scene> mpScene;

    Pipeline mRasterPipeline;
    Pipeline mRtPipeline;
    ShaderBindingTable mSbt{};

    std::vector<DescriptorSetLayout> mDescriptorSetLayouts;
    std::vector<VkDescriptorSet> mDescriptorSets;
    VkSampler mSampler = VK_NULL_HANDLE;

    uint32_t mCurrentFrameInFlight = 0u;
    uint32_t mNumLights = 0;
    std::vector<uint32_t> mIndexCounts;

    // SET 0: MESHES
    Buffer mMeshRecordBuffer;
    Buffer mVertexBuffer;
    Buffer mIndexBuffer;
    Buffer mMeshTransformsBuffer;

    // SET 1: LIGHTS
    Buffer mLightRecordBuffer;

    // SET 2: CAMERA
    Buffer mCameraRecordBuffer;

    // SET 3: MATERIALS & TEXTURES
    Buffer mMaterialRecordsBuffer;
    std::vector<Image> mTextures;

    // SET 4: RT
    std::vector<AccelerationStructure> mBlasses;
    AccelerationStructure mTlas{};

    // resource creation
    void createGraphicsPipeline(uint32_t width, uint32_t height, VkFormat format);
    void createRTPipeline();
    void createAccelerationStructures(std::vector<MeshRecord>& meshes,
                                      std::vector<FLOAT4x4>& transforms,
                                      VkDeviceAddress vertexAddress, VkDeviceAddress indexAddress);

    // render commands recording
    void populateDrawCallRaster(Frame& frame, uint32_t imageIndex);
    void populateDrawCallRT(VkCommandBuffer& commandBuffer, VkImage colorAov, VkExtent2D& extent,
                            VkImageLayout dstImageLayout);

    // private execute draw call standalone
    void executeDrawCallSwapchain();

    void createSpecializationConstants();
};

NP_TRACER_NAMESPACE_END
