#pragma once

#include "opengl.h"
#include "shaderset.h"

class Scene;

class IRenderer
{
public:
    virtual void Init(Scene* scene) = 0;
    virtual void Resize(int width, int height) = 0;
    virtual void Paint() = 0;

    virtual int GetRenderWidth() const = 0;
    virtual int GetRenderHeight() const = 0;
};

IRenderer* NewRenderer();