#pragma once

#include <NPTracerRenderer/structs.h>

#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/vt/array.h>

PXR_NAMESPACE_OPEN_SCOPE

using VtU32Array = VtArray<uint32_t>;

inline FLOAT2 GfToGLMVec2f(const GfVec2f gfVec)
{
    return glm::make_vec2(gfVec.data());
}

inline FLOAT3 GfToGLMVec3f(const GfVec3f gfVec)
{
    return glm::make_vec3(gfVec.data());
}

inline FLOAT4 GfToGLMVec4f(const GfVec4f gfVec)
{
    return glm::make_vec4(gfVec.data());
}

inline FLOAT4x4 GfToGLMMat4f(const GfMatrix4f& gfMat)
{
    return glm::make_mat4(gfMat.data());
}

inline FLOAT4x4 GfToGLMMat4f(const GfMatrix4d& gfMat)
{
    return GfToGLMMat4f(GfMatrix4f(gfMat));
}

inline void VtFlattenVec3iArray(const VtVec3iArray& in, VtU32Array* out)
{
    const size_t targetCount = in.size() * 3llu;
    const size_t targetByteSize = sizeof(uint32_t) * targetCount;
    const GfVec3i* inData = in.data();

    TF_DEV_AXIOM(sizeof(inData) == targetByteSize);  // ensure this memcpy will succeed

    out->resize(targetCount);
    std::memcpy(out->data(), in.data(), targetByteSize);
}

PXR_NAMESPACE_CLOSE_SCOPE
