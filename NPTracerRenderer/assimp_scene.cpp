#include "assimp_scene.h"

#include "utils.h"
#include "assimp/postprocess.h"
#include <stb_image.h>

NP_TRACER_NAMESPACE_BEGIN

static FLOAT4x4 sAiToGLM(const aiMatrix4x4& m)
{
    auto mat = FLOAT4x4(m.a1, m.b1, m.c1, m.d1, m.a2, m.b2, m.c2, m.d2, m.a3, m.b3, m.c3, m.d3,
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
        // NOTE: assimp is right-handed (+Y=up, -Z=forward) by default, which is what we want. so just flip UVs to match Vulkan UV conventions
        scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GlobalScale
                                            | aiProcess_GenSmoothNormals | aiProcess_FlipUVs
                                            | aiProcess_CalcTangentSpace
                                            | aiProcess_JoinIdenticalVertices);
    }
    catch (std::exception& e)
    {
        DEV_ASSERT(false, "failed to load scene: %s", e.what());
    }

    const aiNode* root = scene->mRootNode;

    // visit all nodes
    processAiNode(scene, root, FLOAT4x4(1.0));

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

void AssimpScene::processAiNode(const aiScene* scene, const aiNode* node, const FLOAT4x4& transform)
{
    FLOAT4x4 localTransform = transform * sAiToGLM(node->mTransformation);
    std::string nodeName = std::string(node->mName.C_Str());
    nodeTransforms[nodeName] = localTransform;  // store for light traversal

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

void AssimpScene::loadAndSetTexture(const aiScene* scene, const aiMaterial* aiMat, aiTextureType textureType, uint32_t& targetIndex)
{
    aiString aiStr;
    
    if (aiMat->GetTexture(textureType, 0, &aiStr) == AI_SUCCESS)
    {
        std::string texKey = std::string(aiStr.C_Str());

        auto it = textureIndexByKey.find(texKey);
        if (it != textureIndexByKey.end())
        {
            targetIndex = it->second;
        }
        else  // create a new texture
        {
            auto texture = makePrim<Texture>();
            uint32_t texIdx = static_cast<uint32_t>(_textures.size() - 1);
            targetIndex = texIdx;

            const aiTexture* embedded = scene->GetEmbeddedTexture(aiStr.C_Str());

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
                stbi_uc* pixels = stbi_load(aiStr.C_Str(), &width, &height, &channels,
                                            STBI_rgb_alpha);  // TODO: store scene directory

                DEV_ASSERT(pixels, "failed to load external texture: '%s'\n", aiStr.C_Str());

                texture->pixels = pixels;
                texture->width = static_cast<uint32_t>(width);
                texture->height = static_cast<uint32_t>(height);
            }

            textureIndexByKey[texKey] = texIdx;
        }
    }
}

void AssimpScene::processAiMesh(const aiScene* scene, const aiMesh* inAiMesh,
                                const FLOAT4x4& localTransform)
{
    auto mesh = makePrim<Mesh>();
    mesh->transform = localTransform;
    mesh->scenePath = std::string(inAiMesh->mName.C_Str());

    // get vertices
    mesh->vertices.reserve(inAiMesh->mNumVertices);
    for (uint32_t j = 0; j < inAiMesh->mNumVertices; j++)
    {
        Vertex vert{ .pos = FLOAT4(inAiMesh->mVertices[j].x, inAiMesh->mVertices[j].y,
                                   inAiMesh->mVertices[j].z, 1.0f),
                     .normal = inAiMesh->HasNormals()
                                   ? FLOAT4(inAiMesh->mNormals[j].x, inAiMesh->mNormals[j].y,
                                            inAiMesh->mNormals[j].z, 1.0f)
                                   : FLOAT4(0, 0, 0, 0),
                    .tangent = inAiMesh->HasTangentsAndBitangents()
                                  ? FLOAT4(inAiMesh->mTangents[j].x, inAiMesh->mTangents[j].y,
                                           inAiMesh->mTangents[j].z, inAiMesh->mTangents[j][3])
                                  : FLOAT4(0, 0, 0, 0),
                     .color = inAiMesh->HasVertexColors(0)
                                  ? FLOAT4(inAiMesh->mColors[0][j].r, inAiMesh->mColors[0][j].g,
                                           inAiMesh->mColors[0][j].b, 1.0f)
                                  : FLOAT4(1, 1, 1, 1),
                     .uv = inAiMesh->HasTextureCoords(0) ? FLOAT2(inAiMesh->mTextureCoords[0][j].x,
                                                                  inAiMesh->mTextureCoords[0][j].y)
                                                         : FLOAT2(0, 0),
                     .pad0 = FLOAT2(0, 0) };
        mesh->vertices.push_back(vert);
    }

    // get indices
    for (uint32_t j = 0; j < inAiMesh->mNumFaces; j++)
    {
        const aiFace* face = &inAiMesh->mFaces[j];

        for (uint32_t k = 0; k < face->mNumIndices; k++)
        {
            mesh->indices.push_back(face->mIndices[k]);
        }
    }

    // get material
    auto mat = makePrim<Material>();

    const aiMaterial* aiMat = scene->mMaterials[inAiMesh->mMaterialIndex];

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
        float intensity;
        if (aiMat->Get(AI_MATKEY_SPECULAR_FACTOR, intensity) == AI_SUCCESS)
        {
            mat->specular.w = intensity;
        }
    }

    if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS)
    {
        mat->emission = FLOAT4(color.r, color.g, color.b, 1.0f);
        
        float intensity;
        if (aiMat->Get(AI_MATKEY_EMISSIVE_INTENSITY, intensity) == AI_SUCCESS)
        {
            mat->emission.w = intensity; 
        }
    }
    
    ai_real factor = 0.0f;
    if (aiMat->Get(AI_MATKEY_METALLIC_FACTOR, factor) == AI_SUCCESS)
    {
        mat->metallic.x = factor;
    }
    if (aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, factor) == AI_SUCCESS)
    {
        mat->metallic.y = factor;
    }
    
    aiString aiStr;
    if (aiMat->Get(AI_MATKEY_NAME, aiStr) == AI_SUCCESS)
    {
        mat->scenePath = std::string(aiStr.C_Str());
    }

    // texturing
    loadAndSetTexture(scene, aiMat, aiTextureType_BASE_COLOR, mat->diffuseTextureIndex); // diffuse
    loadAndSetTexture(scene, aiMat, aiTextureType_NORMALS, mat->normalTextureIndex); // normals
    loadAndSetTexture(scene, aiMat, aiTextureType_METALNESS, mat->metallicTextureIndex); // metallic
    
    mesh->materialIndex = static_cast<uint32_t>(_materials.size() - 1);
}

void AssimpScene::processAiLight(const aiLight* inAiLight)
{
    if (inAiLight->mType != aiLightSourceType::aiLightSource_POINT)
        return;  // TEMP: only support point lights

    auto* light = makePrim<Light>();

    light->scenePath = std::string(inAiLight->mName.C_Str());

    auto it = nodeTransforms.find(light->scenePath);

    if (it != nodeTransforms.end())
    {
        light->transform = it->second;
    }
    else
    {
        light->transform = FLOAT4x4(1.0);
    }
    
    FLOAT3 raw = FLOAT3(inAiLight->mColorDiffuse.r, inAiLight->mColorDiffuse.g,
                                inAiLight->mColorDiffuse.b);
    
    // Assimp will encode intensity into color
    float intensityScale = 0.01f;
    float luminanceMag = (0.2126f*raw.r + 0.7152f*raw.g + 0.0722f*raw.b);
    
    light->intensity = luminanceMag * intensityScale;
    light->color = FLOAT4(raw / glm::max(luminanceMag, 1e-5f), 1.0f);
}

void AssimpScene::processAiCamera(const aiScene* inAiScene)
{
    CameraRecord cameraRecord{};
    if (inAiScene->HasCameras())
    {
        aiCamera* aiCam = inAiScene->mCameras[0];

        auto nodeTransform = FLOAT4x4(1.0f);
        auto it = nodeTransforms.find(aiCam->mName.C_Str());
        if (it != nodeTransforms.end())
        {
            nodeTransform = it->second;
        }

        auto localEye4 = glm::vec4(aiCam->mPosition.x, aiCam->mPosition.y, aiCam->mPosition.z, 1.0f);
        auto localLook4 = glm::vec4(aiCam->mLookAt.x, aiCam->mLookAt.y, aiCam->mLookAt.z, 0.0f);
        auto localUp4 = glm::vec4(aiCam->mUp.x, aiCam->mUp.y, aiCam->mUp.z, 0.0f);

        auto eye = glm::vec3(nodeTransform * localEye4);
        glm::vec3 look = glm::normalize(glm::vec3(nodeTransform * localLook4));
        glm::vec3 upVec = glm::normalize(glm::vec3(nodeTransform * localUp4));

        glm::vec3 center = eye + look;

        // NOTE: GLM uses RH by default, but we can be explicit here
        cameraRecord.view = glm::lookAtRH(eye, center, upVec);

        float aspect = aiCam->mAspect != 0.0f ? aiCam->mAspect : (2560.0f / 1440.0f);

        // calculate fovY for `glm::perspective`
        float fovY = 2.0f * atan(tan(aiCam->mHorizontalFOV * 0.5f) / aspect);
        cameraRecord.proj = glm::perspectiveRH(fovY, aspect, aiCam->mClipPlaneNear,
                                               aiCam->mClipPlaneFar);
    }
    else
    {
        cameraRecord.view = glm::lookAtRH(glm::vec3(0.0f, 0.0f, 5.0f),  // eye
                                          glm::vec3(0.0f, 0.0f, 0.0f),  // center
                                          glm::vec3(0.0f, 1.0f, 0.0f)  // up
        );

        cameraRecord.proj = glm::perspectiveRH(glm::radians(75.0f), 2560.0f / 1440.0f, 0.1f,
                                               1000.0f);
    }

    cameraRecord.invView = glm::inverse(cameraRecord.view);
    cameraRecord.invProj = glm::inverse(cameraRecord.proj);

    _camera = cameraRecord;
}

NP_TRACER_NAMESPACE_END
