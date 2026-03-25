#pragma once

#include "structs.h"

#include <vector>

class Scene
{
public:
    Scene();
    ~Scene();

    void addMesh(const NPMesh& mesh);
    bool removeMesh(const uint32_t& objectId);

    inline const std::vector<NPMesh>& getMeshes() const
    {
        return _meshes;
    }

    inline NPCameraRecord* getCamera()
    {
        return &_camera;
    }

    inline void setCamera(NPCameraRecord& camera)
    {
        _camera = camera;
    }

private:
    std::vector<NPMesh> _meshes;
    std::vector<NPLight> _lights;

    NPCameraRecord _camera = {};

    NPRenderSettings _settings = {};
};
