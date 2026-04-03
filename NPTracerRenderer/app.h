#pragma once

#include "context.h"
#include "scene.h"

#include <memory>

template<typename T>
using REF = std::reference_wrapper<T>;

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

    void setAov(std::unique_ptr<NPRendererAovs> aovs)
    {
        aovs = std::move(aovs);
    }

    // interface
    void create(bool isStandalone);
    void destroy();

    void loadSceneFromPath(const char* path);

    void createRenderingResources(std::optional<REF<NPRendererAovs>> aovsRef = std::nullopt);
    void executeDrawCall(NPRendererAovs& aovs);

    void render();

private:
    static constexpr bool kDEBUG = NPTRACER_DEBUG;

    static constexpr uint32_t kWIDTH = 2560;  // default width when standalone
    static constexpr uint32_t kHEIGHT = 1440;  // default width when standalone

    static constexpr int FRAME_COUNT = 2;
    static constexpr uint32_t MAX_RESOURCE_COUNT = 10000;

    bool mIsStandalone = false;
    uint32_t mCurrentRingFrame = 0;

    Context mContext{};
    GLFWwindow* mpWindow = nullptr;

    // push constants
    std::vector<uint32_t> mIndexCounts;
    uint32_t mNumLights = 0;
    static constexpr int kPushConstantCount = 3;

    // rendering resources
    std::unique_ptr<Scene> mpScene;

    NPPipeline mRasterPipeline;
    NPPipeline mRtPipeline;
    NPShaderBindingTable mSbt{};

    std::vector<NPDescriptorSetLayout> mDescriptorSetLayouts;
    std::vector<VkDescriptorSet> mDescriptorSets;
    VkSampler mSampler = VK_NULL_HANDLE;

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
    NPAccelerationStructure mTlas;

    // SET 4: RT
    std::vector<NPAccelerationStructure> blasses;
    NPAccelerationStructure tlas;

    // resource creation
    void createGraphicsPipeline(uint32_t width, uint32_t height, VkFormat format);
    void createRTPipeline();
    void createAccelerationStructures(std::vector<NPMeshRecord>& meshes,
                                      std::vector<FLOAT4X4>& transforms,
                                      VkDeviceAddress vertexAddress, VkDeviceAddress indexAddress);

    // render commands recording
    void populateDrawCallCallable(NPFrame& frame, NPImage* renderTarget);
    void populateDrawCallRaster(NPFrame& frame, uint32_t imageIndex);
    void populateDrawCallRT(VkCommandBuffer& commandBuffer, uint32_t imageIndex);

    // private execute draw call standalone
    void executeDrawCallSwapchain();
};
