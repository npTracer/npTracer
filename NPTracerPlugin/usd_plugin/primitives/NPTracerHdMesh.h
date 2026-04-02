#pragma once

#include <NPTracerRenderer/structs.h>

#include <pxr/imaging/hd/mesh.h>
#include <pxr/imaging/hd/meshUtil.h>

#include <functional>

PXR_NAMESPACE_OPEN_SCOPE

class NPTracerHdRenderDelegate;  // forward declare

class NPTracerHdMesh : public HdMesh
{
public:
    NPTracerHdMesh(const SdfPath& rprimId, NPTracerHdRenderDelegate* renderDelegate);
    ~NPTracerHdMesh() override;

    HdDirtyBits GetInitialDirtyBitsMask() const override;

    void Sync(HdSceneDelegate* delegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits,
              const TfToken& reprToken) override;

    bool IsDirty(const HdDirtyBits* dirtyBits) const;

    static VtValue sGetPrimvar(const SdfPath& id, HdSceneDelegate* delegate, const TfToken& name);

    static bool sIsUVPrimvarDescriptor(const std::string& primvarName);
    static bool sIsNormalsPrimvarDescriptor(const std::string& primvarName);

    static void sConstructMesh(const SdfPath& id, HdSceneDelegate* delegate, NPMesh* outMesh);

    static bool sReadMeshPrimvars(const SdfPath& id, HdSceneDelegate* delegate,
                                  const HdMeshUtil& meshUtil, VtValue* pvValueOut,
                                  const std::function<bool(const std::string&)>& pred);

protected:
    HdDirtyBits _PropagateDirtyBits(HdDirtyBits bits) const override;
    void _InitRepr(const TfToken& reprToken, HdDirtyBits* dirtyBits) override;

private:
    NPTracerHdRenderDelegate* _pCreator;
    NPMesh* _pMesh = nullptr;

    void _UpdateInScene(HdSceneDelegate* delegate);
    void _AddToScene();
    void _RemoveFromScene();
};

PXR_NAMESPACE_CLOSE_SCOPE
