#include "scene.h"

#include <algorithm>
#include <stb_image.h>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/io.hpp>

NP_TRACER_NAMESPACE_BEGIN

constexpr char MISSING_TEXTURE_PATH[] = TEXTURES_ROOT "/missing_texture.png";

Scene::Scene()
{
    _meshes.reserve(kMAX_MESHES);
    _meshRecords.reserve(kMAX_MESHES);

    _lights.reserve(kMAX_LIGHTS);
    _lightRecords.reserve(kMAX_LIGHTS);

    _materials.reserve(kMAX_MATERIALS);
    _materialRecords.reserve(kMAX_MATERIALS);

    _textures.reserve(kMAX_TEXTURES);
    _textureRecords.reserve(kMAX_TEXTURES);

    if constexpr (!gDEBUG) return;  // do not add default missing texture to scene

    {
        int width, height, channels;
        stbi_uc* pixels = stbi_load(MISSING_TEXTURE_PATH, &width, &height, &channels,
                                    STBI_rgb_alpha);
        DEV_ASSERT(pixels, "Failed to load default missing texture asset from '%s'.\n",
                   MISSING_TEXTURE_PATH);

        Texture* missingTex = makePrim<Texture>();
        *missingTex = {
            .pixels = reinterpret_cast<void*>(pixels),
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
        };
    }
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
    if constexpr (gDEBUG)
    {
        constexpr char kDEFAULT_MAT_SCENE_PATH[] = "defaultMat";

        // TEMP: add a default light to the scene if none exist to prevent crashes
        if (_lights.empty()) makePrim<Light>();  // NOTE: instantiated with default values

        // TEMP: add a default material to the scene if none exist to prevent crashes
        if (_materials.empty())
        {
            auto* material = makePrim<Material>();
            material->diffuse = FLOAT4(1.0f, 0.0f, 1.0f, 1.0f);
            material->scenePath = kDEFAULT_MAT_SCENE_PATH;
        }

        // TEMP: add a default mesh (single triangle) to the scene if none exist to prevent crashes
        if (_meshes.empty())
        {
            auto* mesh = makePrim<Mesh>();
            mesh->vertices = {
                { { 0.0f, -0.5f, 0.0f, 1 }, { 0, 0, 1, 0 }, { 0, 0, 0, 0 }, { 1, 0, 0, 1 }, { 0, 0 } },
                { { 0.5f, 0.5f, 0.0f, 1 }, { 0, 0, 1, 0 }, { 0, 0, 0, 0 }, { 0, 1, 0, 1 }, { 1, 0 } },
                { { -0.5f, 0.5f, 0.0f, 1 }, { 0, 0, 1, 0 }, { 0, 0, 0, 0 }, { 0, 0, 1, 1 }, { 0, 1 } },
            };
            mesh->indices = { 0, 1, 2 };
            mesh->materialScenePath = kDEFAULT_MAT_SCENE_PATH;
        }
    }

    // traverse meshes and check if they need to be 'linked' with their material
    for (Mesh& mesh : _meshes)
    {
        if (!mesh.bMaterialNeedsFinalization) continue;  // skip if doesn't need finalization

        auto foundMat = std::ranges::find_if(_materials, [&mesh](const Material& mat)
                                             { return mat.scenePath == mesh.materialScenePath; });
        if (foundMat == std::end(_materials)) continue;
        mesh.materialIndex = std::distance(_materials.begin(), foundMat);
    }
}

void Scene::reportState() const
{
    // prim counts
    DBG_PRINT("Num Meshes: %llu\n", getPrimVector<Mesh>().size());
    DBG_PRINT("Num Lights: %llu\n", getPrimVector<Light>().size());
    DBG_PRINT("Num Materials: %llu\n", getPrimVector<Material>().size());
    DBG_PRINT("Num Textures: %llu\n", getPrimVector<Texture>().size());
}

NP_TRACER_NAMESPACE_END
