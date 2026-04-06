#pragma once

#include "usd_plugin/NPTracerHdRenderParam.h"

#include <pxr/imaging/hd/renderDelegate.h>

#include <NPTracerRenderer/app.h>

#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

class NPTracerHdRenderDelegate : public HdRenderDelegate
{
public:
    NPTracerHdRenderDelegate();

    NPTracerHdRenderDelegate(const HdRenderSettingsMap& settingsMap);

    ~NPTracerHdRenderDelegate() override;

    HdRenderPassSharedPtr CreateRenderPass(HdRenderIndex* index,
                                           const HdRprimCollection& collection) override;

    // query supported hydra prim types
    const TfTokenVector& GetSupportedRprimTypes() const override;  // renderable primitives
    const TfTokenVector& GetSupportedSprimTypes() const override;  // state prims
    const TfTokenVector& GetSupportedBprimTypes() const override;  // buffer prims;

    const TfTokenVector SUPPORTED_RPRIM_TYPES = { HdPrimTypeTokens->mesh };
    const TfTokenVector SUPPORTED_SPRIM_TYPES = { HdPrimTypeTokens->camera,
                                                  HdPrimTypeTokens->material,
                                                  HdPrimTypeTokens->sphereLight };
    const TfTokenVector SUPPORTED_BPRIM_TYPES = { HdPrimTypeTokens->renderBuffer };
    const TfTokenVector NO_SUPPORTED_PRIM_TYPES = {};

    static constexpr np::RendererConstants RENDERER_CONSTANTS = {
        .executionMode = np::eExecutionMode::OFFSCREEN,
#if ASSIMP_OVERRIDE
        .sceneType = np::eSceneType::ASSIMP,
#else
        .sceneType = np::eSceneType::DEFAULT,
#endif
    };

    // return this delegate's render param, which provides top-level scene state
    HdRenderParam* GetRenderParam() const override;

    // returns a list of user-configurable render settings, available in the UI
    HdRenderSettingDescriptorList GetRenderSettingDescriptors() const override;

    /// get the resource registry
    HdResourceRegistrySharedPtr GetResourceRegistry() const override;

    // create an instancer
    HdInstancer* CreateInstancer(HdSceneDelegate* delegate, const SdfPath& id) override;

    /// destroy an instancer.
    void DestroyInstancer(HdInstancer* instancer) override;

    /// create and destroy Rprim
    HdRprim* CreateRprim(const TfToken& typeId, const SdfPath& rprimId) override;
    void DestroyRprim(HdRprim* rprim) override;

    // create, destroy, and create fallback Sprim
    HdSprim* CreateSprim(const TfToken& typeId, const SdfPath& sprimId) override;
    void DestroySprim(HdSprim* sprim) override;
    HdSprim* CreateFallbackSprim(const TfToken& typeId) override;

    /// create, destroy, and create fallback Bprim
    HdBprim* CreateBprim(const TfToken& typeId, const SdfPath& bprimId) override;
    void DestroyBprim(HdBprim* bprim) override;
    HdBprim* CreateFallbackBprim(const TfToken& typeId) override;

    // do work here?
    void CommitResources(HdChangeTracker* tracker) override;

    // return the AOV description for `aovName`. This will be used to initialize the aov buffers.
    HdAovDescriptor GetDefaultAovDescriptor(const TfToken& aovName) const override;

    np::App* GetApp() const
    {
        return _pApp.get();
    }

    np::Scene* GetScene() const
    {
        return _pApp != nullptr ? _pApp->getScene() : nullptr;
    }

private:
    void _Initialize();

    static constexpr bool _bOverrideSceneWithAssimp = ASSIMP_OVERRIDE;
    static constexpr char _kAssimpOverrideFilePath[512] = ASSIMP_OVERRIDE_FILE_PATH;

    std::unique_ptr<np::App> _pApp;  // `App` lasts delegate's lifetime

    HdRenderSettingDescriptorList _settingDescriptors;
    std::unique_ptr<NPTracerHdRenderParam> _pRenderParam;
    HdResourceRegistrySharedPtr _pResourceRegistry;
};

PXR_NAMESPACE_CLOSE_SCOPE
