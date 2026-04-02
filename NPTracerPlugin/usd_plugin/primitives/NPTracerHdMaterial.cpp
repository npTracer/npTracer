#include "usd_plugin/primitives/NPTracerHdMaterial.h"
#include "usd_plugin/NPTracerHdRenderDelegate.h"

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
    return HdMaterial::DirtyParams;
}

// TODO: make a base case class for all hd prim types with add and remove
void NPTracerHdMaterial::_AddToScene()
{
    if (Scene* scene = _pCreator->GetScene())
    {
        _pMaterial = scene->makePrim<NPMaterial>();

        _materialIndex = static_cast<uint32_t>(scene->getPrimCount<NPMaterial>() - 1);
    }
}

void NPTracerHdMaterial::_RemoveFromScene()
{
    Scene* scene = _pCreator->GetScene();
    if (scene && _pMaterial)
    {
        scene->deletePrim<NPMaterial>(_pMaterial);
        _pMaterial = nullptr;
    }
}

void NPTracerHdMaterial::Sync(HdSceneDelegate* delegate, HdRenderParam*, HdDirtyBits* dirtyBits)
{
    const SdfPath& id = GetId();

    HdMaterialNetworkMap networkMap = delegate->GetMaterialResource(id).Get<HdMaterialNetworkMap>();

    for (const auto& entry : networkMap.map)
    {
        for (const auto& node : entry.second.nodes)
        {
            const auto& params = node.parameters;

            if (params.count(TfToken("diffuseColor")))
            {
                auto c = params.at(TfToken("diffuseColor")).Get<GfVec3f>();

                _pMaterial->diffuse = FLOAT4(c[0], c[1], c[2], 1.0f);
            }

            if (params.count(TfToken("emissiveColor")))
            {
                auto e = params.at(TfToken("emissiveColor")).Get<GfVec3f>();

                _pMaterial->emission = FLOAT4(e[0], e[1], e[2], 1.0f);
            }
        }
    }

    *dirtyBits = Clean;
}

PXR_NAMESPACE_CLOSE_SCOPE
