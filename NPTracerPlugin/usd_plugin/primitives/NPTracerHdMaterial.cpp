#include "usd_plugin/primitives/NPTracerHdMaterial.h"

#include "stb_image.h"
#include "usd_plugin/NPTracerHdRenderDelegate.h"
#include "usd_plugin/debugCodes.h"

#include <pxr/imaging/hd/sceneDelegate.h>

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
        _pMaterial->objectId = id.GetHash();
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

    // has similar API to `std::unordered_map`? if not just a wrapper, I suspect
    HdMaterialNetworkMap networkMap = delegate->GetMaterialResource(id).Get<HdMaterialNetworkMap>();

    if (!scene || !_pMaterial || !networkMap.map.count(TfToken("surface"))) return;

    const HdMaterialNetwork& net = networkMap.map.at(TfToken("surface"));

    const HdMaterialNode* previewNode = nullptr;
    const HdMaterialNode* textureNode = nullptr;
    for (const auto& node : net.nodes)
    {
        if (node.identifier == TfToken("UsdPreviewSurface"))
        {
            previewNode = &node;
            NP_DBG("Found `USDPreviewSurface` at scene path '%s'\n", previewNode->path.GetText());
        }
        if (node.identifier == TfToken("UsdUVTexture"))
        {
            textureNode = &node;
        }
        NP_DBG("'%s'\n", node.identifier.GetText());
    }

    if (previewNode)
    {
        const auto& params = previewNode->parameters;
        if (params.count(TfToken("diffuseColor")))
        {
            auto c = params.at(TfToken("diffuseColor")).Get<GfVec3f>();
            _pMaterial->diffuse = FLOAT4(c[0], c[1], c[2], 1.0f);

            NP_DBG("Found uniform diffuse color '(%f, %f, %f)'.\n", c[0], c[1], c[2]);
            const VtValue& val = params.at(TfToken("diffuseColor"));
        }

        if (params.count(TfToken("emissiveColor")))
        {
            auto e = params.at(TfToken("emissiveColor")).Get<GfVec3f>();

            _pMaterial->emission = FLOAT4(e[0], e[1], e[2], 1.0f);

            NP_DBG("Found uniform emission color '(%f, %f, %f)'.\n", e[0], e[1], e[2]);
        }
    }

    if (textureNode)
    {
        const auto& params = textureNode->parameters;
        if (params.count(TfToken("file")))
        {
            auto asset = params.at(TfToken("file")).Get<SdfAssetPath>();

            std::string path = asset.GetResolvedPath();

            NP_DBG("Found texture at scene path '%s'.\n", path.c_str());

            // create texture
            // TODO: pull this into another static function
            np::NPTexture* tex = scene->makePrim<np::NPTexture>();

            int w, h, c;
            unsigned char* data = stbi_load(path.c_str(), &w, &h, &c, 4);

            tex->pixels = data;
            tex->width = w;
            tex->height = h;

            uint32_t texIdx = scene->getPrimCount<np::NPTexture>() - 1;

            _pMaterial->diffuseTextureIdx = texIdx;
        }
    }

    *dirtyBits = Clean;
}

PXR_NAMESPACE_CLOSE_SCOPE
