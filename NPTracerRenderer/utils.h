#pragma once

#include <fstream>
#include <filesystem>
#include <vulkan/vulkan.h>

#define TEXTURE(name) (std::filesystem::path(TEXTURE_PATH) / name)

static std::vector<char> readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file!");
    }

    std::vector<char> buffer(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

    file.close();

    return buffer;
}

static uint32_t alignUp(uint32_t value, uint32_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

static VkDeviceSize alignUpVk(VkDeviceSize value, VkDeviceSize alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

static VkTransformMatrixKHR toVkTransform(const FLOAT4X4& m)
{
    VkTransformMatrixKHR out{};
    
    out.matrix[0][0] = m[0][0];
    out.matrix[0][1] = m[1][0];
    out.matrix[0][2] = m[2][0];
    out.matrix[0][3] = m[3][0];

    out.matrix[1][0] = m[0][1];
    out.matrix[1][1] = m[1][1];
    out.matrix[1][2] = m[2][1];
    out.matrix[1][3] = m[3][1];

    out.matrix[2][0] = m[0][2];
    out.matrix[2][1] = m[1][2];
    out.matrix[2][2] = m[2][2];
    out.matrix[2][3] = m[3][2];
    return out;
}