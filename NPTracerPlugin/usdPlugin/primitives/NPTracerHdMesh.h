#pragma once

#include "usdPlugin/library/usdMath.h"
#include "usdPlugin/library/usdPrimvar.h"

#include <NPTracerRenderer/structs.h>

#include <pxr/imaging/hd/mesh.h>
#include <pxr/imaging/hd/meshUtil.h>

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
    void SyncPrimvars(HdSceneDelegate* delegate, const HdDirtyBits* dirtyBits);

    static void sConstructMesh(
        const VtU32Array& triIndices,
        const std::unordered_map<PrimvarType, UPTR<PrimvarPayloadBase>>& primvarMap,
        np::Mesh* outMesh);

private:
    HdDirtyBits _PropagateDirtyBits(HdDirtyBits bits) const override;
    void _InitRepr(const TfToken& reprToken, HdDirtyBits* dirtyBits) override;

    NPTracerHdRenderDelegate* _pCreator;
    np::Mesh* _pMesh = nullptr;

    // flattened data of trianglulated indices outputted from `HdMeshUtil::ComputeTriangleIndices`
    VtU32Array _triIndices;
    VtIntArray _primitiveParams;

    std::unordered_map<PrimvarType, UPTR<PrimvarPayloadBase>> _primvarMap{};

    void _AddToScene();
    void _RemoveFromScene();
};

PXR_NAMESPACE_CLOSE_SCOPE
