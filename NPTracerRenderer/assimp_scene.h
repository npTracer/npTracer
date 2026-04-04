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
        FMat4 transform;
        std::string nodeName;
    };

    std::unordered_map<std::string, FMat4> nodeTransforms;
    std::vector<AssimpMeshInstance> pendingMeshes;
    std::unordered_map<std::string, uint32_t> textureIndexByKey;

    std::hash<std::string> idHasher;

    void processAiNode(const aiScene* scene, const aiNode* node, const FMat4& transform);
    void processAiMesh(const aiScene* scene, const aiMesh* currMesh, const FMat4& localTransform);
    void processAiCamera(const aiScene* scene);
    void processAiLight(const aiLight* light);
};

NP_TRACER_NAMESPACE_END
