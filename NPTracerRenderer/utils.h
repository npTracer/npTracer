#pragma once

#include "framework.h"

#include <fstream>
#include <vector>
#include <string>

NP_TRACER_NAMESPACE_BEGIN

#define DEV_ASSERT(_cond, ...)                                                                     \
    do                                                                                             \
    {                                                                                              \
        if (!(_cond))                                                                              \
        {                                                                                          \
            fprintf(stderr, ##__VA_ARGS__);                                                        \
            abort();                                                                               \
        }                                                                                          \
    } while (0)

#define VK_CHECK(_res, _msg, ...)                                                                  \
    do                                                                                             \
    {                                                                                              \
        VkResult vkRes = _res;                                                                     \
        DEV_ASSERT(vkRes == VK_SUCCESS, "[%i] " _msg, vkRes, ##__VA_ARGS__);                       \
    } while (0)

#ifdef NPTRACER_DEBUG
#define DBG_PRINT(...)                                                                             \
    do                                                                                             \
    {                                                                                              \
        fprintf(stderr, __VA_ARGS__);                                                              \
    } while (0)
#else
#define DBG_PRINT(...)                                                                             \
    do                                                                                             \
    {                                                                                              \
    } while (0)
#endif

// `[[unreachable]]` isn't available until C++23
#ifdef _MSC_VER
#define UNREACHABLE_CODE __assume(0)
#elif defined(__clang__) || defined(__GNUC__)
#define UNREACHABLE_CODE __builtin_unreachable()
#else
#define UNREACHABLE_CODE DEV_ASSERT(false, "reached unreachable code\n");
#endif

static std::vector<char> readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    DEV_ASSERT(file.is_open(), "failed to open file: '%s'\n", filename.c_str());

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

static VkTransformMatrixKHR toVkTransform(const FLOAT4x4& m)
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

NP_TRACER_NAMESPACE_END
