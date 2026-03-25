#pragma once

#include "structs.h"

#include <vector>

class Scene
{
public:
    Scene();
    ~Scene();

    NPMesh* addMesh();
    bool removeMesh(const uint32_t& objectId);

    inline const std::vector<NPMesh>& getMeshes() const
    {
        return _meshes;
    }

    inline const std::vector<NPLight>& getLights() const
    {
        return _lights;
    }

    inline NPCameraRecord* getCamera()
    {
        return &_camera;
    }

private:
    std::vector<NPMesh> _meshes;
    std::vector<NPLight> _lights;

    NPCameraRecord _camera = {};

    NPRenderSettings _settings = {};
};
