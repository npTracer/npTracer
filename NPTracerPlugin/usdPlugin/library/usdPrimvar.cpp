#include "usdPlugin/library/usdPrimvar.h"

#include <pxr/imaging/hd/vtBufferSource.h>

#include <format>

PXR_NAMESPACE_OPEN_SCOPE

constexpr char kSTYLIZATION_ID_PRIMVAR_NAME[] = "npTracer:stylizationId";

// define extern variable
extern const std::array<TfToken, 3> gUVTokensArray = {
    TfToken("st", TfToken::_ImmortalTag::Immortal),
    TfToken("map1", TfToken::_ImmortalTag::Immortal),  // maya convention
    TfToken("map2", TfToken::_ImmortalTag::Immortal)  // maya convention as well
};

// sometimes render delegates in Houdini author a corrupted `TfToken` object?
bool IsTfTokenCorrupted(const TfToken& token)
{
    if (token.IsEmpty()) return true;
    // guard against corrupted object
    if (reinterpret_cast<uintptr_t>(&token) == 0) return true;

    // try access in a controlled way
    const char* txt = token.GetText();
    // reject invalid pointers, i.e. below page boundary
    if (reinterpret_cast<uintptr_t>(txt) < 4096) return true;

    return false;
}

std::string StringToLowercase(std::string str)
{
    std::ranges::transform(str.begin(), str.end(), str.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return str;
}

std::optional<uint32_t> ProcessTokenAsPrimvar(const ePrimvarType& primvarType,
                                              const std::string& tokenString)
{
    if (primvarType == ePrimvarType::STYLIZATION_ID)
    {
        np::eStylizationId result;
        std::string tokenNormalized = StringToLowercase(tokenString);
        if (tokenNormalized == "greyscale") result = np::eStylizationId::GREYSCALE;
        else if (tokenNormalized == "toon") result = np::eStylizationId::TOON;
        else if (tokenNormalized == "stripes") result = np::eStylizationId::STRIPES;
        else if (tokenNormalized == "crosshatch") result = np::eStylizationId::CROSSHATCH;
        else
        {
            TF_WARN("The given primvar value `%s` for custom primvar `%s` is not within the set of "
                    "valid values.\n",
                    tokenNormalized, kSTYLIZATION_ID_PRIMVAR_NAME);
            return std::nullopt;
        }
        return std::make_optional<uint32_t>(result);
    }
    return std::nullopt;  // not a target token
}

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

bool IsStylizationIdPrimvarDesc(const HdPrimvarDescriptor& desc)
{
    return desc.name == TfToken(kSTYLIZATION_ID_PRIMVAR_NAME);
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
    return std::ranges::any_of(gUVTokensArray.begin(), gUVTokensArray.end(),
                               [dirtyBits, id](const TfToken& uvToken)
                               { return HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, uvToken); });
}

bool IsStylizationIdPrimvarDirty(const HdDirtyBits* dirtyBits, const SdfPath& id)
{
    // use a static variable as creating a `TfToken` is expensive
    static TfToken stylizationIdToken = TfToken(kSTYLIZATION_ID_PRIMVAR_NAME);
    return HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, stylizationIdToken);
}

void ProcessPrimvarsConstant(const HdMeshUtil& meshUtil, const VtU32Array& indices,
                             const VtIntArray& primitiveParams,
                             const std::vector<PrimvarPayloadBase*>& pPayloads)
{
    const size_t count = indices.size();

    for (PrimvarPayloadBase* payload : pPayloads)
    {
        if (!payload->isDirty || payload->desc.interpolation != HdInterpolationConstant
            || payload->bIsConstantValue)
            continue;
        payload->FillConstant(count);
    }
}

void ProcessPrimvarsUniform(const HdMeshUtil& meshUtil, const VtU32Array& indices,
                            const VtIntArray& primitiveParams,
                            const std::vector<PrimvarPayloadBase*>& pPayloads)
{
    const size_t indicesCount = indices.size();
    TF_DEV_AXIOM(primitiveParams.size() * 3
                 == indicesCount);  // if fails, `hdMeshUtil` not consistent

    // prepare our payloads for writing
    for (PrimvarPayloadBase* payload : pPayloads)
    {
        // we can verify this assumption so that we don't have to do the check during write
        TF_DEV_AXIOM(payload->isDirty && payload->desc.interpolation == HdInterpolationUniform);
        payload->Prepare(indicesCount);
    }

    // decode the face index and fill the corresponding indices in a single loop
    size_t triIdx = 0;
    for (size_t i = 0; i < indicesCount; i += 3, ++triIdx)
    {
        const uint32_t faceIdx = HdMeshUtil::DecodeFaceIndexFromCoarseFaceParam(
            primitiveParams[triIdx]);

        for (PrimvarPayloadBase* payload : pPayloads)
        {
            payload->UnsafeWrite(faceIdx, i);
            payload->UnsafeWrite(faceIdx, i + 1);
            payload->UnsafeWrite(faceIdx, i + 2);
        }
    }

    for (PrimvarPayloadBase* payload : pPayloads)
        payload->Cooldown();  // we are done!
}

void ProcessPrimvarsFaceVarying(const HdMeshUtil& meshUtil, const VtU32Array& indices,
                                const VtIntArray& primitiveParams,
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
        TF_DEV_AXIOM(bCanResolveType);  // if this fails later, we can explicitly pass in type

        payload->SetProcessed(processed);
    }
}

void ProcessPrimvarsVertex(const HdMeshUtil& meshUtil, const VtU32Array& indices,
                           const VtIntArray& primitiveParams,
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

    // gather all payloads in one loop over indices
    for (size_t i = 0; i < indicesCount; ++i)
    {
        const uint32_t idx = indices[i];
        for (PrimvarPayloadBase* payload : pPayloads)
            payload->UnsafeWrite(idx, i);
    }

    for (PrimvarPayloadBase* payload : pPayloads)
        payload->Cooldown();  // we are done!
}

PXR_NAMESPACE_CLOSE_SCOPE
