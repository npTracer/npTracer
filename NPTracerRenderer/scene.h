#pragma once

#include "structs.h"

#include <vector>
#include <memory>
#include <mutex>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <unordered_map>

// assimp loading
struct AssimpMeshInstance
{
    const aiMesh* mesh;
    FLOAT4X4 transform;
    std::string nodeName;
};

class Scene
{
public:
    Scene();
    ~Scene();

    std::unordered_map<std::string, FLOAT4X4> nodeTransforms;
    std::vector<std::unique_ptr<PendingTexture>> pendingTextures;
    std::vector<AssimpMeshInstance> pendingMeshes;
    std::unordered_map<std::string, uint32_t> textureIndexByKey;

    void loadSceneAssimp(const char* path);
    void processNode(const aiScene* scene, const aiNode* node, const FLOAT4X4& transform);
    void processMesh(const aiScene* scene, const aiMesh* currMesh, const FLOAT4X4& localTransform);
    void processCamera(const aiScene* scene);
    void processLight(const aiLight* light);

    NPMesh* addMesh();
    bool removeMesh(const uint32_t& objectId);

    const NPMesh* getMeshAtIndex(int idx) const;

    size_t getMeshCount() const
    {
        return _meshes.size();
    }

    const std::vector<std::unique_ptr<NPLight>>& getLights() const
    {
        return _lights;
    }

    const NPLight* getLightAtIndex(int idx) const;

    size_t getLightCount() const
    {
        return _lights.size();
    }

    NPCameraRecord* getCamera()
    {
        return &_camera;
    }

    const std::vector<std::unique_ptr<NPMaterial>>& getMaterials() const
    {
        return _materials;
    }

    size_t getMaterialCount() const
    {
        return _materials.size();
    }

    const NPMaterial* getMaterialAtIndex(int idx) const;

private:
    std::mutex _meshMutex;
    std::vector<std::unique_ptr<NPMesh>> _meshes;

    std::vector<std::unique_ptr<NPLight>> _lights;
    std::vector<FLOAT4X4> _transforms;
    std::vector<std::unique_ptr<NPMaterial>> _materials;

    NPCameraRecord _camera;

    NPRenderSettings _settings;
};
