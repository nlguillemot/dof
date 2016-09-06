#pragma once

class Scene;
class IRenderer;
union SDL_Event;

class ISimulation
{
public:
    virtual void Init(Scene* scene, IRenderer* renderer) = 0;
    virtual void HandleEvent(const SDL_Event& ev) = 0;
    virtual void Update() = 0;
};

ISimulation* NewSimulation();