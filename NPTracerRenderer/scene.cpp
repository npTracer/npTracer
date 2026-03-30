#include "scene.h"

Scene::Scene() {}

Scene::~Scene() {}

static FLOAT4X4 aiToGlm(const aiMatrix4x4& m)
{
    return glm::mat4(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4
    );
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
    
    processNode(scene, root, FLOAT4X4(1.0));
    processCamera(scene);
}

void Scene::processNode(const aiScene* scene, const aiNode* node, const FLOAT4X4& parentTransform)
{
    FLOAT4X4 localTransform = parentTransform * aiToGlm(node->mTransformation);
    
    for (uint32_t i = 0; i < node->mNumMeshes; i++)
    {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        processMesh(mesh, localTransform);
    }
    
    for (uint32_t i = 0; i < node->mNumChildren; i++)
    {
        const aiNode* child = node->mChildren[i];
        processNode(scene, child, localTransform);
    }
}

void Scene::processMesh(const aiMesh* currMesh, const FLOAT4X4& localTransform)
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
        
    _meshes.push_back(std::move(mesh));
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
            glm::vec3(0.0f, 50.0f, 100.0f),   // eye
            glm::vec3(0.0f, 50.0f, 0.0f),   // center
            glm::vec3(0.0f, 1.0f, 0.0f)    // up
        );

        cameraRecord.proj = glm::perspective(
            glm::radians(75.0f),
            2560.0f / 1440.0f,
            0.1f,
            1000.0f
        );
        cameraRecord.proj[1][1] *= -1.0f;
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
