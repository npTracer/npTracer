#pragma once

#include "usd_plugin/NPTracerHdRenderParam.h"

#include <pxr/imaging/hd/renderDelegate.h>

#include <memory>
#include <pxr/imaging/hgi/hgi.h>

PXR_NAMESPACE_OPEN_SCOPE

class NPTracerHdRenderDelegate : public HdRenderDelegate
{
public:
    NPTracerHdRenderDelegate();

    NPTracerHdRenderDelegate(const HdRenderSettingsMap& settingsMap);

    ~NPTracerHdRenderDelegate() override;

    HdRenderPassSharedPtr CreateRenderPass(HdRenderIndex* index,
                                           HdRprimCollection const& collection) override;

    // query supported hydra prim types
    const TfTokenVector& GetSupportedRprimTypes() const override;
    const TfTokenVector& GetSupportedSprimTypes() const override;
    const TfTokenVector& GetSupportedBprimTypes() const override;

    // return this delegate's render param, which provides top-level scene state
    virtual HdRenderParam* GetRenderParam() const override;

    // returns a list of user-configurable render settings, available in the UI
    virtual HdRenderSettingDescriptorList GetRenderSettingDescriptors() const override;

    /// get the resource registry
    virtual HdResourceRegistrySharedPtr GetResourceRegistry() const override;

    // create an instancer
    virtual HdInstancer* CreateInstancer(HdSceneDelegate* delegate, const SdfPath& id) override;

    /// destroy an instancer.
    virtual void DestroyInstancer(HdInstancer* instancer) override;

    /// create and destroy Rprim
    virtual HdRprim* CreateRprim(const TfToken& typeId, const SdfPath& rprimId) override;
    virtual void DestroyRprim(HdRprim* rprim) override;

    // create, destroy, and create fallback Sprim
    virtual HdSprim* CreateSprim(const TfToken& typeId, const SdfPath& sprimId) override;
    virtual void DestroySprim(HdSprim* sprim) override;
    virtual HdSprim* CreateFallbackSprim(const TfToken& typeId) override;

    /// create, destroy, and create fallback Bprim
    virtual HdBprim* CreateBprim(const TfToken& typeId, const SdfPath& bprimId) override;
    virtual void DestroyBprim(HdBprim* bprim) override;
    virtual HdBprim* CreateFallbackBprim(const TfToken& typeId) override;

    // do work here?
    virtual void CommitResources(HdChangeTracker* tracker) override;

    // return the AOV description for `aovName`. This will be used to initialize the aov buffers.
    virtual HdAovDescriptor GetDefaultAovDescriptor(const TfToken& aovName) const override;

private:
    void _Initialize();

    static const TfTokenVector SUPPORTED_RPRIM_TYPES;
    static const TfTokenVector SUPPORTED_SPRIM_TYPES;
    static const TfTokenVector SUPPORTED_BPRIM_TYPES;

    HdRenderSettingDescriptorList _settingDescriptors;
    std::unique_ptr<NPTracerHdRenderParam> _renderParam;
    HdResourceRegistrySharedPtr _resourceRegistry;
    HgiUniquePtr _hgi;
};

PXR_NAMESPACE_CLOSE_SCOPE

