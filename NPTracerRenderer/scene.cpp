#include "scene.h"

#include <iostream>
#include <stb_image.h>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/io.hpp>

NP_TRACER_NAMESPACE_BEGIN

constexpr char MISSING_TEXTURE_PATH[] = TEXTURES_ROOT "/missing_texture.png";

Scene::Scene()
{
    if constexpr (!gDEBUG) return;  // do not add default missing texture to scene

    int width, height, channels;
    stbi_uc* pixels = stbi_load(MISSING_TEXTURE_PATH, &width, &height, &channels, STBI_rgb_alpha);
    DEV_ASSERT(pixels, "Failed to load default missing texture asset from '%s'.\n",
               MISSING_TEXTURE_PATH);

    DBG_PRINT(
        "Loaded default missing texture asset from '%s'. Found width=%u, height=%u, channels=%u.\n",
        MISSING_TEXTURE_PATH, width, height, channels);

    NPTexture* missingTex = makePrim<NPTexture>();
    *missingTex = {
        .pixels = reinterpret_cast<void*>(pixels),
        .width = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
    };
}

void Scene::loadSceneFromPath(const char* path)
{
    DEV_ASSERT(false, "not implemented");
}

void Scene::reportState() const
{
    if constexpr (!gDEBUG) return;
    // prim counts
    {
        DBG_PRINT("Num Meshes: %llu\n", _meshes.size());
        DBG_PRINT("Num Lights: %llu\n", _lights.size());
        DBG_PRINT("Num Materials: %llu\n", _materials.size());
        DBG_PRINT("Num Textures: %llu\n", _textures.size());
    }

    // camera
    {
        DBG_PRINT("Camera View:\n");
        std::cerr << _camera.view << std::endl;
        DBG_PRINT("Camera Projection:\n");
        std::cerr << _camera.proj << std::endl;
    }
}

void Scene::guard()
{
    std::lock_guard<std::mutex> lock(_readWriteMutex);
}

NP_TRACER_NAMESPACE_END
