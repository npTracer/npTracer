#pragma once

#include "structs.h"

#include <vector>
#include <memory>
#include <mutex>

class Scene
{
public:
    virtual ~Scene() = default;

    virtual void loadSceneFromPath(const char* path);  // for compat purposes currently

    template<typename T>
    T* makePrim();

    template<typename T>
    bool deletePrim(T* primToDelete);

    template<typename T>
    size_t getPrimCount() const;

    template<typename T>
    T* getPrimAtIndex(size_t idx);

    inline NPCameraRecord* getCamera()
    {
        return &_camera;
    }

protected:
    std::mutex _readWriteMutex;  // for now temp? keeps i/o single-threaded

    std::vector<std::unique_ptr<NPMesh>> _meshes;

    std::vector<std::unique_ptr<NPLight>> _lights;
    std::vector<std::unique_ptr<NPMaterialRecord>> _materials;

    std::vector<std::unique_ptr<NPTextureRecord>> _textures;

    NPCameraRecord _camera;

    NPRenderSettings _settings;

    template<typename T>
    const std::vector<std::unique_ptr<T>>& getPrimVector() const;

    template<typename T>
    std::vector<std::unique_ptr<T>>& getPrimVector();

    void guard();
};

#include "templates/scene.inl"
