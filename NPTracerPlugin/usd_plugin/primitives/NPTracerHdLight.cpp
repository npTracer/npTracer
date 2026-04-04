#include "usd_plugin/primitives/NPTracerHdLight.h"

#include "usd_plugin/NPTracerHdRenderDelegate.h"
#include "usd_plugin/debugCodes.h"
#include "usd_plugin/hdMathUtils.h"

#include "pxr/imaging/hd/sceneDelegate.h"

PXR_NAMESPACE_OPEN_SCOPE

NPTracerHdLight::NPTracerHdLight(const SdfPath& sprimId, NPTracerHdRenderDelegate* renderDelegate)
    : HdLight(sprimId), _pCreator(renderDelegate)
{
}

NPTracerHdLight::~NPTracerHdLight()
{
    _RemoveFromScene();
}

HdDirtyBits NPTracerHdLight::GetInitialDirtyBitsMask() const
{
    return HdLight::DirtyParams;
}

// TODO: make a base case class for all hd prim types with add and remove
void NPTracerHdLight::_AddToScene()
{
    if (np::Scene* scene = _pCreator->GetScene())
    {
        const SdfPath& id = GetId();
        _pLight = scene->makePrim<np::Light>();

        _PrepareLight();

        NP_DBG("Added light '%s' to scene\n", id.GetAsString().c_str());
    }
}

void NPTracerHdLight::_RemoveFromScene()
{
    np::Scene* scene = _pCreator->GetScene();
    if (scene && _pLight)
    {
        bool removed = scene->deletePrim<np::Light>(_pLight);
        _pLight = nullptr;

        NP_DBG("Removed light '%s' from scene: %d\n", GetId().GetAsString().c_str(), removed);
    }
}

// below is all sphere light specific

NPTracerHdSphereLight::NPTracerHdSphereLight(const SdfPath& sprimId,
                                             NPTracerHdRenderDelegate* renderDelegate)
    : NPTracerHdLight(sprimId, renderDelegate)
{
    _AddToScene();
}

void NPTracerHdSphereLight::Sync(HdSceneDelegate* delegate, HdRenderParam* renderParam,
                                 HdDirtyBits* dirtyBits)
{
    const SdfPath& id = GetId();

    GfMatrix4f lightTransform(delegate->GetTransform(id));
    _pLight->transform = GfMatrix4fToGLM(lightTransform);

    bool i = delegate->GetLightParamValue(id, HdLightTokens->intensity).IsEmpty();
    bool c = delegate->GetLightParamValue(id, HdLightTokens->color).IsEmpty();

    _pLight->intensity = (delegate->GetLightParamValue(id, HdLightTokens->intensity)).Get<float>();

    GfVec3f lightColor = (delegate->GetLightParamValue(id, HdLightTokens->color)).Get<pxr::GfVec3f>();
    _pLight->color = GfVec3ToGLM(lightColor);

    *dirtyBits = Clean;
}

void NPTracerHdSphereLight::_PrepareLight()
{
    DEV_ASSERT(_pLight, "Light should exist before preparation");
    _pLight->type = np::LightType::POINT;
}

PXR_NAMESPACE_CLOSE_SCOPE
