#pragma once

#include "structs.h"

#include <vector>
#include <mutex>

NP_TRACER_NAMESPACE_BEGIN

constexpr uint32_t kMAX_LIGHTS = 1024;
constexpr uint32_t kMAX_MATERIALS = 4096;
constexpr uint32_t kMAX_TEXTURES = 4096;
constexpr uint32_t kMAX_MESHES = 8192;

class Scene
{
public:
    Scene();
    virtual ~Scene() = default;

    virtual void loadSceneFromPath(const char* path);  // for compat purposes currently

    inline virtual eSceneType getSceneType()
    {
        return eSceneType::DEFAULT;
    }

    template<ScenePrim T>
    T* makePrim();

    template<ScenePrim T>
    bool deletePrim(T* primToDelete);

    template<ScenePrim T>
    [[nodiscard]] size_t getPrimCount() const;

    template<ScenePrim T>
    T* getPrimAtIndex(size_t idx);

    inline Camera* getCamera()
    {
        return &_camera;
    }

    void guard();
    void finalize();
    void reportState() const;

protected:
    std::mutex _readWriteMutex;  // keeps i/o single-threaded

    std::vector<Mesh> _meshes;
    std::vector<Light> _lights;
    std::vector<Material> _materials;
    std::vector<Texture> _textures;

    std::vector<MeshRecord> _meshRecords;
    std::vector<LightRecord> _lightRecords;
    std::vector<MaterialRecord> _materialRecords;
    std::vector<TextureRecord> _textureRecords;

    Camera _camera;

    template<ScenePrim T>
    const std::vector<T>& getPrimVector() const;  // constant overload

    template<ScenePrim T>
    std::vector<T>& getPrimVector();

    template<SceneDeviceLocalPrim T>
    const std::vector<T>& getDeviceLocalPrimVector() const;  // constant overload

    template<SceneDeviceLocalPrim T>
    std::vector<T>& getDeviceLocalPrimVector();
};

NP_TRACER_NAMESPACE_END

#include "templates/scene.inl"
