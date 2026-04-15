#pragma once

#include "structs.h"

#include <vector>
#include <memory>
#include <mutex>

NP_TRACER_NAMESPACE_BEGIN

template<typename T>
concept ScenePrim = std::is_same_v<T, Mesh> || std::is_same_v<T, Light>
                    || std::is_same_v<T, Material> || std::is_same_v<T, Texture>;

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
    size_t getPrimCount() const;

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
    std::mutex _readWriteMutex;  // for now temp? keeps i/o single-threaded

    std::vector<UPTR<Mesh>> _meshes;

    std::vector<UPTR<Light>> _lights;
    std::vector<UPTR<Material>> _materials;

    std::vector<UPTR<Texture>> _textures;

    Camera _camera;

    RenderSettings _settings;

    template<ScenePrim T>
    const std::vector<UPTR<T>>& getPrimVector() const;  // constant overload

    template<ScenePrim T>
    std::vector<UPTR<T>>& getPrimVector();
};

NP_TRACER_NAMESPACE_END

#include "templates/scene.inl"
