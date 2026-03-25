#include "usd_plugin/NPTracerHdRenderDelegate.h"

#include "usd_plugin/debugCodes.h"
#include "usd_plugin/NPTracerHdRenderPass.h"
#include "usd_plugin/NPTracerHdRenderBuffer.h"
#include "usd_plugin/primitives/NPTracerHdMesh.h"

#include <pxr/imaging/hd/rprim.h>
#include <pxr/imaging/hd/sprim.h>
#include <pxr/imaging/hd/bprim.h>
#include <pxr/imaging/hd/camera.h>

PXR_NAMESPACE_OPEN_SCOPE

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
                                                                 HdRprimCollection const& collection)
{
    return std::make_shared<NPTracerHdRenderPass>(index, collection, this);
}

const TfTokenVector& NPTracerHdRenderDelegate::GetSupportedRprimTypes() const
{
    return SUPPORTED_RPRIM_TYPES;
}

const TfTokenVector& NPTracerHdRenderDelegate::GetSupportedSprimTypes() const
{
    return SUPPORTED_SPRIM_TYPES;
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
    NP_DBG("Creating instancer not currently supported: id=%s", id.GetText());
    return nullptr;
}

void NPTracerHdRenderDelegate::DestroyInstancer(HdInstancer* instancer)
{
    NP_DBG("Destroying instancer not currently supported.");
}

HdRprim* NPTracerHdRenderDelegate::CreateRprim(const TfToken& typeId, const SdfPath& rprimId)
{
    NP_DBG("[%s] Create Rprim type: type=%s id=%s\n", TF_FUNC_NAME().c_str(), typeId.GetText(),
           rprimId.GetText());

    if (typeId == HdPrimTypeTokens->mesh)
    {
        return new NPTracerHdMesh(rprimId, this);
    }
    else
    {
        NP_DBG("Unknown Rprim: type=%s id=%s", typeId.GetText(), rprimId.GetText());
    }
    return nullptr;
}

void NPTracerHdRenderDelegate::DestroyRprim(HdRprim* rprim)
{
    NP_DBG("[%s] Destroy Rprim: id=%s\n", TF_FUNC_NAME().c_str(), rprim->GetId().GetText());
    delete rprim;
}

HdSprim* NPTracerHdRenderDelegate::CreateSprim(const TfToken& typeId, const SdfPath& sprimId)
{
    NP_DBG("[%s] Create Sprim: type=%s id=%s\n", TF_FUNC_NAME().c_str(), typeId.GetText(),
           sprimId.GetText());

    if (typeId == HdPrimTypeTokens->camera)
    {
        return new HdCamera(sprimId);
    }
    else
    {
        TF_CODING_ERROR("Unknown Sprim: type=%s id=%s", typeId.GetText(), sprimId.GetText());
    }

    return nullptr;
}

void NPTracerHdRenderDelegate::DestroySprim(HdSprim* sprim)
{
    NP_DBG("[%s] Destroy Sprim: id=%s\n", TF_FUNC_NAME().c_str(), sprim->GetId().GetText());
    delete sprim;
}

HdSprim* NPTracerHdRenderDelegate::CreateFallbackSprim(const TfToken& typeId)
{
    NP_DBG("[%s] Create Fallback Sprim: type=%s\n", TF_FUNC_NAME().c_str(), typeId.GetText());

    if (typeId == HdPrimTypeTokens->camera)
    {
        return new HdCamera(SdfPath::EmptyPath());
    }
    else
    {
        TF_CODING_ERROR("Unknown Sprim: type=%s", typeId.GetText());
    }

    return nullptr;
}

HdBprim* NPTracerHdRenderDelegate::CreateBprim(const TfToken& typeId, const SdfPath& bprimId)
{
    NP_DBG("[%s] Create Bprim: type=%s id=%s\n", TF_FUNC_NAME().c_str(), typeId.GetText(),
           bprimId.GetText());

    if (typeId == HdPrimTypeTokens->renderBuffer)
    {
        return new NPTracerHdRenderBuffer(bprimId, _pApp->getContext());
    }
    else
    {
        TF_CODING_ERROR("Unknown Bprim: type=%s id=%s", typeId.GetText(), bprimId.GetText());
    }
    return nullptr;
}

void NPTracerHdRenderDelegate::DestroyBprim(HdBprim* bprim)
{
    NP_DBG("[%s] Destroy Bprim: id=%s\n", TF_FUNC_NAME().c_str(), bprim->GetId().GetText());

    delete bprim;
}

HdBprim* NPTracerHdRenderDelegate::CreateFallbackBprim(const TfToken& typeId)
{
    NP_DBG("[%s] Create Fallback Bprim: type=%s\n", TF_FUNC_NAME().c_str(), typeId.GetText());

    if (typeId == HdPrimTypeTokens->renderBuffer)
    {
        return new NPTracerHdRenderBuffer(SdfPath::EmptyPath(), _pApp->getContext());
    }
    else
    {
        TF_CODING_ERROR("Unknown Bprim: type=%s", typeId.GetText());
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
        return { HdFormatFloat32, false, VtValue(1.0f) };
    }
    else if (aovName == HdAovTokens->normal)
    {
        return { HdFormatFloat32Vec3, false, VtValue(GfVec3f(0.0f, 0.0f, 0.0f)) };
    }
    else if (aovName == HdAovTokens->primId || aovName == HdAovTokens->instanceId
             || aovName == HdAovTokens->elementId)
    {
        return { HdFormatInt32, false, VtValue(-1) };
    }

    return {};
}

void NPTracerHdRenderDelegate::_Initialize()
{
    _pApp = std::make_unique<App>();
    _pApp->create();

    _pRenderParam = std::make_unique<NPTracerHdRenderParam>();
    _pResourceRegistry = std::make_shared<HdResourceRegistry>();
}

PXR_NAMESPACE_CLOSE_SCOPE
