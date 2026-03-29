#pragma once

#include <fstream>
#include <filesystem>
#include <sstream>

#define TEXTURE(name) (std::filesystem::path(TEXTURE_PATH) / name)

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

#ifdef NDEBUG
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
