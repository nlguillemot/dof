// Scene - the "Model"
#include "scene.h"
// Renderer - the "View"
#include "renderer.h"
// Simulation - the "Controller"
#include "simulation.h"

#include "mysdl_dpi.h"

#include "opengl.h"
#include "imgui.h"
#include "imgui_impl_sdl_gl3.h"

#include <SDL.h>

#include <cstdio>

void GLAPIENTRY DebugCallbackGL(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
    fprintf(stderr, "DebugCallbackGL: %s\n", message);
}

extern "C"
int main(int argc, char* argv[])
{
    if (MySDL_SetProcessDpiAware())
    {
        fprintf(stderr, "MySDL_SetProcessDpiAware: %s\n", SDL_GetError());
    }

    if (SDL_Init(SDL_INIT_VIDEO))
    {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        exit(1);
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 4);
#ifdef _DEBUG
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

    SDL_Window* window = SDL_CreateWindow(
        "viewer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        exit(1);
    }

    Uint32 windowID = SDL_GetWindowID(window);

    SDL_GLContext glctx = SDL_GL_CreateContext(window);
    if (!glctx)
    {
        fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError());
        exit(1);
    }

    // Load OpenGL procs
    OpenGL_Init();

#ifdef _DEBUG
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, 0, GL_FALSE);
    glDebugMessageCallback(DebugCallbackGL, 0);
#endif

    // cure the stupidity
    {
        GLint major, minor;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        if ((major > 4 || (major == 4 && minor >= 5)) ||
            SDL_GL_ExtensionSupported("GL_ARB_clip_control"))
        {
            glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
        }
        else
        {
            fprintf(stderr, "glClipControl required, sorry.\n");
            exit(1);
        }
    }

    ImGui_ImplSdlGL3_Init(window);

    Scene scene;
    scene.Init();

    IRenderer* renderer = NewRenderer();
    renderer->Init(&scene);
    
    // Initial resize
    {
        int initialWidth, initialHeight;
        SDL_GL_GetDrawableSize(window, &initialWidth, &initialHeight);
        renderer->Resize(initialWidth, initialHeight);
    }

    ISimulation* sim = NewSimulation();
    sim->Init(&scene, renderer);

    while (true)
    {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            ImGui_ImplSdlGL3_ProcessEvent(&ev);

            if (ev.type == SDL_QUIT)
            {
                goto mainloop_end;
            }
            else if (ev.type == SDL_WINDOWEVENT)
            {
                if (ev.window.windowID == windowID)
                {
                    if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        renderer->Resize(ev.window.data1, ev.window.data2);
                    }
                }
            }
            else if (ev.type == SDL_KEYDOWN)
            {
                if (ev.key.keysym.sym == SDLK_RETURN)
                {
                    if (ev.key.keysym.mod & KMOD_ALT)
                    {
                        Uint32 wflags = SDL_GetWindowFlags(window);
                        if (wflags & SDL_WINDOW_FULLSCREEN_DESKTOP)
                        {
                            SDL_SetWindowFullscreen(window, 0);
                        }
                        else
                        {
                            SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                        }
                    }
                }
            }

            sim->HandleEvent(ev);
        }

        ImGui_ImplSdlGL3_NewFrame(window);

        sim->Update();
        renderer->Paint();

        SDL_GL_SwapWindow(window);
    }
mainloop_end:

    return 0;
}