#pragma once

#include "structs.h"

#include <vector>
#include <memory>
#include <mutex>

class Scene
{
public:
    virtual ~Scene() = default;

    virtual void loadSceneFromPath(const char* path);

    NPMesh* addMesh();
    bool removeMesh(const uint32_t& objectId);

    NPMesh const* getMeshAtIndex(int idx) const;

    inline size_t getMeshCount() const
    {
        return _meshes.size();
    }

    NPLight const* getLightAtIndex(int idx) const;

    inline size_t getLightCount() const
    {
        return _lights.size();
    }

    inline NPCameraRecord* getCamera()
    {
        return &_camera;
    }

    inline NPCameraRecord const* getCamera() const
    {
        return &_camera;
    }

    inline size_t getMaterialCount() const
    {
        return _materials.size();
    }

    const NPMaterial* getMaterialAtIndex(int idx) const;

    inline size_t getTextureCount() const
    {
        return _textures.size();
    }

    const NPTexture* getTextureAtIndex(int idx) const;

protected:
    std::mutex _meshMutex;
    std::vector<std::unique_ptr<NPMesh>> _meshes;
    std::vector<FLOAT4X4> _transforms;

    std::vector<std::unique_ptr<NPLight>> _lights;
    std::vector<std::unique_ptr<NPMaterial>> _materials;

    std::vector<std::unique_ptr<NPTexture>> _textures;

    NPCameraRecord _camera;

    NPRenderSettings _settings;
};
