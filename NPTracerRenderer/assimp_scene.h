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

    inline NPSceneType getSceneType() override
    {
        return NPSceneType::ASSIMP;
    }

    void loadSceneFromPath(const char* path) override;

private:
    // assimp loading
    struct AssimpMeshInstance
    {
        const aiMesh* mesh;
        FLOAT4X4 transform;
        std::string nodeName;
    };

    std::unordered_map<std::string, FLOAT4X4> nodeTransforms;
    std::vector<AssimpMeshInstance> pendingMeshes;
    std::unordered_map<std::string, uint32_t> textureIndexByKey;

    std::hash<std::string> idHasher;

    void processAiNode(const aiScene* scene, const aiNode* node, const FLOAT4X4& transform);
    void processAiMesh(const aiScene* scene, const aiMesh* currMesh, const FLOAT4X4& localTransform);
    void processAiCamera(const aiScene* scene);
    void processAiLight(const aiLight* light);
};

NP_TRACER_NAMESPACE_END
