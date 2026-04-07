#include "usdPlugin/primitives/NPTracerHdMesh.h"

#include "NPTracerHdLight.h"
#include "usdPlugin/debugCodes.h"
#include "usdPlugin/NPTracerHdRenderDelegate.h"

#include <pxr/imaging/hd/vtBufferSource.h>
#include <pxr/base/gf/rotation.h>

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
        GfMatrix4f transform = GfMatrix4f(delegate->GetTransform(id));

        if constexpr (!np::gDEBUG)  // TEMP: toggle based on scene. renderer assumes z-up?
        {
            static const GfMatrix4f rotator = GfMatrix4f().SetRotate(
                GfRotation(GfVec3f(1, 0, 0), 90.f));
            transform = rotator * transform;
        }

        _pMesh->transform = GfToGLMMat4f(transform);
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

    // indices
    const bool bHasDirtyTopology = *dirtyBits & HdChangeTracker::DirtyTopology;
    if (bHasDirtyTopology)
    {
        // use Hydra utilities to retrieve triangulated indices
        const HdMeshTopology& topo = delegate->GetMeshTopology(id);
        const HdMeshUtil meshUtil(&topo, id);
        VtVec3iArray tris;
        VtIntArray primitiveParams;
        meshUtil.ComputeTriangleIndices(&tris, &primitiveParams);  // get triangulated indices

        VtFlattenVec3iArray(tris, &_triIndices);  // make a flat array of indices
    }

    // primvars
    const bool bHasDirtyPrimvars = *dirtyBits & HdChangeTracker::DirtyPrimvar;
    if (bHasDirtyPrimvars) SyncPrimvars(delegate, dirtyBits);

    if (bHasDirtyTopology || bHasDirtyPrimvars) sConstructMesh(_triIndices, _primvarMap, _pMesh);

    *dirtyBits &= ~HdChangeTracker::AllSceneDirtyBits;  // set all bits to clean!
}

void NPTracerHdMesh::SyncPrimvars(HdSceneDelegate* delegate, const HdDirtyBits* dirtyBits)
{
    const SdfPath& id = GetId();

    // build a flat vector of all the descriptors
    HdPrimvarDescriptorVector allDescriptors;
    for (uint8_t i = 0; i < HdInterpolationCount; ++i)
    {
        const HdInterpolation& interpolation = static_cast<HdInterpolation>(i);
        const HdPrimvarDescriptorVector& descs = delegate->GetPrimvarDescriptors(id, interpolation);
        allDescriptors.reserve(allDescriptors.size() + descs.size());
        allDescriptors.insert(allDescriptors.end(), descs.begin(), descs.end());
    }

    // initialize some function-scoped data structures
    std::unordered_map<HdInterpolation, std::vector<PrimvarPayloadBase*>> primvarInterpolationMap;
    primvarInterpolationMap.reserve(HdInterpolationCount);

    const size_t indicesCount = _triIndices.size();
    for (auto& [type, payload] : _primvarMap)
    {
        const IsPrimvarDirtyFn& kIsDirtyFn = IS_PRIMVAR_DIRTY_FN_TABLE[type];
        if (kIsDirtyFn(dirtyBits, id))
        {
            payload->isDirty = true;

            const IsPrimvarDescFn& kIsDescFn = IS_PRIMVAR_DESC_FN_TABLE[static_cast<uint8_t>(type)];
            auto it = std::ranges::find_if(allDescriptors, kIsDescFn);
            if (it == allDescriptors.end())
            {
                payload->FillDefault(indicesCount);  // make safe by filling both with default value
                continue;  // skip processing, primvar does not exist
            }

            // fill in payload with found descriptor
            const HdPrimvarDescriptor& desc = it[0];

            // use the name to query the value
            VtValue pv = delegate->Get(id, desc.name);
            payload->SetSource(pv);
            payload->desc = desc;
            primvarInterpolationMap[desc.interpolation].push_back(payload.get());
        }
    }

    // process all primvars according to their interpolation
    const HdMeshTopology& topo = delegate->GetMeshTopology(id);
    const HdMeshUtil meshUtil(&topo, id);
    for (auto& [interpolation, pPayloads] : primvarInterpolationMap)
    {
        const ProcessPrimvarsFn& kProcessFn
            = PROCESS_PRIMVARS_FN_TABLE[static_cast<uint32_t>(interpolation)];
        kProcessFn(meshUtil, _triIndices, pPayloads);
    }
}

void NPTracerHdMesh::sConstructMesh(
    const VtUIntArray& triIndices,
    const std::unordered_map<PrimvarType, UPTR<PrimvarPayloadBase>>& primvarMap, np::Mesh* outMesh)
{
    // expected count for all attrs
    const uint32_t count = static_cast<uint32_t>(triIndices.size());

    // resize all to desired size
    outMesh->indices.resize(count);
    outMesh->vertices.resize(count);

    const VtVec3fArray& positions = GetPayload<GfVec3f>(primvarMap, POSITION)->GetProcessedArray();
    const VtVec3fArray& normals = GetPayload<GfVec3f>(primvarMap, NORMAL)->GetProcessedArray();
    const VtVec3fArray& colors = GetPayload<GfVec3f>(primvarMap, COLOR)->GetProcessedArray();
    const VtVec2fArray& UVs = GetPayload<GfVec2f>(primvarMap, UV)->GetProcessedArray();

    // fill in all vertex data
    for (uint32_t i = 0u; i < count; ++i)
    {
        outMesh->indices[i] = i;  // indices will now just increment linearly
        // simple linear indexing for primvars as well now that they have been preprocessed
        outMesh->vertices[i] = {
            .pos = FLOAT4(GfToGLMVec3f(positions[i]), 1.f),
            .normal = FLOAT4(GfToGLMVec3f(normals[i]), 1.f),
            .color = FLOAT4(GfToGLMVec3f(colors[i]), 1.f),
            .uv = GfToGLMVec2f(UVs[i]),
        };
    }
}

HdDirtyBits NPTracerHdMesh::_PropagateDirtyBits(HdDirtyBits bits) const
{
    return bits;
}

void NPTracerHdMesh::_InitRepr(const TfToken& reprToken, HdDirtyBits*)
{
    auto it = std::ranges::find_if(_reprs.begin(), _reprs.end(), _ReprComparator(reprToken));
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

    // reset and reserve space for the map
    _primvarMap.clear();
    _primvarMap.reserve(PrimvarType::_COUNT);
    // set default value for each
    _primvarMap[PrimvarType::POSITION] = std::make_unique<PrimvarPayload<GfVec3f>>(GfVec3f(0.f));
    _primvarMap[PrimvarType::NORMAL] = std::make_unique<PrimvarPayload<GfVec3f>>(GfVec3f(0.f));
    _primvarMap[PrimvarType::COLOR] = std::make_unique<PrimvarPayload<GfVec3f>>(GfVec3f(1.f));
    _primvarMap[PrimvarType::UV] = std::make_unique<PrimvarPayload<GfVec2f>>(GfVec2f(0.f));
}

void NPTracerHdMesh::_RemoveFromScene()
{
    np::Scene* scene = _pCreator->GetScene();
    if (scene && _pMesh)
    {
        bool removed = scene->deletePrim<np::Mesh>(_pMesh);
        NP_DBG("Removed mesh '%s' from scene: %d\n", GetId().GetText(), removed);
        _pMesh = nullptr;
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
