#include "usd_plugin/primitives/NPTracerHdMaterial.h"

#include "usd_plugin/NPTracerHdRenderDelegate.h"
#include "usd_plugin/debugCodes.h"

#include <pxr/imaging/hd/sceneDelegate.h>

#include "stb_image.h"

PXR_NAMESPACE_OPEN_SCOPE

NPTracerHdMaterial::NPTracerHdMaterial(const SdfPath& id, NPTracerHdRenderDelegate* renderDelegate)
    : HdMaterial(id), _pCreator(renderDelegate)
{
    _AddToScene();
}

NPTracerHdMaterial::~NPTracerHdMaterial()
{
    _RemoveFromScene();
}

HdDirtyBits NPTracerHdMaterial::GetInitialDirtyBitsMask() const
{
    return HdChangeTracker::AllSceneDirtyBits;
}

// TODO: make a base case class for all hd prim types with add and remove
void NPTracerHdMaterial::_AddToScene()
{
    if (np::Scene* scene = _pCreator->GetScene())
    {
        const SdfPath& id = GetId();
        _pMaterial = scene->makePrim<np::Material>();
        _pMaterial->scenePath = id.GetString();
    }
}

void NPTracerHdMaterial::_RemoveFromScene()
{
    np::Scene* scene = _pCreator->GetScene();
    if (scene && _pMaterial)
    {
        scene->deletePrim<np::Material>(_pMaterial);
        _pMaterial = nullptr;
    }
}

void NPTracerHdMaterial::Sync(HdSceneDelegate* delegate, HdRenderParam*, HdDirtyBits* dirtyBits)
{
    const SdfPath& id = GetId();
    np::Scene* scene = _pCreator->GetScene();

    // is a wrapper of sorts around `unordered_map`
    HdMaterialNetworkMap networkMap = delegate->GetMaterialResource(id).Get<HdMaterialNetworkMap>();

    if (!scene || !_pMaterial || !networkMap.map.count(TfToken("surface"))) return;

    const HdMaterialNetwork& net = networkMap.map.at(TfToken("surface"));

    const HdMaterialNode* previewNode = nullptr;

    for (const auto& node : net.nodes)
    {
        if (node.identifier == TfToken("UsdPreviewSurface"))
        {
            previewNode = &node;
            NP_DBG("Found `USDPreviewSurface` at scene path '%s'\n", previewNode->path.GetText());
        }
    }

    if (!previewNode) return;  // for now, there is nothing else we can do

    const auto& params = previewNode->parameters;
    if (params.count(TfToken("diffuseColor")))
    {
        auto c = params.at(TfToken("diffuseColor")).Get<GfVec3f>();
        _pMaterial->diffuse = FLOAT4(c[0], c[1], c[2], 1.0f);

        NP_DBG("Found uniform diffuse color '(%f, %f, %f)'.\n", c[0], c[1], c[2]);
    }

    const SdfPath& previewNodePath = previewNode->path;
    const HdMaterialNode* textureNode = nullptr;
    for (const auto& rel : net.relationships)
    {
        if (rel.outputId == previewNodePath && rel.outputName == TfToken("diffuseColor"))
        {
            for (const auto& node : net.nodes)
            {
                if (node.path == rel.inputId && node.identifier == TfToken("UsdUVTexture"))
                {
                    textureNode = &node;
                    NP_DBG("Found `UsdUVTexture` at scene path '%s'\n", previewNode->path.GetText());
                }
            }
        }
    }

    // try to create a texture in the scene as well
    if (textureNode)
    {
        const auto& params = textureNode->parameters;
        if (params.count(TfToken("file")))
        {
            auto asset = params.at(TfToken("file")).Get<SdfAssetPath>();

            std::string texPath = asset.GetResolvedPath();

            int width, height, channels;
            unsigned char* pixels = stbi_load(texPath.c_str(), &width, &height, &channels,
                                              STBI_rgb_alpha);

            if (pixels)
            {
                np::Texture* tex = scene->makePrim<np::Texture>();

                tex->pixels = pixels;
                tex->width = width;
                tex->height = height;

                uint32_t texIdx = scene->getPrimCount<np::Texture>() - 1;
                _pMaterial->diffuseTextureIndex = texIdx;

                NP_DBG("Successfully loaded a texture at file path '%s' for '%s' material.\n",
                       texPath.c_str(), id.GetText());
            }
            else
            {
                NP_DBG(
                    "Found a texture at file path '%s' for '%s' material but failed to load it.\n",
                    texPath.c_str(), id.GetText());
            }
        }
    }

    *dirtyBits = DirtyBits::Clean;  // mark as clean
}

PXR_NAMESPACE_CLOSE_SCOPE
