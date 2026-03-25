#include "scene.h"

Scene::Scene() {}

Scene::~Scene() {}

void Scene::addMesh(const NPMesh& mesh)
{
    _meshes.push_back(mesh);
}

bool Scene::removeMesh(const uint32_t& id)
{
    auto it = std::find_if(_meshes.begin(), _meshes.end(),
                           [&id](const NPMesh& mesh) { return mesh.id == id; });

    bool found = it != _meshes.end();
    if (found)
    {
        _meshes.erase(it);
    }
    return found;
}
