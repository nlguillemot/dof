#include "simulation.h"

#include "scene.h"
#include "renderer.h"

#define ARCBALL_CAMERA_IMPLEMENTATION
#include "arcball_camera.h"

#include "imgui.h"

#include <SDL.h>
#include <glm/gtc/type_ptr.hpp>

class Simulation : public ISimulation
{
public:
    Scene* mScene;
    IRenderer* mRenderer;

    bool mFirstUpdate;

    unsigned int mLastUpdateTick;
    int mLastMouseX;
    int mLastMouseY;
    int mAccumulatedMouseWheel;

    void Init(Scene* scene, IRenderer* renderer) override
    {
        mScene = scene;
        mRenderer = renderer;

        mFirstUpdate = true;

        mAccumulatedMouseWheel = 0;

        std::vector<uint32_t> loadedMeshIDs;

        loadedMeshIDs.clear();
        LoadMeshes(*mScene, "assets/cube/cube.obj", &loadedMeshIDs);
        for (uint32_t loadedMeshID : loadedMeshIDs)
        {
            uint32_t newInstanceID;
            AddInstance(*mScene, loadedMeshID, &newInstanceID);

            // scale up the cube
            uint32_t newTransformID = scene->Instances[newInstanceID].TransformID;
            scene->Transforms[newTransformID].Scale = glm::vec3(2.0f);
        }

        loadedMeshIDs.clear();
        LoadMeshes(*mScene, "assets/teapot/teapot.obj", &loadedMeshIDs);
        for (uint32_t loadedMeshID : loadedMeshIDs)
        {
            uint32_t newInstanceID;
            AddInstance(*mScene, loadedMeshID, &newInstanceID);

            // place the teapot on top of the cube
            uint32_t newTransformID = scene->Instances[newInstanceID].TransformID;
            scene->Transforms[newTransformID].Translation += glm::vec3(0.0f, 2.0f, 0.0f);
        }

        loadedMeshIDs.clear();
        LoadMeshes(*mScene, "assets/floor/floor.obj", &loadedMeshIDs);
        for (uint32_t loadedMeshID : loadedMeshIDs)
        {
            AddInstance(*mScene, loadedMeshID, nullptr);
        }

        Camera mainCamera;
        mainCamera.Eye = glm::vec3(3.0f);
        mainCamera.Target = glm::vec3(0.0f);
        glm::vec3 across = cross(mainCamera.Target - mainCamera.Eye, glm::vec3(0.0f, 1.0f, 0.0f));
        mainCamera.Up = normalize(cross(across, mainCamera.Target - mainCamera.Eye));
        mainCamera.FovY = glm::radians(70.0f);
        mScene->MainCameraID = mScene->Cameras.insert(mainCamera);
    }

    void HandleEvent(const SDL_Event& ev) override
    {
        if (ev.type == SDL_MOUSEWHEEL)
        {
            mAccumulatedMouseWheel += ev.wheel.y;
        }
    }

    void Update() override
    {
        unsigned int currentTick = SDL_GetTicks();
        if (mFirstUpdate)
        {
            mLastUpdateTick = currentTick;
        }

        float dtSec = (currentTick - mLastUpdateTick) / 1000.0f;

        Camera& mainCamera = mScene->Cameras[mScene->MainCameraID];

        int mouseX, mouseY;
        Uint32 mouseButtons = SDL_GetMouseState(&mouseX, &mouseY);

        if (mFirstUpdate)
        {
            mLastMouseX = mouseX;
            mLastMouseY = mouseY;
        }

        // update camera
        arcball_camera_update(
            value_ptr(mainCamera.Eye),
            value_ptr(mainCamera.Target),
            value_ptr(mainCamera.Up),
            nullptr,
            dtSec,
            0.1f, 1.0f, 5.0f,
            mRenderer->GetRenderWidth(), mRenderer->GetRenderHeight(),
            mLastMouseX, mouseX,
            mLastMouseY, mouseY,
            (mouseButtons & SDL_BUTTON_MMASK),
            (mouseButtons & SDL_BUTTON_RMASK),
            mAccumulatedMouseWheel,
            0);

        mainCamera.Aspect = (float)mRenderer->GetRenderWidth() / mRenderer->GetRenderHeight();
        mainCamera.ZNear = 0.01f;

        ImGui::ShowTestWindow();

        mFirstUpdate = false;

        mLastUpdateTick = currentTick;
        mAccumulatedMouseWheel = 0;
        mLastMouseX = mouseX;
        mLastMouseY = mouseY;
    }
};

ISimulation* NewSimulation()
{
    return new Simulation();
}