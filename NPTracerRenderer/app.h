#pragma once

#include "context.h"
#include "scene.h"

#include <memory>

class App
{
    static constexpr bool kDEBUG = NPTRACER_DEBUG;
    static constexpr bool kSTANDALONE = NPTRACER_STANDALONE;

    static constexpr uint32_t WIDTH = 2560;
    static constexpr uint32_t HEIGHT = 1440;

    std::unique_ptr<NPRendererAovs> m_aovs = nullptr;

public:
    Context* getContext()
    {
        return &context;
    }

    Scene* getScene() const
    {
        return scene.get();
    }

    void setAov(std::unique_ptr<NPRendererAovs> aovs)
    {
        m_aovs = std::move(aovs);
    }

    // interface
    void create();
    void destroy();

    void loadScene(const char* path);

    void createRenderingResources();
    void executeDrawCallCallable(NPRendererAovs* aovs = nullptr);

    void run();
    void render();

private:
    static constexpr int FRAME_COUNT = 2;
    static constexpr uint32_t MAX_RESOURCE_COUNT = 10000;
    uint32_t currentFrame = 0;

    Context context;
    GLFWwindow* window = nullptr;

    // push constants
    std::vector<uint32_t> indexCounts;
    uint32_t numLights = 0;

    // rendering resources
    std::unique_ptr<Scene> scene;
    
    NPPipeline rasterPipeline;
    NPPipeline rtPipeline;
    ShaderBindingTable sbt;
    
    std::vector<NPDescriptorSetLayout> descriptorSetLayouts;
    std::vector<VkDescriptorSet> descriptorSets;
    VkSampler sampler = VK_NULL_HANDLE;

    // SET 0: GEOMETRY
    NPBuffer meshRecordBuffer;
    NPBuffer vertexBuffer;
    NPBuffer indexBuffer;

    // SET 1: TRANSFORMS
    NPBuffer geometryTransformsBuffer;
    NPBuffer lightTransformsBuffer;

    // SET 2: CAMERA & LIGHTS
    NPBuffer cameraRecordBuffer;
    NPBuffer lightRecordBuffer;

    // SET 3: MATERIALS
    NPBuffer materialRecordsBuffer;
    std::vector<NPImage> textures;
    
    // SET 4: RT
    std::vector<NPAccelerationStructure> blasses;
    NPAccelerationStructure tlas;
    
    // resource creation
    void createGraphicsPipeline();
    void createRTPipeline();
    void createAccelerationStructures(
        std::vector<NPMeshRecord>& meshes, 
        std::vector<FLOAT4X4>& transforms, 
        VkDeviceAddress vertexAddress, 
        VkDeviceAddress indexAddress);
    
    // render commands recording
    void populateDrawCallCallable(NPFrame& frame, NPImage* renderTarget);
    void populateDrawCallRaster(NPFrame& frame, uint32_t imageIndex);
    void populateDrawCallRT(VkCommandBuffer& commandBuffer, uint32_t imageIndex);
    
    // private execute draw call standalone
    void executeDrawCallSwapchain();
};
