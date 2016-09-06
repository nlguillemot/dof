#include "renderer.h"

#include "scene.h"

#include "preamble.glsl"

#include "imgui.h"

#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdio>
#include <memory>

class Renderer : public IRenderer
{
public:
    const int kSampleCount = 4;
    const int kMaxTextureCount = 32;

    Scene* mScene;

    ShaderSet mShaders;
    GLuint* mSceneSP;

    int mBackbufferWidth;
    int mBackbufferHeight;
    // multi-sampled buffers
    GLuint mBackbufferFBOMS;
    GLuint mBackbufferColorTOMS;
    GLuint mBackbufferDepthTOMS;
    // single-sampled buffers
    GLuint mBackbufferFBOSS;
    GLuint mBackbufferColorTOSS;
    GLuint mBackbufferDepthTOSS;

    // empty VAO, for attrib-less rendering passes
    GLuint mNullVAO;

    // may be different from backbuffer if scaled
    int mWindowWidth;
    int mWindowHeight;

    bool mUseCPUForSAT;
    glm::uvec4* mCPUSummedAreaTable;
    GLuint* mSummedAreaTableSP;
    GLuint mSummedAreaTableTO;

    GLuint* mDepthOfFieldSP;
    float mFocusDepth;

    void Init(Scene* scene) override
    {
        mScene = scene;

        mShaders.SetVersion("440");
        mShaders.SetPreambleFile("preamble.glsl");

        mSceneSP = mShaders.AddProgramFromExts({ "scene.vert", "scene.frag" });
        mSummedAreaTableSP = mShaders.AddProgramFromExts({ "sat.comp" });
        mDepthOfFieldSP = mShaders.AddProgramFromExts({ "blit.vert", "dof.frag" });

        glGenVertexArrays(1, &mNullVAO);
        glBindVertexArray(mNullVAO);
        glBindVertexArray(0);

        mFocusDepth = 5.0f;
    }

    void Resize(int width, int height) override
    {
        mWindowWidth = width;
        mWindowHeight = height;

        mBackbufferWidth = mWindowWidth;
        mBackbufferHeight = mWindowHeight;

        // OS X doesn't like it when you delete framebuffers it's using
        // No big deal, this happens implicitly anyways.
        glFinish();

        // Init multisampled FBO
        {
            glDeleteTextures(1, &mBackbufferColorTOMS);
            glGenTextures(1, &mBackbufferColorTOMS);
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mBackbufferColorTOMS);
            glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, kSampleCount, GL_SRGB8_ALPHA8, mBackbufferWidth, mBackbufferHeight, GL_TRUE);
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

            glDeleteTextures(1, &mBackbufferDepthTOMS);
            glGenTextures(1, &mBackbufferDepthTOMS);
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mBackbufferDepthTOMS);
            glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, kSampleCount, GL_DEPTH_COMPONENT32F, mBackbufferWidth, mBackbufferHeight, GL_TRUE);
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

            glDeleteFramebuffers(1, &mBackbufferFBOMS);
            glGenFramebuffers(1, &mBackbufferFBOMS);
            glBindFramebuffer(GL_FRAMEBUFFER, mBackbufferFBOMS);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, mBackbufferColorTOMS, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, mBackbufferDepthTOMS, 0);
            GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
                fprintf(stderr, "glCheckFramebufferStatus: %x\n", fboStatus);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        // Init singlesampled FBO
        {
            glDeleteTextures(1, &mBackbufferColorTOSS);
            glGenTextures(1, &mBackbufferColorTOSS);
            glBindTexture(GL_TEXTURE_2D, mBackbufferColorTOSS);
            glTexStorage2D(GL_TEXTURE_2D, 1, GL_SRGB8_ALPHA8, mBackbufferWidth, mBackbufferHeight);
            glBindTexture(GL_TEXTURE_2D, 0);

            glDeleteTextures(1, &mBackbufferDepthTOSS);
            glGenTextures(1, &mBackbufferDepthTOSS);
            glBindTexture(GL_TEXTURE_2D, mBackbufferDepthTOSS);
            glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT32F, mBackbufferWidth, mBackbufferHeight);
            glBindTexture(GL_TEXTURE_2D, 0);

            glDeleteFramebuffers(1, &mBackbufferFBOSS);
            glGenFramebuffers(1, &mBackbufferFBOSS);
            glBindFramebuffer(GL_FRAMEBUFFER, mBackbufferFBOSS);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mBackbufferColorTOSS, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mBackbufferDepthTOSS, 0);
            GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
                fprintf(stderr, "glCheckFramebufferStatus: %x\n", fboStatus);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        // Init summed area table
        {
            delete[] mCPUSummedAreaTable;
            mCPUSummedAreaTable = new glm::uvec4[mBackbufferWidth * mBackbufferHeight];

            glDeleteTextures(1, &mSummedAreaTableTO);
            glGenTextures(1, &mSummedAreaTableTO);
            glBindTexture(GL_TEXTURE_2D, mSummedAreaTableTO);
            glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32UI, mBackbufferWidth, mBackbufferHeight);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    void UpdateGUI()
    {
        if (ImGui::Begin("Renderer"))
        {
            ImGui::Checkbox("CPU SAT", &mUseCPUForSAT);
            ImGui::SliderFloat("Focus Depth", &mFocusDepth, 0.0f, 10.0f);
        }
        ImGui::End();
    }

    void Paint() override
    {
        UpdateGUI();

        // Reload any programs
        mShaders.UpdatePrograms();

        // Render scene
        if (*mSceneSP)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, mBackbufferFBOMS);
            glViewport(0, 0, mBackbufferWidth, mBackbufferHeight);

            glClearColor(100.0f / 255.0f, 149.0f / 255.0f, 237.0f / 255.0f, 1.0f);
            glClearDepth(0.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            Camera& mainCamera = mScene->Cameras[mScene->MainCameraID];

            glm::vec3 eye = mainCamera.Eye;
            glm::vec3 up = mainCamera.Up;

            glm::mat4 V = glm::lookAt(eye, mainCamera.Target, up);
            glm::mat4 P;
            {
                float f = 1.0f / tanf(mainCamera.FovY / 2.0f);
                P = glm::mat4(
                    f / mainCamera.Aspect, 0.0f, 0.0f, 0.0f,
                    0.0f, f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, -1.0f,
                    0.0f, 0.0f, mainCamera.ZNear, 0.0f);
            }

            glm::mat4 VP = P * V;

            glUseProgram(*mSceneSP);

            glUniform3fv(SCENE_CAMERAPOS_UNIFORM_LOCATION, 1, value_ptr(eye));

            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_GREATER);
            glEnable(GL_FRAMEBUFFER_SRGB);
            for (uint32_t instanceID : mScene->Instances)
            {
                const Instance* instance = &mScene->Instances[instanceID];
                const Mesh* mesh = &mScene->Meshes[instance->MeshID];
                const Transform* transform = &mScene->Transforms[instance->TransformID];

                glm::mat4 MW;
                MW = translate(-transform->RotationOrigin) * MW;
                MW = mat4_cast(transform->Rotation) * MW;
                MW = translate(transform->RotationOrigin) * MW;
                MW = scale(transform->Scale) * MW;
                MW = translate(transform->Translation) * MW;

                glm::mat3 N_MW;
                N_MW = mat3_cast(transform->Rotation) * N_MW;
                N_MW = glm::mat3(scale(1.0f / transform->Scale)) * N_MW;

                glm::mat4 MVP = VP * MW;

                glUniformMatrix4fv(SCENE_MW_UNIFORM_LOCATION, 1, GL_FALSE, value_ptr(MW));
                glUniformMatrix3fv(SCENE_N_MW_UNIFORM_LOCATION, 1, GL_FALSE, value_ptr(N_MW));
                glUniformMatrix4fv(SCENE_MVP_UNIFORM_LOCATION, 1, GL_FALSE, value_ptr(MVP));

                glBindVertexArray(mesh->MeshVAO);
                for (size_t meshDrawIdx = 0; meshDrawIdx < mesh->DrawCommands.size(); meshDrawIdx++)
                {
                    const GLDrawElementsIndirectCommand* drawCmd = &mesh->DrawCommands[meshDrawIdx];
                    const Material* material = &mScene->Materials[mesh->MaterialIDs[meshDrawIdx]];

                    glActiveTexture(GL_TEXTURE0 + SCENE_DIFFUSE_MAP_TEXTURE_BINDING);
                    if (material->DiffuseMapID == -1)
                    {
                        glBindTexture(GL_TEXTURE_2D, 0);
                        glUniform1i(SCENE_HAS_DIFFUSE_MAP_UNIFORM_LOCATION, 0);
                    }
                    else
                    {
                        const DiffuseMap* diffuseMap = &mScene->DiffuseMaps[material->DiffuseMapID];
                        glBindTexture(GL_TEXTURE_2D, diffuseMap->DiffuseMapTO);
                        glUniform1i(SCENE_HAS_DIFFUSE_MAP_UNIFORM_LOCATION, 1);
                    }

                    glUniform3fv(SCENE_AMBIENT_UNIFORM_LOCATION, 1, material->Ambient);
                    glUniform3fv(SCENE_DIFFUSE_UNIFORM_LOCATION, 1, material->Diffuse);
                    glUniform3fv(SCENE_SPECULAR_UNIFORM_LOCATION, 1, material->Specular);
                    glUniform1f(SCENE_SHININESS_UNIFORM_LOCATION, material->Shininess);

                    glDrawElementsInstancedBaseVertexBaseInstance(
                        GL_TRIANGLES,
                        drawCmd->count,
                        GL_UNSIGNED_INT, (GLvoid*)(sizeof(uint32_t) * drawCmd->firstIndex),
                        drawCmd->primCount,
                        drawCmd->baseVertex,
                        drawCmd->baseInstance);
                }
                glBindVertexArray(0);
            }
            glBindTextures(0, kMaxTextureCount, NULL);
            glDisable(GL_FRAMEBUFFER_SRGB);
            glDepthFunc(GL_LESS);
            glDisable(GL_DEPTH_TEST);
            glUseProgram(0);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        // resolve multisampled backbuffer to singlesampled backbuffer
        {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, mBackbufferFBOMS);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mBackbufferFBOSS);
            glBlitFramebuffer(
                0, 0, mBackbufferWidth, mBackbufferHeight,
                0, 0, mBackbufferWidth, mBackbufferHeight,
                GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        // Compute SAT for the rendered image
        if (mUseCPUForSAT)
        {
            // Dumb CPU SAT. Mainly used as a reference.
            glBindFramebuffer(GL_READ_FRAMEBUFFER, mBackbufferFBOSS);
            std::vector<glm::u8vec4> pixels(mBackbufferWidth * mBackbufferHeight);
            glReadPixels(0, 0, mBackbufferWidth, mBackbufferHeight, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);
            std::copy(begin(pixels), end(pixels), mCPUSummedAreaTable);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

            // sum the rows
            for (int row = 0; row < mBackbufferHeight; row++)
            {
                for (int col = 1; col < mBackbufferWidth; col++)
                {
                    mCPUSummedAreaTable[row * mBackbufferWidth + col] += mCPUSummedAreaTable[row * mBackbufferWidth + (col - 1)];
                }
            }

            // sum the columns (gross memory access...)
            for (int col = 0; col < mBackbufferWidth; col++)
            {
                for (int row = 1; row < mBackbufferHeight; row++)
                {
                    mCPUSummedAreaTable[row * mBackbufferWidth + col] += mCPUSummedAreaTable[(row-1) * mBackbufferWidth + col];
                }
            }

            glBindTexture(GL_TEXTURE_2D, mSummedAreaTableTO);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mBackbufferWidth, mBackbufferHeight, GL_RGBA_INTEGER, GL_UNSIGNED_INT, &mCPUSummedAreaTable[0]);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        else
        {
            // GPU SAT
            if (*mSummedAreaTableSP)
            {
                glUseProgram(*mSummedAreaTableSP);
                glBindImageTexture(SAT_INPUT_IMAGE_BINDING, mBackbufferColorTOSS, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8UI);
                glBindImageTexture(SAT_OUTPUT_IMAGE_BINDING, mSummedAreaTableTO, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32UI);
            
                int nWorkGroupsX = (mBackbufferWidth + SAT_WORKGROUP_SIZE_X - 1) / SAT_WORKGROUP_SIZE_X;
                int nWorkGroupsY = (mBackbufferHeight + SAT_WORKGROUP_SIZE_Y - 1) / SAT_WORKGROUP_SIZE_Y;
                glDispatchCompute(nWorkGroupsX, nWorkGroupsY, 1);
            
                glBindImageTextures(SAT_INPUT_IMAGE_BINDING, 1, NULL);
                glBindImageTextures(SAT_OUTPUT_IMAGE_BINDING, 1, NULL);
                glUseProgram(0);
            }
        }

        // Apply DoF-blur to scene
        if (*mDepthOfFieldSP)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, mBackbufferFBOSS);
            glUseProgram(*mDepthOfFieldSP);
            glBindVertexArray(mNullVAO);
            glBindTextures(DOF_SAT_TEXTURE_BINDING, 1, &mSummedAreaTableTO);
            glBindTextures(DOF_DEPTH_TEXTURE_BINDING, 1, &mBackbufferDepthTOSS);
            glEnable(GL_FRAMEBUFFER_SRGB);

            Camera& mainCamera = mScene->Cameras[mScene->MainCameraID];

            glUniform1f(DOF_ZNEAR_UNIFORM_LOCATION, mainCamera.ZNear);
            glUniform1f(DOF_FOCUS_UNIFORM_LOCATION, mFocusDepth);
            
            glDrawArrays(GL_TRIANGLES, 0, 3);
            
            glDisable(GL_FRAMEBUFFER_SRGB);
            glBindTextures(DOF_SAT_TEXTURE_BINDING, 1, NULL);
            glBindTextures(DOF_DEPTH_TEXTURE_BINDING, 1, NULL);
            glBindVertexArray(0);
            glUseProgram(0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        // Render GUI
        {
            glBindFramebuffer(GL_FRAMEBUFFER, mBackbufferFBOSS);
            ImGui::Render();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        // Blit to window's framebuffer
        {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, mBackbufferFBOSS);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // default FBO
            
            bool scaled = mWindowWidth != mBackbufferWidth || mWindowHeight != mBackbufferHeight;
            glBlitFramebuffer(
                0, 0, mBackbufferWidth, mBackbufferHeight,
                0, 0, mWindowWidth, mWindowHeight,
                GL_COLOR_BUFFER_BIT, scaled ? GL_LINEAR : GL_NEAREST);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }

    int GetRenderWidth() const override
    {
        return mBackbufferWidth;
    }

    int GetRenderHeight() const override
    {
        return mBackbufferHeight;
    }

    void* operator new(size_t sz)
    {
        void* mem = ::operator new(sz);
        memset(mem, 0, sz);
        return mem;
    }
};

IRenderer* NewRenderer()
{
    return new Renderer();
}