#pragma once

#include "usdPlugin/library/usdMath.h"

#include <NPTracerRenderer/structs.h>

#include <pxr/imaging/hd/mesh.h>
#include <pxr/imaging/hd/meshUtil.h>

#include <functional>

PXR_NAMESPACE_OPEN_SCOPE

class NPTracerHdRenderDelegate;  // forward declare

enum class PrimvarType : uint8_t
{
    POSITION,
    NORMAL,
    COLOR,
    UV,
    _COUNT
};

struct PrimvarPayload
{
    HdPrimvarDescriptor desc{};

    VtValue original{};
    VtValue processed{};

    bool isDirty = true;
};

using IsPrimvarFn = bool (*)(const HdPrimvarDescriptor&);
using UpdatePrimvarFn = std::function<void(const HdMeshUtil&, PrimvarPayload&)>;

class NPTracerHdMesh : public HdMesh
{
public:
    NPTracerHdMesh(const SdfPath& rprimId, NPTracerHdRenderDelegate* renderDelegate);
    ~NPTracerHdMesh() override;

    HdDirtyBits GetInitialDirtyBitsMask() const override;

    void Sync(HdSceneDelegate* delegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits,
              const TfToken& reprToken) override;
    bool SyncPrimvars(HdSceneDelegate* delegate, HdDirtyBits* dirtyBits);

    void UpdateMeshPrimvarMap(HdSceneDelegate* delegate);

    static void sConstructMesh(const VtU32Array& triIndices,
                               const std::unordered_map<PrimvarType, PrimvarPayload>& primvarMap,
                               np::Mesh* outMesh);

    static constexpr std::array<HdInterpolation, 1> SUPPORTED_INTERPOLATIONS = {
        HdInterpolationFaceVarying
    };

private:
    HdDirtyBits _PropagateDirtyBits(HdDirtyBits bits) const override;
    void _InitRepr(const TfToken& reprToken, HdDirtyBits* dirtyBits) override;

    NPTracerHdRenderDelegate* _pCreator;
    np::Mesh* _pMesh = nullptr;

    // flattened data of trianglulated indices outputted from `HdMeshUtil::ComputeTriangleIndices`
    VtU32Array _triIndices;

    std::unordered_map<PrimvarType, PrimvarPayload> _primvarMap;

    void _AddToScene();
    void _RemoveFromScene();

    // manual maps
    static UpdatePrimvarFn sMapUpdatePrimvarFn(HdInterpolation interpolation);

    static bool sIsPositionPrimvar(const HdPrimvarDescriptor& desc);
    static bool sIsNormalPrimvar(const HdPrimvarDescriptor& desc);
    static bool sIsColorPrimvar(const HdPrimvarDescriptor& desc);
    static bool sIsUVPrimvar(const HdPrimvarDescriptor& desc);

    // this should follow the order of `PrimvarType`
    static constexpr IsPrimvarFn IS_PRIMVAR_FN_TABLE[] = { &sIsPositionPrimvar, &sIsNormalPrimvar,
                                                           &sIsColorPrimvar, &sIsUVPrimvar };

    static void sUpdateFaceVaryingPrimvar(const HdMeshUtil& meshUtil,
                                          PrimvarPayload& payloadToUpdate);
};

PXR_NAMESPACE_CLOSE_SCOPE
