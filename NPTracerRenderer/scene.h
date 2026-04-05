#pragma once

#include "structs.h"

#include <vector>
#include <memory>
#include <mutex>

NP_TRACER_NAMESPACE_BEGIN

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

    template<typename T>
    T* makePrim();

    template<typename T>
    bool deletePrim(T* primToDelete);

    template<typename T>
    size_t getPrimCount() const;

    template<typename T>
    T* getPrimAtIndex(size_t idx);

    inline Camera* getCamera()
    {
        return &_camera;
    }

    void reportState() const;

protected:
    std::mutex _readWriteMutex;  // for now temp? keeps i/o single-threaded

    std::vector<std::unique_ptr<Mesh>> _meshes;

    std::vector<std::unique_ptr<Light>> _lights;
    std::vector<std::unique_ptr<Material>> _materials;

    std::vector<std::unique_ptr<NPTexture>> _textures;

    Camera _camera;

    RenderSettings _settings;

    template<typename T>
    const std::vector<std::unique_ptr<T>>& getPrimVector() const;

    template<typename T>
    std::vector<std::unique_ptr<T>>& getPrimVector();

    void guard();
};

NP_TRACER_NAMESPACE_END

#include "templates/scene.inl"
