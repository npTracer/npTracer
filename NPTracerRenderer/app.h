#pragma once

#include "context.h"
#include "scene.h"

#include <memory>

class App
{
    static constexpr bool enableDebug =
#ifdef NDEBUG
        false;
#else
        true;
#endif

    static constexpr uint32_t WIDTH = 2560;
    static constexpr uint32_t HEIGHT = 1440;

public:
    inline Context* getContext()
    {
        return &context;
    }

    inline Scene* getScene() const
    {
        return scene.get();
    }

    // interface
    void create();
    void destroy();

    void executeDrawCall(NPRendererAovs& aovs);

    void run();

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
    std::vector<NPMeshRecord> meshRecords;

    NPBuffer meshRecordBuffer;
    std::vector<NPBuffer> vertexBuffers;
    std::vector<NPBuffer> indexBuffers;

    NPBuffer cameraRecordBuffer;

    // resource creation
    void createRenderingResources(NPRendererAovs& aovs);
    void createGraphicsPipeline(NPPipeline& pipeline,
                                std::vector<NPDescriptorSetLayout>& descriptorSetLayouts,
                                NPRendererAovs& aovs);
    void populateDrawCall(VkCommandBuffer& commandBuffer, NPImage* renderTarget);
};
