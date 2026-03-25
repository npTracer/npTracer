#include "usd_plugin/primitives/NPTracerHdMesh.h"

#include "usd_plugin/debugCodes.h"
#include "usd_plugin/hdMathUtils.h"
#include "usd_plugin/NPTracerHdRenderDelegate.h"

#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/matrix4f.h>
#include <pxr/imaging/hd/vtBufferSource.h>

PXR_NAMESPACE_OPEN_SCOPE

NPTracerHdMesh::NPTracerHdMesh(SdfPath const& rprimId, NPTracerHdRenderDelegate* renderDelegate)
    : HdMesh(rprimId), _pCreator(renderDelegate)
{
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
                          HdDirtyBits* dirtyBits, TfToken const& reprToken)
{
    const bool isMeshDirty = IsDirty(dirtyBits);
    // TODO: check if instance dirty and material dirty

    if (_pCreator->GetRendererApp() && isMeshDirty)
    {
        _UpdateInScene(delegate);
    }

    *dirtyBits &= ~HdChangeTracker::AllSceneDirtyBits;  // set all bits to clean!
}

bool NPTracerHdMesh::IsDirty(HdDirtyBits const* dirtyBits) const
{
    const SdfPath& id = GetId();

    if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->points)
        || HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->normals)
        || HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, pxr::TfToken("st"))
        || HdChangeTracker::IsTopologyDirty(*dirtyBits, id))
    {  // check vertex attributes
        return true;
    }
    else if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->displayColor)
             || (*dirtyBits & pxr::HdChangeTracker::DirtyMaterialId)
             || HdChangeTracker::IsVisibilityDirty(*dirtyBits, id))
    {  // check mesh attributes
        return true;
    }
    return false;
}

void NPTracerHdMesh::_UpdateInScene(HdSceneDelegate* delegate)
{
    const SdfPath& id = GetId();

    _RemoveFromScene();  // remove existing mesh from scene

    if (!delegate->GetVisible(GetId()))
    {
        return;  // do not add to scene if it is not visible
    }

    _AddToScene();

    sConstructMesh(id, delegate, _pMesh);
}

void NPTracerHdMesh::sConstructMesh(SdfPath const& id, HdSceneDelegate* delegate, NPMesh* outMesh)
{
    FLOAT4X4 xform = GfMatrix4dToGLM(delegate->GetTransform(id));

    outMesh->objectToWorld = xform;
    outMesh->worldToObject = glm::inverse(xform);

    HdMeshTopology topo = delegate->GetMeshTopology(id);

    HdMeshUtil meshUtil(&topo, id);
    VtVec3iArray tris;
    VtIntArray primitiveParams;
    meshUtil.ComputeTriangleIndices(&tris, &primitiveParams);  // get triangulated indices

    size_t flattenedCount = tris.size() * 3;
    bool hasUVs = false;
    bool hasFlattenedUVs = false;
    bool hasNormals = false;
    bool hasFlattenedNormals = false;

    VtVec3fArray indexedPositions = delegate->Get(id, HdTokens->points).Get<VtVec3fArray>();
    VtVec3fArray indexedNormals = delegate->Get(id, HdTokens->normals).Get<VtVec3fArray>();
    VtVec2fArray indexedUVs;

    // try to fill `indexedUVs` with per-vertex interpolated attributes
    if (VtValue uvValue = delegate->Get(id, pxr::TfToken("map1")); !uvValue.IsEmpty())
    {  // check the first way that UV's would be stored on a mesh
        indexedUVs = uvValue.Get<VtVec2fArray>();
    }
    else
    {
        VtValue stValue = delegate->Get(id, pxr::TfToken("st"));
        if (!stValue.IsEmpty())
        {
            indexedUVs = stValue.Get<VtVec2fArray>();
        }
    }

    hasUVs = !indexedUVs.empty() && (indexedUVs.size() == indexedPositions.size());
    // look for UVs in primvars as they are more ideal
    VtValue uvValue;
    if (sReadMeshPrimvars(id, delegate, meshUtil, &uvValue,
                          [](const std::string& pvName)
                          { // naming standards for UVs
                              return pvName.compare("st") == 0
                                     || pvName.substr(0, 3).compare("map") == 0;
                          }))
    {
        if (uvValue.GetArraySize() == flattenedCount)
        {
            VtVec2fArray gfUVArray = uvValue.Get<VtVec2fArray>();
            outMesh->_uvs = VtVec2fArrayToGLM(gfUVArray);

            hasFlattenedUVs = true;
        }
        else
        {
            TF_WARN("Primvar UVs were found for %s but count of %i does not match "
                    "flattened index count of %i.",
                    id.GetString().c_str(), uvValue.GetArraySize(), flattenedCount);
        }
    }

    // second condition is what is received from USD for geometry that has no normals
    hasNormals = indexedNormals.size() == indexedPositions.size()
                 && !(indexedNormals.size() == 1 && indexedNormals[0] == GfVec3f(0.f));

    // look for normals in primvars
    VtValue normalsValue;
    if (sReadMeshPrimvars(id, delegate, meshUtil, &normalsValue,
                          [](const std::string& pvName) { return pvName == HdTokens->normals; }))
    {
        if (normalsValue.GetArraySize() == flattenedCount)
        {
            VtVec3fArray vtNormalsArray = normalsValue.Get<VtVec3fArray>();
            outMesh->_normals = VtVec3fArrayToGLM(vtNormalsArray);

            hasFlattenedNormals = true;
        }
        else
        {
            TF_WARN("Primvar normals were found for %s but count of %i does not match "
                    "flattened index count of %i.",
                    id.GetString().c_str(), normalsValue.GetArraySize(), flattenedCount);
        }
    }

    // resize all to desired size
    outMesh->indices.resize(flattenedCount);
    outMesh->vertices.resize(flattenedCount);
    outMesh->_positions.resize(flattenedCount);
    outMesh->_normals.resize(flattenedCount);
    outMesh->_uvs.resize(flattenedCount);

    int maxAllowedIndex = static_cast<int>(indexedPositions.size()) - 1;

    // flatten all vertex data
    for (size_t i = 0; i < tris.size(); i++)
    {
        const GfVec3i& tri = tris[i];

        int t0 = std::min(maxAllowedIndex, tri[0]);
        int t1 = std::min(maxAllowedIndex, tri[1]);
        int t2 = std::min(maxAllowedIndex, tri[2]);

        int flatIdx0 = i * 3 + 0;
        int flatIdx1 = i * 3 + 1;
        int flatIdx2 = i * 3 + 2;

        outMesh->indices[flatIdx0] = t0;
        outMesh->indices[flatIdx1] = t1;
        outMesh->indices[flatIdx2] = t2;

        outMesh->_positions[flatIdx0] = GfVec3ToGLM(indexedPositions[t0]);
        outMesh->_positions[flatIdx1] = GfVec3ToGLM(indexedPositions[t1]);
        outMesh->_positions[flatIdx2] = GfVec3ToGLM(indexedPositions[t2]);

        if (hasNormals && !hasFlattenedNormals)
        {
            outMesh->_normals[flatIdx0] = GfVec3ToGLM(indexedNormals[t0]);
            outMesh->_normals[flatIdx1] = GfVec3ToGLM(indexedNormals[t1]);
            outMesh->_normals[flatIdx2] = GfVec3ToGLM(indexedNormals[t2]);
        }
        if (hasUVs && !hasFlattenedUVs)
        {
            outMesh->_uvs[flatIdx0] = GfVec2ToGLM(indexedUVs[t0]);
            outMesh->_uvs[flatIdx1] = GfVec2ToGLM(indexedUVs[t1]);
            outMesh->_uvs[flatIdx2] = GfVec2ToGLM(indexedUVs[t2]);
        }
    }
    if (!hasNormals && !hasFlattenedNormals)
    {  // fill with default value if has none
        std::fill(outMesh->_normals.begin(), outMesh->_normals.end(), FLOAT3(0.f, 0.f, 0.f));
    }
    if (!hasUVs && !hasFlattenedUVs)
    {  // fill with default value if has none
        std::fill(outMesh->_uvs.begin(), outMesh->_uvs.end(), FLOAT2(0.f, 0.f));
    }

    outMesh->populateVertices();  // populate when everything is finalized for simplicity
}

void NPTracerHdMesh::_AddToScene()
{
    Scene* scene = _pCreator->GetScene();
    if (scene)
    {
        const SdfPath& id = GetId();
        _pMesh = scene->addMesh();
        _pMesh->objectId = id.GetHash();
        _pMesh->scenePath = id.GetString();

        NP_DBG("Added mesh '%s' to scene\n", id.GetAsString().c_str());
    }
}

void NPTracerHdMesh::_RemoveFromScene()
{
    Scene* scene = _pCreator->GetScene();
    if (scene && _pMesh)
    {
        bool removed = scene->removeMesh(_pMesh->objectId);
        _pMesh = nullptr;

        NP_DBG("Removed mesh '%s' from scene: %b\n", GetId().GetAsString().c_str(), removed);
    }
}

VtValue NPTracerHdMesh::sGetPrimvar(SdfPath const& id, HdSceneDelegate* delegate,
                                    const TfToken& name)
{
    return delegate->Get(id, name);
}

HdDirtyBits NPTracerHdMesh::_PropagateDirtyBits(HdDirtyBits bits) const
{
    return bits;
}

void NPTracerHdMesh::_InitRepr(TfToken const& reprToken, HdDirtyBits*)
{
    _ReprVector::iterator it = std::find_if(_reprs.begin(), _reprs.end(),
                                            _ReprComparator(reprToken));
    if (it == _reprs.end())
    {
        _reprs.emplace_back(reprToken, HdReprSharedPtr());
    }
}

bool NPTracerHdMesh::sReadMeshPrimvars(SdfPath const& id, HdSceneDelegate* delegate,
                                       const HdMeshUtil& meshUtil, VtValue* pvValueOut,
                                       const std::function<bool(const std::string&)>& pred)
{
    HdPrimvarDescriptorVector primvars
        = delegate->GetPrimvarDescriptors(id, HdInterpolation::HdInterpolationFaceVarying);

    const HdPrimvarDescriptor* foundPvDesc = nullptr;
    for (size_t idx = 0; idx < primvars.size(); idx++)
    {
        const HdPrimvarDescriptor& pvDesc = primvars[idx];
        const std::string& pvName = pvDesc.name.GetString();
        if (pred(pvName))
        {
            foundPvDesc = &pvDesc;
            break;
        }
    }

    if (!foundPvDesc)
    {
        return false;
    }

    const TfToken nameToken = foundPvDesc->name;

    // get the underlying data of the descriptor
    VtValue pv = sGetPrimvar(id, delegate, nameToken);

    // make a named buffer of the data
    HdVtBufferSource buffer(nameToken, pv);

    // triangulate the primvar values
    return meshUtil.ComputeTriangulatedFaceVaryingPrimvar(buffer.GetData(),
                                                          static_cast<int>(buffer.GetNumElements()),
                                                          buffer.GetTupleType().type, pvValueOut);
}

PXR_NAMESPACE_CLOSE_SCOPE
