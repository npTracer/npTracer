#pragma once

#include <glm/gtc/type_ptr.hpp>
#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/gf/vec2f.h>

PXR_NAMESPACE_OPEN_SCOPE

#define GfVec2ToGLM(_gfVecPtr) (glm::make_vec2(reinterpret_cast<const float*>(_gfVecPtr.data())))
#define GfVec3ToGLM(_gfVecPtr) (glm::make_vec3(reinterpret_cast<const float*>(_gfVecPtr.data())))
#define GfVec4ToGLM(_gfVecPtr) (glm::make_vec4(reinterpret_cast<const float*>(_gfVecPtr.data())))
#define GfMatrix4fToGLM(_gfMatPtr)                                                                 \
    (glm::make_mat4(reinterpret_cast<const float*>(_gfMatPtr.data())))
#define VtVec2fArrayToGLM(_gfArrPtr)                                                               \
    (std::vector<FLOAT2>(reinterpret_cast<const FLOAT2*>(_gfArrPtr.data()),                        \
                         reinterpret_cast<const FLOAT2*>(_gfArrPtr.data()) + _gfArrPtr.size()))
#define VtVec3fArrayToGLM(_gfArrPtr)                                                               \
    (std::vector<FLOAT3>(reinterpret_cast<const FLOAT3*>(_gfArrPtr.data()),                        \
                         reinterpret_cast<const FLOAT3*>(_gfArrPtr.data()) + _gfArrPtr.size()))

PXR_NAMESPACE_CLOSE_SCOPE
