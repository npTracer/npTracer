#pragma once

#include "structs.h"

#include <vector>
#include <memory>
#include <mutex>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <unordered_map>

class Scene
{
public:
    Scene();
    ~Scene();
    
    // assimp loading
    struct PendingMeshInstance
    {
        const aiMesh* mesh;
        FLOAT4X4 transform;
        std::string nodeName;
    };

    std::unordered_map<std::string, FLOAT4X4> nodeTransforms;
    std::vector<std::unique_ptr<PendingTexture>> pendingTextures;
    std::vector<PendingMeshInstance> pendingMeshes;
    std::unordered_map<std::string, uint32_t> textureIndexByKey;
    
    void loadSceneAssimp(const char *path);
    void processNode(const aiScene* scene, const aiNode* node, const FLOAT4X4& transform);
    void processMesh(const aiScene* scene, const aiMesh* currMesh, const FLOAT4X4& localTransform);
    void processCamera(const aiScene* scene);
    void processLight(const aiLight* light);
    
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
    
    inline const std::vector<std::unique_ptr<NPMaterial>>& getMaterials() const
    {
        return _materials;
    }
    
    inline size_t getMaterialCount() const
    {
        return _materials.size();
    }
    
    NPMaterial const* getMaterialAtIndex(int idx) const;

private:
    std::mutex _meshMutex;
    std::vector<std::unique_ptr<NPMesh>> _meshes;

    std::vector<std::unique_ptr<NPLight>> _lights;
    std::vector<FLOAT4X4> _transforms;
    std::vector<std::unique_ptr<NPMaterial>> _materials;
    
    NPCameraRecord _camera = {};

    NPRenderSettings _settings = {};
};
