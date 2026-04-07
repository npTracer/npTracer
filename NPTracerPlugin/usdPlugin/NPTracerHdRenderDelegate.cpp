#include "usdPlugin/NPTracerHdRenderDelegate.h"

#include "usdPlugin/debugCodes.h"
#include "usdPlugin/NPTracerHdRenderPass.h"
#include "usdPlugin/NPTracerHdRenderBuffer.h"

#include "usdPlugin/primitives/NPTracerHdMesh.h"
#include "usdPlugin/primitives/NPTracerHdLight.h"
#include "usdPlugin/primitives/NPTracerHdMaterial.h"

#include <pxr/imaging/hd/rprim.h>
#include <pxr/imaging/hd/sprim.h>
#include <pxr/imaging/hd/bprim.h>
#include <pxr/imaging/hd/camera.h>
#include <pxr/imaging/hdSt/light.h>

PXR_NAMESPACE_OPEN_SCOPE

#define VECTOR_FROM_C_ARRAY(T, arr) std::vector<T>(std::begin(arr), std::end(arr))

NPTracerHdRenderDelegate::NPTracerHdRenderDelegate()
{
    _Initialize();
}

NPTracerHdRenderDelegate::NPTracerHdRenderDelegate(const HdRenderSettingsMap& settingsMap)
    : HdRenderDelegate(settingsMap)  // TODO: use the settings map ourselves
{
    _Initialize();
}

NPTracerHdRenderDelegate::~NPTracerHdRenderDelegate()
{
    _pApp->destroy();
    _pRenderParam.reset();
    _pResourceRegistry.reset();
    _settingDescriptors.clear();
    _settingsMap.clear();
};

HdRenderPassSharedPtr NPTracerHdRenderDelegate::CreateRenderPass(HdRenderIndex* index,
                                                                 const HdRprimCollection& collection)
{
    return std::make_shared<NPTracerHdRenderPass>(index, collection, this);
}

const TfTokenVector& NPTracerHdRenderDelegate::GetSupportedRprimTypes() const
{
    return _bOverrideSceneWithAssimp ? NO_SUPPORTED_PRIM_TYPES : SUPPORTED_RPRIM_TYPES;
}

const TfTokenVector& NPTracerHdRenderDelegate::GetSupportedSprimTypes() const
{
    return _bOverrideSceneWithAssimp ? NO_SUPPORTED_PRIM_TYPES : SUPPORTED_SPRIM_TYPES;
}

const TfTokenVector& NPTracerHdRenderDelegate::GetSupportedBprimTypes() const
{
    return SUPPORTED_BPRIM_TYPES;
}

HdRenderParam* NPTracerHdRenderDelegate::GetRenderParam() const
{
    return _pRenderParam.get();
}

HdRenderSettingDescriptorList NPTracerHdRenderDelegate::GetRenderSettingDescriptors() const
{
    return _settingDescriptors;
}

HdResourceRegistrySharedPtr NPTracerHdRenderDelegate::GetResourceRegistry() const
{
    return _pResourceRegistry;
}

HdInstancer* NPTracerHdRenderDelegate::CreateInstancer(HdSceneDelegate* delegate, const SdfPath& id)
{
    NP_DBG("Creating instancer not currently supported: id=%s\n", id.GetText());
    return nullptr;
}

void NPTracerHdRenderDelegate::DestroyInstancer(HdInstancer* instancer)
{
    NP_DBG("Destroying instancer not currently supported.\n");
}

HdRprim* NPTracerHdRenderDelegate::CreateRprim(const TfToken& typeId, const SdfPath& rprimId)
{
    NP_DBG("Create Rprim type: type=%s id=%s\n", typeId.GetText(), rprimId.GetText());

    if (typeId == HdPrimTypeTokens->mesh)
    {
        return new NPTracerHdMesh(rprimId, this);
    }
    else
    {
        NP_DBG("Unknown Rprim: type=%s id=%s\n", typeId.GetText(), rprimId.GetText());
    }
    return nullptr;
}

void NPTracerHdRenderDelegate::DestroyRprim(HdRprim* rprim)
{
    if (!rprim) return;  // necessary since we purposefully do not initialize some prims
    NP_DBG("Destroy Rprim: id=%s\n", rprim->GetId().GetText());
    delete rprim;
}

HdSprim* NPTracerHdRenderDelegate::CreateSprim(const TfToken& typeId, const SdfPath& sprimId)
{
    NP_DBG("Create Sprim: type=%s id=%s\n", typeId.GetText(), sprimId.GetText());

    if (typeId == HdPrimTypeTokens->camera)
    {
        return new HdCamera(sprimId);
    }
    else if (typeId == HdPrimTypeTokens->material)
    {
        return new NPTracerHdMaterial(sprimId, this);
    }
    else if (typeId == HdPrimTypeTokens->sphereLight)
    {
        return new NPTracerHdSphereLight(sprimId, this);
    }
    else
    {
        TF_FATAL_CODING_ERROR("Unknown Sprim: type=%s id=%s\n", typeId.GetText(), sprimId.GetText());
    }

    return nullptr;
}

void NPTracerHdRenderDelegate::DestroySprim(HdSprim* sprim)
{
    if (!sprim) return;  // necessary since we purposefully do not initialize some prims
    NP_DBG("Destroy Sprim: id=%s\n", sprim->GetId().GetText());
    delete sprim;
}

HdSprim* NPTracerHdRenderDelegate::CreateFallbackSprim(const TfToken& typeId)
{
    NP_DBG("Create Fallback Sprim: type=%s\n", typeId.GetText());

    if (typeId == HdPrimTypeTokens->camera)
    {
        return new HdCamera(SdfPath::EmptyPath());
    }
    else if (typeId == HdPrimTypeTokens->material || typeId == HdPrimTypeTokens->sphereLight)
    {
        // do not corrupt our scene with other fallback prims as it their data is not initialized and they are also not properly deleted
        return nullptr;
    }
    else
    {
        TF_FATAL_CODING_ERROR("Unknown Sprim: type=%s\n", typeId.GetText());
    }

    return nullptr;
}

HdBprim* NPTracerHdRenderDelegate::CreateBprim(const TfToken& typeId, const SdfPath& bprimId)
{
    NP_DBG("Create Bprim: type=%s id=%s\n", typeId.GetText(), bprimId.GetText());

    if (typeId == HdPrimTypeTokens->renderBuffer)
    {
        return new NPTracerHdRenderBuffer(bprimId, _pApp->getContext());
    }
    else
    {
        TF_FATAL_CODING_ERROR("Unknown Bprim: type=%s id=%s\n", typeId.GetText(), bprimId.GetText());
    }
    return nullptr;
}

void NPTracerHdRenderDelegate::DestroyBprim(HdBprim* bprim)
{
    if (!bprim) return;  // necessary since we purposefully do not initialize some prims
    NP_DBG("Destroy Bprim: id=%s\n", bprim->GetId().GetText());
    delete bprim;
}

HdBprim* NPTracerHdRenderDelegate::CreateFallbackBprim(const TfToken& typeId)
{
    NP_DBG("Create Fallback Bprim: type=%s\n", typeId.GetText());

    if (typeId == HdPrimTypeTokens->renderBuffer)
    {
        return new NPTracerHdRenderBuffer(SdfPath::EmptyPath(), _pApp->getContext());
    }
    else
    {
        TF_FATAL_CODING_ERROR("Unknown Bprim: type=%s\n", typeId.GetText());
    }

    return nullptr;
}

void NPTracerHdRenderDelegate::CommitResources(HdChangeTracker* tracker) {}

HdAovDescriptor NPTracerHdRenderDelegate::GetDefaultAovDescriptor(const TfToken& aovName) const
{
    if (aovName == HdAovTokens->color)
    {
        return { HdFormatUNorm8Vec4, true, VtValue(GfVec4f(0.0f)) };
    }
    else if (aovName == HdAovTokens->depth)
    {
        return HdAovDescriptor(HdFormatFloat32, false, VtValue(1.0f));
    }
    return {};
}

void NPTracerHdRenderDelegate::_Initialize()
{
    _pApp = std::make_unique<np::App>();

    _pApp->create(RENDERER_CONSTANTS);  // hydra assumes bottom-left origin for NDC

    if (_bOverrideSceneWithAssimp)
    {
        _pApp->loadSceneFromPath(_kAssimpOverrideFilePath);
    }

    _pRenderParam = std::make_unique<NPTracerHdRenderParam>();
    _pResourceRegistry = std::make_shared<HdResourceRegistry>();
}

PXR_NAMESPACE_CLOSE_SCOPE
