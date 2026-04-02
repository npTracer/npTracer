#pragma once

#include "scene.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// assimp loading
struct AssimpMeshInstance
{
    const aiMesh* mesh;
    FLOAT4X4 transform;
    std::string nodeName;
};

class AssimpScene final : public Scene
{
public:
    ~AssimpScene() override;

    void loadSceneFromPath(const char* path) override;

private:
    std::unordered_map<std::string, FLOAT4X4> nodeTransforms;
    std::vector<AssimpMeshInstance> pendingMeshes;
    std::unordered_map<std::string, uint32_t> textureIndexByKey;

    void processAiNode(const aiScene* scene, const aiNode* node, const FLOAT4X4& transform);
    void processAiMesh(const aiScene* scene, const aiMesh* currMesh, const FLOAT4X4& localTransform);
    void processAiCamera(const aiScene* scene);
    void processAiLight(const aiLight* light);
};
