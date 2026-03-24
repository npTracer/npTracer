#pragma once

#include "structs.h"

#include <vector>

class Scene
{
public:
    Scene();
    ~Scene();

    bool addInstances(const NPScenePathCollection& instances);
    bool clearInstances(const NPScenePathCollection& instances);

    inline const std::vector<NPMesh>& getMeshes() const
    {
        return _meshes;
    }

    inline const NPCameraRecord& getCamera() const
    {
        return _camera;
    }

private:
    std::vector<NPMesh> _meshes;
    std::vector<NPLight> _lights;

    NPCameraRecord _camera = {};

    NPRenderSettings _settings = {};
};
