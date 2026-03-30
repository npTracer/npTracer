#include "scene.h"
#include <iostream>
Scene::Scene() {}

Scene::~Scene() {}

static FLOAT4X4 aiToGlm(const aiMatrix4x4& m)
{
    FLOAT4X4 mat = FLOAT4X4(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4
    );
    return mat;
}

void Scene::loadSceneAssimp(const char* path)
{
    if (!path)
    {
        throw std::runtime_error("failed to load scene");
    }
    
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, 
        aiProcess_MakeLeftHanded |
        aiProcess_Triangulate |
        aiProcess_GlobalScale |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices);
    
    if (!scene) throw std::runtime_error("failed to load scene");
    
    const aiNode* root = scene->mRootNode;
    
    // visit all nodes
    processNode(scene, root, FLOAT4X4(1.0));
    
    for (const auto& inst : pendingMeshes)
    {
        processMesh(scene, inst.mesh, inst.transform);
    }
    
    for (uint32_t i = 0; i < scene->mNumLights; i++)
    {
        processLight(scene->mLights[i]);
    }
    
    processCamera(scene);
}

void Scene::processNode(const aiScene* scene, const aiNode* node, const FLOAT4X4& transform)
{
    FLOAT4X4 localTransform = transform * aiToGlm(node->mTransformation);
    nodeTransforms[node->mName.C_Str()] = localTransform; // store for light traversal

    for (uint32_t i = 0; i < node->mNumMeshes; i++)
    {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        pendingMeshes.push_back({ mesh, localTransform, node->mName.C_Str() });
    }

    for (uint32_t i = 0; i < node->mNumChildren; i++)
    {
        processNode(scene, node->mChildren[i], localTransform);
    }
}

void Scene::processMesh(const aiScene* scene, const aiMesh* currMesh, const FLOAT4X4& localTransform)
{
    auto mesh = std::make_unique<NPMesh>();
    mesh->objectToWorld = localTransform;
    
    // get vertices
    mesh->vertices.reserve(currMesh->mNumVertices);
    for (uint32_t j = 0; j < currMesh->mNumVertices; j++)
    {
        NPVertex vertex;
        vertex.pos = FLOAT4(currMesh->mVertices[j].x, currMesh->mVertices[j].y, currMesh->mVertices[j].z, 1.0f);
        vertex.normal = currMesh->HasNormals() ? FLOAT4(currMesh->mNormals[j].x, currMesh->mNormals[j].y, currMesh->mNormals[j].z, 1.0f) : FLOAT4(0, 0, 0, 0);
        vertex.color = currMesh->HasVertexColors(0) ? FLOAT4(currMesh->mColors[0][j].r, currMesh->mColors[0][j].g, currMesh->mColors[0][j].b, 1.0f) : FLOAT4(1, 1, 1, 1);
        vertex.uv = currMesh->HasTextureCoords(0) ? FLOAT2(currMesh->mTextureCoords[0][j].x, currMesh->mTextureCoords[0][j].y) : FLOAT2(0, 0);
        vertex.pad0 = FLOAT2(0, 0);
        mesh->vertices.push_back(vertex);
    }
        
    // get indices
    for (uint32_t j = 0; j < currMesh->mNumFaces; j++)
    {
        const aiFace* face = &currMesh->mFaces[j];
            
        for (uint32_t k = 0; k < face->mNumIndices; k++)
        {
            mesh->indices.push_back(face->mIndices[k]);
        }
    }
    
    // get material
    auto mat = std::make_unique<NPMaterial>();
    
    mat->ambient = FLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    mat->diffuse = FLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
    mat->specular = FLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    mat->emission = FLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    
    const aiMaterial* aiMat = scene->mMaterials[currMesh->mMaterialIndex];
    
    aiColor3D color(0.0f, 0.0f, 0.0f);
    if (aiMat->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS)
    {
        mat->ambient = FLOAT4(color.r, color.b, color.g, 1.0f);
    }
    
    if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
    {
        mat->diffuse = FLOAT4(color.r, color.g, color.b, 1.0f);
    }
    
    if (aiMat->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS)
    {
        mat->specular = FLOAT4(color.r, color.g, color.b, 1.0f);
    }
    
    if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS)
    {
        mat->emission = FLOAT4(color.r, color.g, color.b, 1.0f);
    }
    
    // texturing
    aiString path;
    if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS)
    {
        auto it = std::find(texturePaths.begin(), texturePaths.end(), path.C_Str());
        if (it != texturePaths.end())
        {
            uint32_t texIdx = std::distance(texturePaths.begin(), it);
            mat->diffuseTextureIdx = texIdx;
        }
        else
        {
            mat->diffuseTextureIdx = static_cast<uint32_t>(texturePaths.size());
            texturePaths.push_back(path.C_Str());
        }
    }
    
    mesh->materialIndex = static_cast<uint32_t>(_materials.size());
    _materials.push_back(std::move(mat));
        
    _meshes.push_back(std::move(mesh));
}

void Scene::processLight(const aiLight* light)
{
    auto currLight = std::make_unique<NPLight>();

    std::string lightName = light->mName.C_Str();
    
    auto it = nodeTransforms.find(lightName);
    
    if (it != nodeTransforms.end())
    {
        currLight->transform = nodeTransforms[lightName];
    }
    else
    {
        currLight->transform = FLOAT4X4(1.0);
    }
    
    currLight->color = FLOAT3(light->mColorDiffuse.r, light->mColorDiffuse.g, light->mColorDiffuse.b);
    currLight->intensity = 1.0f; // TODO update this
    
    _lights.push_back(std::move(currLight));
}

void Scene::processCamera(const aiScene* scene)
{
    NPCameraRecord cameraRecord{};
    if (scene->HasCameras())
    {
        aiCamera* aiCam = scene->mCameras[0];

        aiVector3D pos = aiCam->mPosition;
        aiVector3D look = aiCam->mLookAt;
        aiVector3D up = aiCam->mUp;

        glm::vec3 eye(pos.x, pos.y, pos.z);
        glm::vec3 center = eye + glm::vec3(look.x, look.y, look.z);
        glm::vec3 upVec(up.x, up.y, up.z);
        
        cameraRecord.model = glm::mat4(1.0f);
        cameraRecord.view = glm::lookAt(eye, center, upVec);

        float aspect = aiCam->mAspect != 0.0f ? aiCam->mAspect : (2560.0f / 1440.0f);
        cameraRecord.proj = glm::perspective(aiCam->mHorizontalFOV,
                                             aspect,
                                             aiCam->mClipPlaneNear,
                                             aiCam->mClipPlaneFar);
        cameraRecord.proj[1][1] *= -1.0f;
    }
    else
    {
        cameraRecord.model = glm::mat4(1.0f);

        cameraRecord.view = glm::lookAt(
            glm::vec3(0.0f, 5.0f, -15.0f),   // eye
            glm::vec3(0.0f, 0.0f, 0.0f),   // center
            glm::vec3(0.0f, -1.0f, 0.0f)    // up
        );

        cameraRecord.proj = glm::perspective(
            glm::radians(75.0f),
            2560.0f / 1440.0f,
            0.1f,
            1000.0f
        );
        cameraRecord.proj[1][1] *= 1.0f;
    }
    _camera = cameraRecord;
}

NPMesh* Scene::addMesh()
{
    std::lock_guard<std::mutex> lock(_meshMutex);

    _meshes.push_back(std::make_unique<NPMesh>());
    return _meshes.back().get();
}

bool Scene::removeMesh(const uint32_t& objectId)
{
    std::lock_guard<std::mutex> lock(_meshMutex);

    auto it = std::find_if(_meshes.begin(), _meshes.end(),
                           [&objectId](const std::unique_ptr<NPMesh>& mesh)
                           { return mesh->objectId == objectId; });

    bool found = it != _meshes.end();
    if (found)
    {
        _meshes.erase(it);
    }
    return found;
}

NPMesh const* Scene::getMeshAtIndex(int idx) const
{
    if (idx < 0 || idx >= _meshes.size())
    {
        return nullptr;
    }

    return _meshes[idx].get();
}

NPLight const* Scene::getLightAtIndex(int idx) const
{
    if (idx < 0 || idx >= _lights.size())
    {
        return nullptr;
    }

    return _lights[idx].get();
}

NPMaterial const* Scene::getMaterialAtIndex(int idx) const
{
    if (idx < 0 || idx >= _materials.size())
    {
        return nullptr;
    }
    
    return _materials[idx].get();
}
