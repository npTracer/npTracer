#pragma once

#include "structs.h"

#include <vector>
#include <memory>
#include <mutex>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

class Scene
{
public:
    Scene();
    ~Scene();

    
    // assimp loading
    void loadSceneAssimp(const char *path);
    void processNode(const aiScene* scene, const aiNode* node, const FLOAT4X4& parentTransform);
    void processMesh(const aiMesh* currMesh, const FLOAT4X4& localTransform);
    void processCamera(const aiScene* scene);
    
    NPMesh* addMesh();
    bool removeMesh(const uint32_t& objectId);

    NPMesh const* getMeshAtIndex(int idx) const;

    inline size_t getMeshCount() const
    {
        return _meshes.size();
    }

    inline const std::vector<std::unique_ptr<NPLight>>& getLights() const
    {
        return _lights;
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

private:
    std::mutex _meshMutex;
    std::vector<std::unique_ptr<NPMesh>> _meshes;

    std::vector<std::unique_ptr<NPLight>> _lights;
    std::vector<FLOAT4X4> _transforms;

    NPCameraRecord _camera = {};

    NPRenderSettings _settings = {};
};
