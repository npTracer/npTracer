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
    NPTracerHdMesh(SdfPath const& rprimId, NPTracerHdRenderDelegate* renderDelegate);
    ~NPTracerHdMesh();

    HdDirtyBits GetInitialDirtyBitsMask() const override;

    void Sync(HdSceneDelegate* delegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits,
              TfToken const& reprToken) override;

    bool IsDirty(HdDirtyBits const* dirtyBits) const;

    static void sConstructMesh(SdfPath const& id, HdSceneDelegate* delegate, NPMesh* outMesh);
    static bool sReadMeshPrimvars(SdfPath const& id, HdSceneDelegate* delegate,
                                  const HdMeshUtil& meshUtil, VtValue* pvValueOut,
                                  const std::function<bool(const std::string&)>& pred);

    static VtValue sGetPrimvar(SdfPath const& id, HdSceneDelegate* delegate, const TfToken& name);

protected:
    HdDirtyBits _PropagateDirtyBits(HdDirtyBits bits) const override;
    void _InitRepr(TfToken const& reprToken, HdDirtyBits* dirtyBits) override;

private:
    NPTracerHdRenderDelegate* _pCreator;
    std::unique_ptr<NPMesh> _pMesh;

    void _UpdateInScene(HdSceneDelegate* delegate);
    void _AddToScene();
    void _RemoveFromScene();
};

PXR_NAMESPACE_CLOSE_SCOPE
