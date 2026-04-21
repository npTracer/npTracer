#pragma once
#include "utils.h"

NP_TRACER_NAMESPACE_BEGIN

// definitions of all template functions for `scene.h`

template<ScenePrim T>
inline T* Scene::makePrim()
{
    guard();  // lock the mutex

    std::vector<std::unique_ptr<T>>& primVector = getPrimVector<T>();

    primVector.push_back(std::make_unique<T>());
    return primVector.back().get();
}

template<ScenePrim T>
inline bool Scene::deletePrim(T* primToDelete)
{
    guard();

    std::vector<std::unique_ptr<T>>& primVector = getPrimVector<T>();

    auto it = std::find_if(primVector.begin(), primVector.end(), [&](const std::unique_ptr<T>& prim)
                           { return prim.get() == primToDelete; });

    if (it != primVector.end())
    {
        primVector.erase(it);
        return true;
    }
    return false;
}

template<ScenePrim T>
inline size_t Scene::getPrimCount() const
{
    const std::vector<std::unique_ptr<T>>& primVector = getPrimVector<T>();
    return primVector.size();
}

template<ScenePrim T>
inline T* Scene::getPrimAtIndex(size_t idx)
{
    guard();

    std::vector<std::unique_ptr<T>>& primVector = getPrimVector<T>();
    if (idx < 0 || idx >= primVector.size())
    {
        return nullptr;
    }
    return primVector[idx].get();
}

template<ScenePrim T>
inline std::vector<std::unique_ptr<T>>& Scene::getPrimVector()
{
    if constexpr (std::is_same_v<T, Mesh>) return _meshes;
    else if constexpr (std::is_same_v<T, Light>) return _lights;
    else if constexpr (std::is_same_v<T, Material>) return _materials;
    else if constexpr (std::is_same_v<T, Texture>) return _textures;

    UNREACHABLE_CODE  // since template type constrained by `concept`, assert unreachable to compiler per-platform
}

template<ScenePrim T>
const std::vector<std::unique_ptr<T>>& Scene::getPrimVector() const
{
    return const_cast<Scene*>(this)->getPrimVector<T>();  // const-cast is necessary for compiler
}

NP_TRACER_NAMESPACE_END
