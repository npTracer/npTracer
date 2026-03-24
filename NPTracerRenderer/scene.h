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

private:
    std::vector<NPMesh> meshes;
    std::vector<NPLight> lights;

    NPCamera cam = {};

    NPRenderSettings settings = {};
};
