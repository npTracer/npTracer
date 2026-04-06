#pragma once

#include <NPTracerRenderer/structs.h>

#include <pxr/imaging/hd/mesh.h>
#include <pxr/imaging/hd/meshUtil.h>

#include <functional>

PXR_NAMESPACE_OPEN_SCOPE

class NPTracerHdRenderDelegate;  // forward declare

enum PrimvarType : uint8_t
{
    POSITION,
    NORMAL,
    COLOR,
    UV,
    _COUNT
};

struct PrimvarPayload
{
    TfToken name{};
    HdInterpolation interpolation = HdInterpolationCount;  // sentient unimplemented va

    VtValue original{};
    VtValue processed{};

    bool isDirty = true;
};

using IsPrimvarDescPredicateFn = std::function<bool(const HdPrimvarDescriptor&)>;
using UpdatePrimvarFn = std::function<void(const HdMeshUtil&, PrimvarPayload&)>;

class NPTracerHdMesh : public HdMesh
{
public:
    NPTracerHdMesh(const SdfPath& rprimId, NPTracerHdRenderDelegate* renderDelegate);
    ~NPTracerHdMesh() override;

    HdDirtyBits GetInitialDirtyBitsMask() const override;

    void Sync(HdSceneDelegate* delegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits,
              const TfToken& reprToken) override;

    void UpdateMeshPrimvarMap(HdSceneDelegate* delegate);

    static void sConstructMesh(const VtArray<uint32_t>& triIndices,
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
    VtArray<uint32_t> _triIndices;

    std::unordered_map<PrimvarType, PrimvarPayload> _primvarMap;

    void _AddToScene();
    void _RemoveFromScene();

    // manual maps
    static IsPrimvarDescPredicateFn sMapIsPrimvarDescPredicateFn(PrimvarType type);
    static UpdatePrimvarFn sMapUpdatePrimvarFn(HdInterpolation interpolation);

    static bool sIsNormalsPrimvarDescriptor(const HdPrimvarDescriptor& desc);
    static bool sIsColorsPrimvarDescriptor(const HdPrimvarDescriptor& desc);
    static bool sIsUVPrimvarDescriptor(const HdPrimvarDescriptor& desc);

    static void sUpdateFaceVaryingPrimvar(const HdMeshUtil& meshUtil,
                                          PrimvarPayload& payloadToUpdate);
};

PXR_NAMESPACE_CLOSE_SCOPE
