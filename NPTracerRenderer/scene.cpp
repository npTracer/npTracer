#include "scene.h"

Scene::Scene() {}

Scene::~Scene() {}

NPMesh* Scene::addMesh()
{
    std::lock_guard<std::mutex> lock(_meshMutex);

    _meshes.push_back(std::make_unique<NPMesh>());
    return _meshes.back().get();
}

bool Scene::removeMesh(const uint32_t& objectId)
{
    std::lock_guard<std::mutex> lock(_meshMutex);

    auto it = std::find_if(_meshes.begin(), _meshes.end(),
                           [&objectId](const std::unique_ptr<NPMesh>& mesh)
                           { return mesh->objectId == objectId; });

    bool found = it != _meshes.end();
    if (found)
    {
        _meshes.erase(it);
    }
    return found;
}

NPMesh const* Scene::getMeshAtIndex(int idx) const
{
    if (idx < 0 || idx >= _meshes.size())
    {
        return nullptr;
    }

    return _meshes[idx].get();
}

NPLight const* Scene::getLightAtIndex(int idx) const
{
    if (idx < 0 || idx >= _lights.size())
    {
        return nullptr;
    }

    return _lights[idx].get();
}
