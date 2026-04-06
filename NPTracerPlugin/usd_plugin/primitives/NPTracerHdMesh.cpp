#include "usd_plugin/primitives/NPTracerHdMesh.h"

#include "usd_plugin/debugCodes.h"
#include "usd_plugin/hdMathUtils.h"
#include "usd_plugin/NPTracerHdRenderDelegate.h"

#include <pxr/base/gf/vec2f.h>
#include <pxr/imaging/hd/vtBufferSource.h>

PXR_NAMESPACE_OPEN_SCOPE

NPTracerHdMesh::NPTracerHdMesh(const SdfPath& rprimId, NPTracerHdRenderDelegate* renderDelegate)
    : HdMesh(rprimId), _pCreator(renderDelegate)
{
    _AddToScene();
}

NPTracerHdMesh::~NPTracerHdMesh()
{
    _RemoveFromScene();
}

HdDirtyBits NPTracerHdMesh::GetInitialDirtyBitsMask() const
{
    return HdChangeTracker::AllSceneDirtyBits;  // all bits start dirty
}

void NPTracerHdMesh::Sync(HdSceneDelegate* delegate, HdRenderParam* renderParam,
                          HdDirtyBits* dirtyBits, const TfToken& reprToken)
{
    const SdfPath& id = GetId();

    // transform
    if (*dirtyBits & HdChangeTracker::DirtyTransform)
    {
        // retrieve the transform first (it only gets more complex from here)
        FLOAT4x4 xform = GfToGLMMat4f(delegate->GetTransform(id));
        if constexpr (np::gDEBUG)  // TEMP: renderer assumes z-up?
        {
            static FLOAT4x4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(+90.0f),
                                                   glm::vec3(1.0f, 0.0f, 0.0f));
            xform = rotation * xform;
        }

        _pMesh->transform = xform;
    }

    // material
    // check if material exists / needs to be updated in scene
    if (*dirtyBits & HdChangeTracker::DirtyMaterialId)
    {
        np::ScenePath currMaterialScenePath = delegate->GetMaterialId(id).GetString();
        if (!currMaterialScenePath.empty() && (currMaterialScenePath != _pMesh->_materialScenePath))
        {
            _pMesh->_materialScenePath = currMaterialScenePath;
            _pMesh->bMaterialNeedsFinalization = true;
        }
    }

    bool bNeedsReconstruction = false;
    bool bNeedsPrimvarsUpdate = false;

    // indices
    if (*dirtyBits & HdChangeTracker::DirtyTopology)
    {
        _trisArray.clear();
        // use Hydra utilities to retrieve triangulated indices
        const HdMeshTopology& topo = delegate->GetMeshTopology(id);
        const HdMeshUtil meshUtil(&topo, id);
        VtIntArray primitiveParams;
        meshUtil.ComputeTriangleIndices(&_trisArray, &primitiveParams);  // get triangulated indices
        _tris = VtValue(_trisArray);  // TODO: check is this update necessary?
        bNeedsReconstruction = true;
    }

    // positions
    if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->points))
    {
        PrimvarPayload& payload = _primvarMap[PrimvarType::POSITION];
        payload.primvar = delegate->Get(id, HdTokens->points);
        payload.isDirty = true;
        bNeedsReconstruction = true;
    }

    // normals
    if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->normals))
    {
        _primvarMap[PrimvarType::NORMAL] = {};  // reset map entry
        bNeedsReconstruction = true;
        bNeedsPrimvarsUpdate = true;
    }

    // colors
    if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->displayColor))
    {
        _primvarMap[PrimvarType::COLOR] = {};  // reset map entry
        bNeedsReconstruction = true;
        bNeedsPrimvarsUpdate = true;
    }

    // UVs
    if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, TfToken("st"))  // UVs
        || HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, TfToken("map1")))  // Maya UVs
    {
        _primvarMap[PrimvarType::UV] = {};  // reset map entry
        bNeedsReconstruction = true;
        bNeedsPrimvarsUpdate = true;
    }

    if (bNeedsPrimvarsUpdate) UpdateMeshPrimvarMap(delegate);

    if (bNeedsReconstruction) sConstructMesh(id, delegate, _tris, _primvarMap, _pMesh);

    *dirtyBits &= ~HdChangeTracker::AllSceneDirtyBits;  // set all bits to clean!
}

void NPTracerHdMesh::UpdateMeshPrimvarMap(HdSceneDelegate* delegate)
{
    const SdfPath& id = GetId();
    const HdMeshTopology& topo = delegate->GetMeshTopology(id);
    const HdMeshUtil meshUtil(&topo, id);
    const size_t faceVaryingCount = _tris.GetArraySize() * 3;

    // build a flat vector of all the descriptors
    HdPrimvarDescriptorVector allDescriptors;
    for (uint8_t i = 0; i < HdInterpolationCount; ++i)
    {
        const HdInterpolation& interpolation = static_cast<HdInterpolation>(i);
        const HdPrimvarDescriptorVector& descs = delegate->GetPrimvarDescriptors(id, interpolation);
        allDescriptors.reserve(allDescriptors.size() + descs.size());
        allDescriptors.insert(allDescriptors.end(), descs.begin(), descs.end());
    }

    for (uint8_t i = 0; i < PrimvarType::_COUNT; ++i)
    {
        PrimvarType type = static_cast<PrimvarType>(i);
        PrimvarPayload& payload = _primvarMap[type];
        if (!payload.isDirty) continue;  // nothing else needed for this primvar

        if (payload.primvar.IsEmpty())
        {
            const IsPrimvarDescPredicateFn& pred = sMapIsPrimvarDescPredicateFn(type);
            auto it = std::ranges::find_if(allDescriptors, pred);
            if (it != allDescriptors.end())
            {
                payload.desc = it[0];  // fill in payload with found descriptor
                payload.primvar = delegate->Get(id,
                                                payload.desc.name);  // use the name to get the value
            }
        }
        const UpdatePrimvarFn& updater = sMapUpdatePrimvarFn(payload.desc.interpolation);
        updater(meshUtil, payload);
        TF_DEV_AXIOM(payload.primvar.GetArraySize() == faceVaryingCount);
        payload.isDirty = false;
    }
}

void NPTracerHdMesh::sConstructMesh(
    const SdfPath& id, HdSceneDelegate* delegate, const VtValue& tris,
    const std::unordered_map<PrimvarType, PrimvarPayload>& primvarMap, np::Mesh* outMesh)
{
    const VtVec3iArray& trisArray = tris.Get<VtVec3iArray>();
    const size_t faceVaryingCount = trisArray.size() * 3;  // expected count for face-varying attrs

    // resize all to desired size
    outMesh->indices.resize(faceVaryingCount);
    outMesh->vertices.resize(faceVaryingCount);

    const VtVec3fArray& positions = primvarMap.at(PrimvarType::POSITION).primvar.Get<VtVec3fArray>();
    const VtVec3fArray& normals = primvarMap.at(PrimvarType::NORMAL).primvar.Get<VtVec3fArray>();
    const VtVec3fArray& colors = primvarMap.at(PrimvarType::COLOR).primvar.Get<VtVec3fArray>();
    const VtVec2fArray& uvs = primvarMap.at(PrimvarType::UV).primvar.Get<VtVec2fArray>();

    // fill in all vertex data
    uint32_t flatIdx = 0u;
    for (size_t i = 0u; i < faceVaryingCount; ++i)
    {
        const GfVec3i& tri = GfVec3i(1);

        const uint32_t t0 = tri[0];
        const uint32_t t1 = tri[1];
        const uint32_t t2 = tri[2];

        const uint32_t flatIdx0 = flatIdx;
        const uint32_t flatIdx1 = flatIdx + 1;
        const uint32_t flatIdx2 = flatIdx + 2;

        outMesh->indices[flatIdx0] = t0;
        outMesh->indices[flatIdx1] = t1;
        outMesh->indices[flatIdx2] = t2;

        outMesh->vertices[flatIdx0] = {
            .pos = FLOAT4(GfToGLM<GfVec3f, FLOAT3>(positions[flatIdx0]), 1.f),
            .normal = FLOAT4(GfToGLM<GfVec3f, FLOAT3>(normals[flatIdx0]), 1.f),
            .color = FLOAT4(GfToGLM<GfVec3f, FLOAT3>(colors[flatIdx0]), 1.f),
            .uv = GfToGLM<GfVec2f, FLOAT2>(uvs[flatIdx0]),
        };
        outMesh->vertices[flatIdx1] = {
            .pos = FLOAT4(GfToGLM<GfVec3f, FLOAT3>(positions[flatIdx1]), 1.f),
            .normal = FLOAT4(GfToGLM<GfVec3f, FLOAT3>(normals[flatIdx1]), 1.f),
            .color = FLOAT4(GfToGLM<GfVec3f, FLOAT3>(colors[flatIdx1]), 1.f),
            .uv = GfToGLM<GfVec2f, FLOAT2>(uvs[flatIdx1]),
        };
        outMesh->vertices[flatIdx2] = {
            .pos = FLOAT4(GfToGLM<GfVec3f, FLOAT3>(positions[flatIdx2]), 1.f),
            .normal = FLOAT4(GfToGLM<GfVec3f, FLOAT3>(normals[flatIdx2]), 1.f),
            .color = FLOAT4(GfToGLM<GfVec3f, FLOAT3>(colors[flatIdx2]), 1.f),
            .uv = GfToGLM<GfVec2f, FLOAT2>(uvs[flatIdx2]),
        };

        flatIdx += 3;
    }
}

HdDirtyBits NPTracerHdMesh::_PropagateDirtyBits(HdDirtyBits bits) const
{
    return bits;
}

void NPTracerHdMesh::_InitRepr(const TfToken& reprToken, HdDirtyBits*)
{
    auto it = std::find_if(_reprs.begin(), _reprs.end(), _ReprComparator(reprToken));
    if (it == _reprs.end())
    {
        _reprs.emplace_back(reprToken, HdReprSharedPtr());
    }
}

void NPTracerHdMesh::_AddToScene()
{
    if (np::Scene* scene = _pCreator->GetScene())
    {
        const SdfPath& id = GetId();
        _pMesh = scene->makePrim<np::Mesh>();
        _pMesh->scenePath = id.GetString();

        NP_DBG("Added mesh '%s' to scene\n", id.GetAsString().c_str());
    }
}

void NPTracerHdMesh::_RemoveFromScene()
{
    np::Scene* scene = _pCreator->GetScene();
    if (scene && _pMesh)
    {
        bool removed = scene->deletePrim<np::Mesh>(_pMesh);
        _pMesh = nullptr;

        NP_DBG("Removed mesh '%s' from scene: %d\n", GetId().GetAsString().c_str(), removed);
    }
}

IsPrimvarDescPredicateFn NPTracerHdMesh::sMapIsPrimvarDescPredicateFn(PrimvarType type)
{
    switch (type)
    {
        case PrimvarType::NORMAL: return sIsNormalsPrimvarDescriptor;
        case PrimvarType::COLOR: return sIsColorsPrimvarDescriptor;
        case PrimvarType::UV: return sIsUVPrimvarDescriptor;
        default: UNREACHABLE_CODE;
    }
}

UpdatePrimvarFn NPTracerHdMesh::sMapUpdatePrimvarFn(HdInterpolation interpolation)
{
    switch (interpolation)
    {
        case HdInterpolationFaceVarying: return sUpdateFaceVaryingPrimvar;
        default: UNREACHABLE_CODE;
    }
}

bool NPTracerHdMesh::sIsUVPrimvarDescriptor(const HdPrimvarDescriptor& desc)
{
    const std::string& name = desc.name;
    // primvar would be called `st` or `map1` when exported from Maya
    return name.compare("st") == 0 || name.compare("map1") == 0;
}

bool NPTracerHdMesh::sIsNormalsPrimvarDescriptor(const HdPrimvarDescriptor& desc)
{
    const std::string& name = desc.name;
    return name == HdTokens->normals;
}

bool NPTracerHdMesh::sIsColorsPrimvarDescriptor(const HdPrimvarDescriptor& desc)
{
    const std::string& name = desc.name;
    return name == HdTokens->displayColor;
}

void NPTracerHdMesh::sUpdateFaceVaryingPrimvar(const HdMeshUtil& meshUtil,
                                               PrimvarPayload& payloadToUpdate)
{
    // reverse type-erasure. assign the primvar data to a named buffer
    const HdVtBufferSource buffer(payloadToUpdate.desc.name, payloadToUpdate.primvar);

    // try to triangulate the primvars
    bool bCanResolveType
        = meshUtil.ComputeTriangulatedFaceVaryingPrimvar(buffer.GetData(),
                                                         static_cast<int>(buffer.GetNumElements()),
                                                         buffer.GetTupleType().type,
                                                         &payloadToUpdate.primvar);
    TF_DEV_AXIOM(bCanResolveType);  // if this fails later, we can explictly pass in type
}

PXR_NAMESPACE_CLOSE_SCOPE
