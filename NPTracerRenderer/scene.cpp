#include "scene.h"

#include <iostream>
#include <algorithm>
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

    Texture* missingTex = makePrim<Texture>();
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

void Scene::guard()
{
    std::lock_guard<std::mutex> lock(_readWriteMutex);
}

void Scene::finalize()
{
    constexpr char kDEFAULT_MAT_SCENE_PATH[] = "defaultMat";

    // TEMP: add a default light to the scene if none exist (when debugging)
    if (gDEBUG && _lights.empty())
    {
        auto* light = makePrim<Light>();  // NOTE: instantiated with default values
    }

    if (gDEBUG && _materials.empty())
    {
        auto* material = makePrim<Material>();
        material->diffuse = FLOAT4(1.0f, 0.0f, 1.0f, 1.0f);
        material->scenePath = kDEFAULT_MAT_SCENE_PATH;
    }

    // TEMP: add a default mesh (single triangle) to the scene if none exist to prevent crashes
    if (gDEBUG && _meshes.empty())
    {
        auto* mesh = makePrim<Mesh>();
        mesh->vertices = {
            { { 0.0f, -0.5f, 0.0f, 1 }, { 0, 0, 1, 0 }, { 0, 0, 0, 0 }, { 1, 0, 0, 1 }, { 0, 0 } },
            { { 0.5f, 0.5f, 0.0f, 1 }, { 0, 0, 1, 0 }, { 0, 0, 0, 0 }, { 0, 1, 0, 1 }, { 1, 0 } },
            { { -0.5f, 0.5f, 0.0f, 1 }, { 0, 0, 1, 0 }, { 0, 0, 0, 0 }, { 0, 0, 1, 1 }, { 0, 1 } },
        };
        mesh->indices = { 0, 1, 2 };
        mesh->_materialScenePath = kDEFAULT_MAT_SCENE_PATH;
    }

    // traverse meshes and check if they need to be 'linked' with their material
    for (size_t i = 0; i < _meshes.size(); i++)
    {
        const auto& mesh = _meshes[i];
        if (!mesh->bMaterialNeedsFinalization) continue;  // skip if doesn't need finalization

        auto foundMat = std::ranges::find_if(_materials, [&mesh](const auto& mat)
                                             { return mat->scenePath == mesh->_materialScenePath; });
        if (foundMat == std::end(_materials)) continue;
        mesh->materialIndex = std::distance(_materials.begin(), foundMat);
        mesh->bMaterialNeedsFinalization = false;
    }
}

void Scene::reportState() const
{
    if constexpr (!gDEBUG) return;  // toggle to disable

    // prim counts
    DBG_PRINT("Num Meshes: %llu\n", _meshes.size());
    DBG_PRINT("Num Lights: %llu\n", _lights.size());
    DBG_PRINT("Num Materials: %llu\n", _materials.size());
    DBG_PRINT("Num Textures: %llu\n", _textures.size());

    if constexpr (false) return;  // toggle on for verbose debugging

    // meshes
    for (const auto& mesh : _meshes)
    {
        DBG_PRINT("MESH '%s'\n", mesh->scenePath.c_str());
        std::cerr << "Transform: " << mesh->transform << std::endl;
        DBG_PRINT("Num Indices: %llu\n", mesh->indices.size());
        DBG_PRINT("Num Vertices: %llu\n", mesh->vertices.size());
        DBG_PRINT("Material Index: %u\n", mesh->materialIndex);

        if constexpr (false)  // toggle on for extremely verbose debugging
        {
            for (size_t i = 0; i < mesh->indices.size(); i++)
            {
                uint32_t index = mesh->indices[i];
                const Vertex& vertex = mesh->vertices[index];
                DBG_PRINT("[%llu] MESH INDEX '%u'\n", i, index);
                std::cerr << vertex.pos << " [POSITION]" << std::endl;
                std::cerr << vertex.normal << " [NORMAL]" << std::endl;
                std::cerr << vertex.color << " [COLOR]" << std::endl;
                std::cerr << vertex.uv << " [UV]" << std::endl;
            }
        }
    }

    // lights
    for (const auto& light : _lights)
    {
        DBG_PRINT("LIGHT '%s'\n", light->scenePath.c_str());
        std::cerr << "Transform: " << light->transform << std::endl;
        std::cerr << "Color: " << light->color << std::endl;
        DBG_PRINT("Intensity: %f\n", light->intensity);
    }

    // materials
    for (const auto& mat : _materials)
    {
        DBG_PRINT("MATERIAL '%s'\n", mat->scenePath.c_str());
        std::cerr << "Diffuse Color: " << mat->diffuse << std::endl;
        DBG_PRINT("Diffuse Texture Index: %u\n", mat->diffuseTextureIndex);
    }

    // camera
    {
        std::cerr << "Camera View: " << _camera.view << std::endl;
        std::cerr << "Camera Projection: " << _camera.proj << std::endl;
    }
}

NP_TRACER_NAMESPACE_END
