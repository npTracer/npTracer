#include "scene.h"

void Scene::loadSceneFromPath(const char* path)
{
    DEV_ASSERT(false, "not implemented");
}

void Scene::guard()
{
    std::lock_guard<std::mutex> lock(_readWriteMutex);
}
