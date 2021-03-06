#pragma once

#include "Registry.h"
#include "RenderGraph.h"
#include "rendering/scene/Scene.h"

class App {
public:
    App() = default;
    virtual ~App() = default;

    virtual std::vector<Backend::Capability> requiredCapabilities() = 0;
    virtual std::vector<Backend::Capability> optionalCapabilities() = 0;

    virtual void setup(RenderGraph&) = 0;
    virtual void update(float elapsedTime, float deltaTime) = 0;

    Scene& scene() { return *m_scene; }
    const Scene& scene() const { return *m_scene; }

    void createScene(Badge<Backend>, Registry& sceneRegistry)
    {
        m_scene = std::make_unique<Scene>(sceneRegistry);
    }

private:
    std::unique_ptr<Scene> m_scene {};
};
