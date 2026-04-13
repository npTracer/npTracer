#pragma once

#include "scene.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>

#include <functional>
#include <string>

NP_TRACER_NAMESPACE_BEGIN

class AssimpScene final : public Scene
{
public:
    ~AssimpScene() override;

    inline eSceneType getSceneType() override
    {
        return eSceneType::ASSIMP;
    }

    void loadSceneFromPath(const char* path) override;

private:
    // assimp loading
    struct AssimpMeshInstance
    {
        const aiMesh* mesh;
        FLOAT4x4 transform;
        std::string nodeName;
    };

    std::unordered_map<std::string, FLOAT4x4> nodeTransforms;
    std::vector<AssimpMeshInstance> pendingMeshes;
    std::unordered_map<std::string, uint32_t> textureIndexByKey;

    std::hash<std::string> idHasher;

    void processAiNode(const aiScene* scene, const aiNode* node, const FLOAT4x4& transform);
    void processAiMesh(const aiScene* scene, const aiMesh* inAiMesh, const FLOAT4x4& localTransform);
    void processAiCamera(const aiScene* inAiScene);
    void processAiLight(const aiLight* inAiLight);
    void loadAndSetTexture(const aiScene* scene, const aiMaterial* aiMat, aiTextureType textureType, uint32_t& targetIndex);
};

NP_TRACER_NAMESPACE_END
