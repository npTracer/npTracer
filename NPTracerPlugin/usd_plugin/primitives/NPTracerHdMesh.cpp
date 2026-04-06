#include "usd_plugin/primitives/NPTracerHdMesh.h"

#include "usd_plugin/debugCodes.h"
#include "usd_plugin/hdMathUtils.h"
#include "usd_plugin/NPTracerHdRenderDelegate.h"

#include <iostream>
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
        // use Hydra utilities to retrieve triangulated indices
        const HdMeshTopology& topo = delegate->GetMeshTopology(id);
        const HdMeshUtil meshUtil(&topo, id);
        VtArray<GfVec3i> tris;
        VtIntArray primitiveParams;
        meshUtil.ComputeTriangleIndices(&tris, &primitiveParams);  // get triangulated indices

        const size_t targetIndicesCount = tris.size() * 3llu;
        const size_t indicesByteSize = sizeof(uint32_t) * targetIndicesCount;
        const GfVec3i* trisData = tris.data();

        TF_DEV_AXIOM(sizeof(trisData) == indicesByteSize);  // ensure this memcpy will succeed

        _triIndices.resize(targetIndicesCount);
        std::memcpy(_triIndices.data(), trisData, indicesByteSize);

        bNeedsReconstruction = true;
    }

    // positions
    if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->points))
    {
        PrimvarPayload& payload = _primvarMap[PrimvarType::POSITION];
        payload.original = delegate->Get(id, HdTokens->points);
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

    if (bNeedsReconstruction) sConstructMesh(_triIndices, _primvarMap, _pMesh);

    *dirtyBits &= ~HdChangeTracker::AllSceneDirtyBits;  // set all bits to clean!
}

void NPTracerHdMesh::UpdateMeshPrimvarMap(HdSceneDelegate* delegate)
{
    const SdfPath& id = GetId();
    const HdMeshTopology& topo = delegate->GetMeshTopology(id);
    const HdMeshUtil meshUtil(&topo, id);
    const size_t targetIndicesCount = _triIndices.size();

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

        if (payload.original.IsEmpty())
        {
            const IsPrimvarDescPredicateFn& pred = sMapIsPrimvarDescPredicateFn(type);
            auto it = std::ranges::find_if(allDescriptors, pred);
            if (it != allDescriptors.end())
            {
                // fill in payload with found descriptor
                const HdPrimvarDescriptor& desc = it[0];
                payload.name = desc.name;
                payload.interpolation = desc.interpolation;

                // use the name to query the value
                payload.original = delegate->Get(id, payload.name);
            }
        }
        const UpdatePrimvarFn& updater = sMapUpdatePrimvarFn(payload.interpolation);
        updater(meshUtil, payload);
        TF_DEV_AXIOM(payload.original.GetArraySize() == targetIndicesCount);
        payload.isDirty = false;
    }
}

void NPTracerHdMesh::sConstructMesh(
    const VtUIntArray& triIndices,
    const std::unordered_map<PrimvarType, PrimvarPayload>& primvarMap, np::Mesh* outMesh)
{
    // expected count for all attrs
    const uint32_t count = static_cast<uint32_t>(triIndices.size());

    // resize all to desired size
    outMesh->indices.resize(count);
    outMesh->vertices.resize(count);

    const VtVec3fArray& positions = primvarMap.at(PrimvarType::POSITION).original.Get<VtVec3fArray>();
    const VtVec3fArray& normals = primvarMap.at(PrimvarType::NORMAL).original.Get<VtVec3fArray>();
    const VtVec3fArray& colors = primvarMap.at(PrimvarType::COLOR).original.Get<VtVec3fArray>();
    const VtVec2fArray& uvs = primvarMap.at(PrimvarType::UV).original.Get<VtVec2fArray>();

    const uint32_t maxAllowedIdx = positions.size();  // indices should not go out of bounds

    // copy indices over to mesh
    std::memcpy(outMesh->indices.data(), triIndices.data(), count * sizeof(uint32_t));

    // fill in all vertex data
    for (uint32_t i = 0u; i < count; ++i)
    {
        uint32_t idx = outMesh->indices[i];
        idx = std::min<uint32_t>(idx, maxAllowedIdx);

        outMesh->vertices[idx] = {
            .pos = FLOAT4(GfToGLMVec3f(positions[idx]), 1.f),
            .normal = FLOAT4(GfToGLMVec3f(normals[idx]), 1.f),
            .color = FLOAT4(GfToGLMVec3f(colors[idx]), 1.f),
            .uv = GfToGLMVec2f(uvs[idx]),
        };
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
    const HdVtBufferSource buffer(payloadToUpdate.name, payloadToUpdate.original);

    // try to triangulate the primvars
    bool bCanResolveType
        = meshUtil.ComputeTriangulatedFaceVaryingPrimvar(buffer.GetData(),
                                                         static_cast<int>(buffer.GetNumElements()),
                                                         buffer.GetTupleType().type,
                                                         &payloadToUpdate.processed);
    TF_DEV_AXIOM(bCanResolveType);  // if this fails later, we can explictly pass in type
}

PXR_NAMESPACE_CLOSE_SCOPE
