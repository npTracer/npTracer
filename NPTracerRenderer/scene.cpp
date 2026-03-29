#include "scene.h"

Scene::Scene() {}

Scene::~Scene() {}

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
    
    _meshes.reserve(scene->mNumMeshes);
    for (uint32_t i = 0; i < scene->mNumMeshes; i++)
    {
        const aiMesh* currMesh = scene->mMeshes[i];
        
        auto mesh = std::make_unique<NPMesh>();
        
        // get vertices
        mesh->vertices.reserve(currMesh->mNumVertices);
        for (uint32_t j = 0; j < currMesh->mNumVertices; j++)
        {
            NPVertex vertex;
            vertex.pos = FLOAT3(currMesh->mVertices[j].x, currMesh->mVertices[j].y, currMesh->mVertices[j].z);
            vertex.normal = currMesh->HasNormals() ? FLOAT3(currMesh->mNormals[j].x, currMesh->mNormals[j].y, currMesh->mNormals[j].z) : FLOAT3(0, 0, 0);
            vertex.color = currMesh->HasVertexColors(0) ? FLOAT3(currMesh->mColors[0][j].r, currMesh->mColors[0][j].g, currMesh->mColors[0][j].b) : FLOAT3(0, 0, 0);
            vertex.uv = currMesh->HasTextureCoords(0) ? FLOAT2(currMesh->mTextureCoords[0][j].x, currMesh->mTextureCoords[0][j].y) : FLOAT2(0, 0);
            
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
        
    // TODO REPLACE
    NPCameraRecord cameraRecord;
    cameraRecord.model = rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
    cameraRecord.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
               glm::vec3(0.0f, 0.0f, 1.0f)),
    cameraRecord.proj = glm::perspective(glm::radians(45.0f),
                         static_cast<float>(2560)
                             / static_cast<float>(1440),
                         0.1f, 10.0f);
    
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
