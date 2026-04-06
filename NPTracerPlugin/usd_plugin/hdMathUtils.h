#pragma once

#include <NPTracerRenderer/structs.h>
#include <NPTracerRenderer/utils.h>

#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/gf/vec2f.h>

PXR_NAMESPACE_OPEN_SCOPE

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

template<typename GfType, typename GLMType>
inline GLMType GfToGLM(const GfType& gf)
{
    if constexpr (std::is_same_v<GfType, GfVec2f>) return GfToGLMVec2f(gf);
    else if constexpr (std::is_same_v<GfType, GfVec3f>) return GfToGLMVec3f(gf);
    else if constexpr (std::is_same_v<GfType, GfVec4f>) return GfToGLMVec4f(gf);
    else if constexpr (std::is_same_v<GfType, GfMatrix4f>) return GfToGLMMat4f(gf);
    else if constexpr (std::is_same_v<GfType, GfMatrix4d>) return GfToGLMMat4d(gf);
    DEV_ASSERT(false, "not implemented");
}

PXR_NAMESPACE_CLOSE_SCOPE
