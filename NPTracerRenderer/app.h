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
    uint32_t currentFrame = 0;

    Context context;
    GLFWwindow* window = nullptr;

    // pipeline
    NPPipeline pipeline;

    // descriptors
    std::vector<NPDescriptorSetLayout> descriptorSetLayouts;
    void createGraphicsPipeline(NPPipeline& pipeline,
                                std::vector<NPDescriptorSetLayout>& descriptorSetLayouts);
    void createDescriptorSets();

    // draw call
    void executeDrawCall(GLFWwindow* window);
    void populateDrawCall(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void updateUniformBuffer();

    // rendering resources
    NPBuffer vertexBuffer;
    NPBuffer indexBuffer;

    Image textureImage;
    VkSampler textureSampler;

    void createRenderingResources();  // placeholder function for testing rendering functionality

    void render();
    void destroy();

public:
    Context& getContext()
    {
        return context;
    }

    void create();
    void run();
};
