#include "scene.h"

NP_TRACER_NAMESPACE_BEGIN

void Scene::loadSceneFromPath(const char* path)
{
    DEV_ASSERT(false, "not implemented");
}

void Scene::guard()
{
    std::lock_guard<std::mutex> lock(_readWriteMutex);
}

NP_TRACER_NAMESPACE_END
