#pragma once

#include <glm/glm.hpp>
#include <array>

struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;

    // tell vulkan how vertices should be moved through
    static VkVertexInputBindingDescription getBindingDescription()
    {
        return { 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX };
    }

    // tell vulkan what attributes exist within each vertex and how big they are
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        return { VkVertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32_SFLOAT,
                                                   offsetof(Vertex, pos)),
                 VkVertexInputAttributeDescription(1, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                                   offsetof(Vertex, color)) };
    }
};