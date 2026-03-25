#include "scene.h"

Scene::Scene() {}

Scene::~Scene() {}

NPMesh* Scene::addMesh()
{
    _meshes.push_back({});
    return &_meshes.back();
}

bool Scene::removeMesh(const uint32_t& objectId)
{
    auto it = std::find_if(_meshes.begin(), _meshes.end(),
                           [&objectId](const NPMesh& mesh) { return mesh.objectId == objectId; });

    bool found = it != _meshes.end();
    if (found)
    {
        _meshes.erase(it);
    }
    return found;
}
