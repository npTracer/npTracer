#pragma once

#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/vt/array.h>

PXR_NAMESPACE_OPEN_SCOPE

inline glm::f32vec2 GfToGLMVec2f(const GfVec2f gfVec)
{
    return glm::make_vec2(gfVec.data());
}

inline glm::f32vec3 GfToGLMVec3f(const GfVec3f gfVec)
{
    return glm::make_vec3(gfVec.data());
}

inline glm::f32vec4 GfToGLMVec4f(const GfVec4f gfVec)
{
    return glm::make_vec4(gfVec.data());
}

inline glm::f32mat4 GfToGLMMat4f(const GfMatrix4f& gfMat)
{
    return glm::make_mat4(gfMat.data());
}

inline glm::f32mat4 GfToGLMMat4d(const GfMatrix4d& gfMat)
{
    return GfToGLMMat4f(GfMatrix4f(gfMat));
}

template<typename VtType, typename GLMType>
inline void VtArrayToGLMVector(const VtType& vtArr, std::vector<GLMType>* const outVec)
{
    outVec->clear();
    if (vtArr.empty()) return;
    outVec->reserve(vtArr.size());
    memcpy(outVec->data(), vtArr.data(), vtArr.size() * sizeof(vtArr[0]));
}

PXR_NAMESPACE_CLOSE_SCOPE
