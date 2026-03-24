#include "usd_plugin/NPTracerHdRenderDelegate.h"

#include "usd_plugin/NPTracerDebugCodes.h"
#include "usd_plugin/NPTracerHdRenderPass.h"
#include "usd_plugin/NPTracerHdRenderBuffer.h"

#include <pxr/imaging/hd/rprim.h>
#include <pxr/imaging/hd/sprim.h>
#include <pxr/imaging/hd/bprim.h>
#include <pxr/imaging/hd/camera.h>

PXR_NAMESPACE_OPEN_SCOPE

const TfTokenVector NPTracerHdRenderDelegate::SUPPORTED_RPRIM_TYPES = {};  // renderable primitives
const TfTokenVector NPTracerHdRenderDelegate::SUPPORTED_SPRIM_TYPES = {
    // state prims
    HdPrimTypeTokens->camera
};
const TfTokenVector NPTracerHdRenderDelegate::SUPPORTED_BPRIM_TYPES = {
    // buffer prims
    HdPrimTypeTokens->renderBuffer
};

NPTracerHdRenderDelegate::NPTracerHdRenderDelegate()
{
    _Initialize();
}

NPTracerHdRenderDelegate::NPTracerHdRenderDelegate(const HdRenderSettingsMap& settingsMap)
    : HdRenderDelegate(settingsMap)
{
    _Initialize();
}

NPTracerHdRenderDelegate::~NPTracerHdRenderDelegate()
{
    _renderParam.reset();
};

HdRenderPassSharedPtr NPTracerHdRenderDelegate::CreateRenderPass(HdRenderIndex* index,
                                                                 HdRprimCollection const& collection)
{
    return HdRenderPassSharedPtr(new NPTracerHdRenderPass(index, collection, this));
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
    return HdRenderDelegate::GetRenderParam();
}

HdRenderSettingDescriptorList NPTracerHdRenderDelegate::GetRenderSettingDescriptors() const
{
    return _settingDescriptors;
}

HdResourceRegistrySharedPtr NPTracerHdRenderDelegate::GetResourceRegistry() const
{
    return _resourceRegistry;
}

HdInstancer* NPTracerHdRenderDelegate::CreateInstancer(HdSceneDelegate* delegate, const SdfPath& id)
{
    TF_CODING_ERROR("Creating instancer not currently supported: id=%s", id.GetText());
    return nullptr;
}

void NPTracerHdRenderDelegate::DestroyInstancer(HdInstancer* instancer)
{
    TF_CODING_ERROR("Destroying instancer not currently supported.");
}

HdRprim* NPTracerHdRenderDelegate::CreateRprim(const TfToken& typeId, const SdfPath& rprimId)
{
    TF_DEBUG(NPTRACER_RENDER)
        .Msg("[%s] Create Rprim type: type=%s id=%s\n", TF_FUNC_NAME().c_str(), typeId.GetText(),
             rprimId.GetText());

    if (true)
    {
        TF_CODING_ERROR("Unknown Rprim: type=%s id=%s", typeId.GetText(), rprimId.GetText());
    }
    return nullptr;
}

void NPTracerHdRenderDelegate::DestroyRprim(HdRprim* rprim)
{
    TF_DEBUG(NPTRACER_RENDER)
        .Msg("[%s] Destroy Rprim: id=%s\n", TF_FUNC_NAME().c_str(), rprim->GetId().GetText());
    delete rprim;
}

HdSprim* NPTracerHdRenderDelegate::CreateSprim(const TfToken& typeId, const SdfPath& sprimId)
{
    TF_DEBUG(NPTRACER_RENDER)
        .Msg("[%s] Create Sprim: type=%s id=%s\n", TF_FUNC_NAME().c_str(), typeId.GetText(),
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
    TF_DEBUG(NPTRACER_RENDER)
        .Msg("[%s] Destroy Sprim: id=%s\n", TF_FUNC_NAME().c_str(), sprim->GetId().GetText());
    delete sprim;
}

HdSprim* NPTracerHdRenderDelegate::CreateFallbackSprim(const TfToken& typeId)
{
    TF_DEBUG(NPTRACER_RENDER)
        .Msg("[%s] Create Fallback Sprim: type=%s\n", TF_FUNC_NAME().c_str(), typeId.GetText());

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
    TF_DEBUG(NPTRACER_RENDER)
        .Msg("[%s] Create Bprim: type=%s id=%s\n", TF_FUNC_NAME().c_str(), typeId.GetText(),
             bprimId.GetText());

    if (typeId == HdPrimTypeTokens->renderBuffer)
    {
        return new NPTracerHdRenderBuffer(bprimId);
    }
    else
    {
        TF_CODING_ERROR("Unknown Bprim: type=%s id=%s", typeId.GetText(), bprimId.GetText());
    }
    return nullptr;
}

void NPTracerHdRenderDelegate::DestroyBprim(HdBprim* bprim)
{
    TF_DEBUG(NPTRACER_RENDER)
        .Msg("[%s] Destroy Bprim: id=%s\n", TF_FUNC_NAME().c_str(), bprim->GetId().GetText());

    delete bprim;
}

HdBprim* NPTracerHdRenderDelegate::CreateFallbackBprim(const TfToken& typeId)
{
    TF_DEBUG(NPTRACER_RENDER)
        .Msg("[%s] Create Fallback Bprim: type=%s\n", TF_FUNC_NAME().c_str(), typeId.GetText());

    if (typeId == HdPrimTypeTokens->renderBuffer)
    {
        return new NPTracerHdRenderBuffer(SdfPath::EmptyPath());
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
        return HdAovDescriptor(HdFormatUNorm8Vec4, true, VtValue(GfVec4f(0.0f)));
    }
    else if (aovName == HdAovTokens->depth)
    {
        return HdAovDescriptor(HdFormatFloat32, false, VtValue(1.0f));
    }
    else if (aovName == HdAovTokens->primId || aovName == HdAovTokens->instanceId
             || aovName == HdAovTokens->elementId)
    {
        return HdAovDescriptor(HdFormatInt32, false, VtValue(-1));
    }

    return HdAovDescriptor();
}

void NPTracerHdRenderDelegate::_Initialize()
{
    _renderParam = std::unique_ptr<NPTracerHdRenderParam>(new NPTracerHdRenderParam());
    _resourceRegistry = std::make_shared<HdResourceRegistry>();
}

PXR_NAMESPACE_CLOSE_SCOPE
