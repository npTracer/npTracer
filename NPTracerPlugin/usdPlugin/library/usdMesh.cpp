#include "usdPlugin/library/usdMesh.h"

#include <pxr/imaging/hd/vtBufferSource.h>

PXR_NAMESPACE_OPEN_SCOPE

// define extern variable
extern const std::array<TfToken, 3> gUVTokensArray = {
    TfToken("st", TfToken::_ImmortalTag::Immortal),
    TfToken("map1", TfToken::_ImmortalTag::Immortal),  // maya convention
    TfToken("map2", TfToken::_ImmortalTag::Immortal)  // maya co
};

bool IsPositionPrimvarDesc(const HdPrimvarDescriptor& desc)
{
    return desc.name == HdTokens->points || desc.role == HdPrimvarRoleTokens->point;
}

bool IsNormalPrimvarDesc(const HdPrimvarDescriptor& desc)
{
    return desc.name == HdTokens->normals || desc.role == HdPrimvarRoleTokens->normal;
}

bool IsColorPrimvarDesc(const HdPrimvarDescriptor& desc)
{
    return desc.name == HdTokens->displayColor || desc.role == HdPrimvarRoleTokens->color;
}

bool IsUVPrimvarDesc(const HdPrimvarDescriptor& desc)
{
    return desc.name == TfToken("st") || std::string(desc.name).starts_with("map")
           || desc.role == HdPrimvarRoleTokens->textureCoordinate;
}

bool IsPositionPrimvarDirty(const HdDirtyBits* dirtyBits, const SdfPath& id)
{
    return HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->points);
}

bool IsNormalPrimvarDirty(const HdDirtyBits* dirtyBits, const SdfPath& id)
{
    return HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->points);
}

bool IsColorPrimvarDirty(const HdDirtyBits* dirtyBits, const SdfPath& id)
{
    return HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->normals);
}

bool IsUVPrimvarDirty(const HdDirtyBits* dirtyBits, const SdfPath& id)
{
    static auto pred = [&](const TfToken& token)
    { return HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, token); };

    return std::ranges::any_of(gUVTokensArray.begin(), gUVTokensArray.end(), pred);
}

void ProcessPrimvarsFaceVarying(const HdMeshUtil& meshUtil, const VtU32Array& indices,
                                const std::vector<PrimvarPayloadBase*>& pPayloads)
{
    for (PrimvarPayloadBase* payload : pPayloads)
    {
        if (!payload->isDirty || payload->desc.interpolation != HdInterpolationFaceVarying)
            continue;  // extra guard here

        // reverse type-erasure. assign the primvar data to a named buffer
        const HdVtBufferSource buffer(payload->desc.name, payload->GetSource());

        // try to triangulate the primvars
        VtValue processed;
        bool bCanResolveType = meshUtil.ComputeTriangulatedFaceVaryingPrimvar(
            buffer.GetData(), static_cast<int>(buffer.GetNumElements()), buffer.GetTupleType().type,
            &processed);
        TF_DEV_AXIOM(bCanResolveType);  // if this fails later, we can explictly pass in type
        payload->SetProcessed(processed);
    }
}

void ProcessPrimvarsVertex(const HdMeshUtil& meshUtil, const VtU32Array& indices,
                           const std::vector<PrimvarPayloadBase*>& pPayloads)
{
    const size_t indicesCount = indices.size();

    // prepare our payloads for writing
    for (PrimvarPayloadBase* payload : pPayloads)
    {
        // we can verify this assumption so that we don't have to do the check during write
        TF_DEV_AXIOM(payload->isDirty && payload->desc.interpolation == HdInterpolationVertex);
        payload->Prepare(indicesCount);
    }

    // scatter all payloads in one loop over indices
    for (size_t i = 0; i < indicesCount; ++i)
    {
        const uint32_t idx = indices[i];
        for (PrimvarPayloadBase* payload : pPayloads)
        {
            payload->UnsafeWrite(idx, i);
        }
    }

    for (PrimvarPayloadBase* payload : pPayloads)
    {
        payload->Cooldown();  // we are done!
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
