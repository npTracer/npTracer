#pragma once

#include "context.h"
#include "scene.h"

#include <memory>

class App
{
    static constexpr bool enableDebug = NPTRACER_DEBUG;
    static constexpr bool standalone = NPTRACER_STANDALONE;
    
    static constexpr uint32_t WIDTH = 2560;
    static constexpr uint32_t HEIGHT = 1440;
    
    std::unique_ptr<NPRendererAovs> m_aovs = nullptr;
    
public:
    inline Context* getContext()
    {
        return &context;
    }

    inline Scene* getScene() const
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

    // rendering resources
    NPPipeline pipeline;
    std::vector<NPDescriptorSetLayout> descriptorSetLayouts;
    std::vector<VkDescriptorSet> descriptorSets;

    std::unique_ptr<Scene> scene;
    std::vector<uint32_t> indexCounts;

    NPBuffer meshRecordBuffer;
    NPBuffer lightRecordBuffer;
    NPBuffer cameraRecordBuffer;
    NPBuffer transformRecordBuffer;
    NPBuffer vertexBuffer;
    NPBuffer indexBuffer;

    // resource creation
    void createGraphicsPipeline();
    
    // render commands recording
    void populateDrawCallCallable(VkCommandBuffer& commandBuffer, NPImage* renderTarget);
    void populateDrawCallSwapchain(VkCommandBuffer& commandBuffer, uint32_t imageIndex);

    // private execute draw call standalone
    void executeDrawCallSwapchain();
};
