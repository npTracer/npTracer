#pragma once

#include "context.h"

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
    Context& getContext()
    {
        return context;
    }

    // interface
    void create();
    void destroy();

    void executeDrawCall(RendererPayload& payload, VkRendererAovs& aovs);

    void run();

private:
    const std::vector<Vertex> vertices = {
        { { -0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
        { { 0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
        { { 0.5f, 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
        { { -0.5f, 0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f } },

        { { -0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
        { { 0.5f, -0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
        { { 0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
        { { -0.5f, 0.5f, -0.5f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f } }
    };

    const std::vector<uint16_t> indices = { 0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4 };

    static constexpr int FRAME_COUNT = 2;
    static constexpr uint32_t MAX_RESOURCE_COUNT = 10000;
    uint32_t currentFrame = 0;

    Context context;
    GLFWwindow* window = nullptr;

    // rendering resources
    NPPipeline pipeline;
    std::vector<NPDescriptorSetLayout> descriptorSetLayouts;

    std::vector<VkDescriptorSet> descriptorSets;

    RendererPayload payload; // keep payload for now (useful for drawindexed call)
    std::vector<MeshRecord> meshRecords;

    NPBuffer meshRecordBuffer;
    std::vector<NPBuffer> vertexBuffers;
    std::vector<NPBuffer> indexBuffers;

    NPBuffer cameraRecordBuffer;

    // resource creation
    void createRenderingResources(RendererPayload& payload, VkRendererAovs& aovs);
    void createGraphicsPipeline(NPPipeline& pipeline,
                                std::vector<NPDescriptorSetLayout>& descriptorSetLayouts,
                                VkRendererAovs& aovs);
    void populateDrawCall(VkCommandBuffer& commandBuffer, Image& renderTarget);
};
