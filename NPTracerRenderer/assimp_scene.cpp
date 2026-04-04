#include "assimp_scene.h"

#include "utils.h"
#include "assimp/postprocess.h"
#include <stb_image.h>

NP_TRACER_NAMESPACE_BEGIN

static FLOAT4X4 sAiToGLM(const aiMatrix4x4& m)
{
    auto mat = FLOAT4X4(m.a1, m.b1, m.c1, m.d1, m.a2, m.b2, m.c2, m.d2, m.a3, m.b3, m.c3, m.d3,
                        m.a4, m.b4, m.c4, m.d4);
    return mat;
}

AssimpScene::~AssimpScene()
{
    nodeTransforms.clear();
    pendingMeshes.clear();
    textureIndexByKey.clear();
}

void AssimpScene::loadSceneFromPath(const char* path)
{
    Assimp::Importer importer;
    const aiScene* scene;
    try
    {
        scene = importer.ReadFile(path, aiProcess_MakeLeftHanded | aiProcess_Triangulate
                                            | aiProcess_GlobalScale | aiProcess_GenSmoothNormals
                                            | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
    }
    catch (std::exception& e)
    {
        DEV_ASSERT(false, "failed to load scene: %s", e.what());
    }

    const aiNode* root = scene->mRootNode;

    // mark default texture in texture path vector
    auto defaultTexture = std::make_unique<TextureRecord>();
    auto* pixel = static_cast<uint32_t*>(malloc(sizeof(uint32_t)));
    defaultTexture->pixels = pixel;
    defaultTexture->width = 1;
    defaultTexture->height = 1;
    _textures.push_back(std::move(defaultTexture));

    // visit all nodes
    processAiNode(scene, root, FLOAT4X4(1.0));

    for (const auto& inst : pendingMeshes)
    {
        processAiMesh(scene, inst.mesh, inst.transform);
    }

    for (uint32_t i = 0; i < scene->mNumLights; i++)
    {
        processAiLight(scene->mLights[i]);
    }

    processAiCamera(scene);
}

void AssimpScene::processAiNode(const aiScene* scene, const aiNode* node, const FLOAT4X4& transform)
{
    FLOAT4X4 localTransform = transform * sAiToGLM(node->mTransformation);
    nodeTransforms[node->mName.C_Str()] = localTransform;  // store for light traversal

    for (uint32_t i = 0; i < node->mNumMeshes; i++)
    {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        pendingMeshes.push_back({ mesh, localTransform, node->mName.C_Str() });
    }

    for (uint32_t i = 0; i < node->mNumChildren; i++)
    {
        processAiNode(scene, node->mChildren[i], localTransform);
    }
}

void AssimpScene::processAiMesh(const aiScene* scene, const aiMesh* currMesh,
                                const FLOAT4X4& localTransform)
{
    auto mesh = std::make_unique<Mesh>();
    mesh->objectToWorld = localTransform;

    // get vertices
    mesh->vertices.reserve(currMesh->mNumVertices);
    for (uint32_t j = 0; j < currMesh->mNumVertices; j++)
    {
        Vertex vert{ .pos = FLOAT4(currMesh->mVertices[j].x, currMesh->mVertices[j].y,
                                   currMesh->mVertices[j].z, 1.0f),
                     .normal = currMesh->HasNormals()
                                   ? FLOAT4(currMesh->mNormals[j].x, currMesh->mNormals[j].y,
                                            currMesh->mNormals[j].z, 1.0f)
                                   : FLOAT4(0, 0, 0, 0),
                     .color = currMesh->HasVertexColors(0)
                                  ? FLOAT4(currMesh->mColors[0][j].r, currMesh->mColors[0][j].g,
                                           currMesh->mColors[0][j].b, 1.0f)
                                  : FLOAT4(1, 1, 1, 1),
                     .uv = currMesh->HasTextureCoords(0) ? FLOAT2(currMesh->mTextureCoords[0][j].x,
                                                                  currMesh->mTextureCoords[0][j].y)
                                                         : FLOAT2(0, 0),
                     .pad0 = FLOAT2(0, 0) };
        mesh->vertices.push_back(vert);
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
    auto mat = std::make_unique<Material>();

    const aiMaterial* aiMat = scene->mMaterials[currMesh->mMaterialIndex];

    aiColor3D color(0.0f, 0.0f, 0.0f);
    if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
    {
        mat->diffuse = FLOAT4(color.r, color.g, color.b, 1.0f);
    }

    if (aiMat->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS)
    {
        mat->ambient = FLOAT4(color.r, color.g, color.b, 1.0f);
    }

    if (aiMat->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS)
    {
        mat->specular = FLOAT4(color.r, color.g, color.b, 1.0f);
    }

    if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS)
    {
        mat->emission = FLOAT4(color.r, color.g, color.b, 1.0f);
    }

    aiString aiStr;
    if (aiMat->Get(AI_MATKEY_NAME, aiStr) == AI_SUCCESS)
    {
        mat->scenePath = std::string(aiStr.C_Str());
        mat->objectId = idHasher(mat->scenePath);
    }

    // texturing
    aiString path;
    if (aiMat->GetTexture(aiTextureType_BASE_COLOR, 0, &path) == AI_SUCCESS)
    {
        std::string key = path.C_Str();

        auto it = textureIndexByKey.find(key);
        if (it != textureIndexByKey.end())
        {
            mat->diffuseTextureIdx = it->second;
        }
        else  // create a new texture
        {
            auto texture = std::make_unique<TextureRecord>();
            uint32_t newIndex = static_cast<uint32_t>(_textures.size());
            mat->diffuseTextureIdx = newIndex;

            const aiTexture* embedded = scene->GetEmbeddedTexture(path.C_Str());

            if (embedded)
            {
                if (embedded->mHeight == 0)
                {
                    int width = 0, height = 0, channels = 0;

                    stbi_uc* decodedPixels
                        = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(embedded->pcData),
                                                static_cast<int>(embedded->mWidth), &width, &height,
                                                &channels, STBI_rgb_alpha);

                    DEV_ASSERT(decodedPixels, "failed to decode embedded texture from memory");

                    texture->pixels = decodedPixels;
                    texture->width = static_cast<uint32_t>(width);
                    texture->height = static_cast<uint32_t>(height);
                }
                else
                {
                    texture->pixels = embedded->pcData;
                    texture->width = embedded->mWidth;
                    texture->height = embedded->mHeight;
                }
            }
            else
            {
                int width = 0, height = 0, channels = 0;
                stbi_uc* pixels = stbi_load(path.C_Str(), &width, &height, &channels,
                                            STBI_rgb_alpha);  // TODO: store scene directory

                DEV_ASSERT(pixels, "failed to load external texture: '%s'\n", path.C_Str());

                texture->pixels = pixels;
                texture->width = static_cast<uint32_t>(width);
                texture->height = static_cast<uint32_t>(height);
            }

            textureIndexByKey[key] = newIndex;
            _textures.push_back(std::move(texture));
        }
    }

    mesh->materialIndex = static_cast<uint32_t>(_materials.size());
    _materials.push_back(std::move(mat));

    _meshes.push_back(std::move(mesh));
}

void AssimpScene::processAiLight(const aiLight* light)
{
    auto currLight = std::make_unique<Light>();

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

    currLight->color = FLOAT3(light->mColorDiffuse.r, light->mColorDiffuse.g,
                              light->mColorDiffuse.b);
    currLight->intensity = 1.0f;  // TODO update this

    _lights.push_back(std::move(currLight));
}

void AssimpScene::processAiCamera(const aiScene* scene)
{
    CameraRecord cameraRecord{};
    if (scene->HasCameras())
    {
        aiCamera* aiCam = scene->mCameras[0];

        auto nodeTransform = FLOAT4X4(1.0f);
        auto it = nodeTransforms.find(aiCam->mName.C_Str());
        if (it != nodeTransforms.end())
        {
            nodeTransform = it->second;
        }

        auto localEye4 = glm::vec4(aiCam->mPosition.x, aiCam->mPosition.y, aiCam->mPosition.z, 1.0f);
        auto localLook4 = glm::vec4(aiCam->mLookAt.x, aiCam->mLookAt.y, aiCam->mLookAt.z, 0.0f);
        auto localUp4 = glm::vec4(aiCam->mUp.x, -aiCam->mUp.y, aiCam->mUp.z, 0.0f);

        auto eye = glm::vec3(nodeTransform * localEye4);
        glm::vec3 look = glm::normalize(glm::vec3(nodeTransform * localLook4));
        glm::vec3 upVec = glm::normalize(glm::vec3(nodeTransform * localUp4));

        glm::vec3 center = eye + look;

        cameraRecord.view = glm::lookAt(eye, center, upVec);

        float aspect = aiCam->mAspect != 0.0f ? aiCam->mAspect : (2560.0f / 1440.0f);
        cameraRecord.proj = glm::perspective(aiCam->mHorizontalFOV, aspect, aiCam->mClipPlaneNear,
                                             aiCam->mClipPlaneFar);
        cameraRecord.proj[1][1] *= 1.0f;
    }
    else
    {
        cameraRecord.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f),  // eye
                                        glm::vec3(0.0f, 0.0f, 0.0f),  // center
                                        glm::vec3(0.0f, -1.0f, 0.0f)  // up
        );

        cameraRecord.proj = glm::perspective(glm::radians(75.0f), 2560.0f / 1440.0f, 0.1f, 1000.0f);
        cameraRecord.proj[1][1] *= 1.0f;
    }

    cameraRecord.invView = glm::inverse(cameraRecord.view);
    cameraRecord.invProj = glm::inverse(cameraRecord.proj);

    _camera = cameraRecord;
}

NP_TRACER_NAMESPACE_END
