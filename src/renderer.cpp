#include "renderer.h"
#include "shader.h"
#include "assimp_model.h"
#include "environment.h"
#include <glm/gtc/matrix_transform.hpp>
#include "level.h"
#include "voxel_world.h"
#include <stb_image.h>

Renderer::Renderer() {}
Renderer::~Renderer()
{
    if (shadowTex_)
        glDeleteTextures(1, &shadowTex_);
    if (shadowFBO_)
        glDeleteFramebuffers(1, &shadowFBO_);
    if (screenVAO_)
        glDeleteVertexArrays(1, &screenVAO_);
    if (voxelEBO_)
        glDeleteBuffers(1, &voxelEBO_);
    if (voxelVBO_)
        glDeleteBuffers(1, &voxelVBO_);
    if (voxelVAO_)
        glDeleteVertexArrays(1, &voxelVAO_);
    if (gridTex_)
        glDeleteTextures(1, &gridTex_);
    delete sky_;
    delete pbr_;
    delete shadow_;
}

bool Renderer::init()
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glGenVertexArrays(1, &screenVAO_);

    sky_ = new ShaderProgram();
    pbr_ = new ShaderProgram();
    shadow_ = new ShaderProgram();
    debug_ = new ShaderProgram();
    std::string log;
    if (!sky_->loadFromFiles("shaders/env_sky.vert", "shaders/env_sky.frag", &log))
        return false;
    log.clear();
    if (!pbr_->loadFromFiles("shaders/pbr.vert", "shaders/pbr.frag", &log))
        return false;
    log.clear();
    if (!shadow_->loadFromFiles("shaders/shadow.vert", "shaders/shadow.frag", &log))
        return false;
    // Minimal debug shader (inline)
    const char* dbg_vs = R"(#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uMVP;
void main(){ gl_Position = uMVP * vec4(aPos,1.0); }
)";
    const char* dbg_fs = R"(#version 330 core
out vec4 FragColor; uniform vec3 uColor; void main(){ FragColor = vec4(uColor,1.0); }
)";
    if (!debug_->loadFromSource(dbg_vs, dbg_fs, &log)) return false;
    return initShadow();
}

bool Renderer::initShadow()
{
    const int SHADOW_SIZE = 4096;
    glGenFramebuffers(1, &shadowFBO_);
    glGenTextures(1, &shadowTex_);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_SIZE, SHADOW_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[4] = {1.f, 1.f, 1.f, 1.f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void Renderer::drawColliders(const std::vector<AABB>& colliders){
    if (!voxelVAO_) ensureVoxelResources();
    if (!voxelVAO_) return;
    if (screenW_ > 0 && screenH_ > 0) glViewport(0,0,screenW_,screenH_);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    debug_->use();
    GLint locColor = glGetUniformLocation(debug_->id(), "uColor");
    glUniform3f(locColor, 1.0f, 0.1f, 0.1f);
    glBindVertexArray(voxelVAO_);
    for (const auto& b : colliders){
        glm::vec3 center = (b.min + b.max) * 0.5f;
        glm::vec3 size = (b.max - b.min);
        glm::mat4 M(1.0f);
        M = glm::translate(M, center);
        M = glm::scale(M, size);
        glm::mat4 MVP = proj_ * view_ * M;
        debug_->setMatrix4("uMVP", &MVP[0][0]);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void Renderer::setEnvironment(const EnvironmentMap *env) { env_ = env; }
void Renderer::setCamera(const glm::mat4 &proj, const glm::mat4 &view, const glm::vec3 &camPos)
{
    proj_ = proj;
    view_ = view;
    camPos_ = camPos;
}
void Renderer::setLightDir(const glm::vec3 &dir) { lightDir_ = dir; }

void Renderer::drawSky()
{
    if (!env_ || !env_->id() || !skyEnabled_)
        return;
    glDisable(GL_DEPTH_TEST);
    sky_->use();
    sky_->set1i("uEnvEquirect", 0);
    // camera basis from view matrix (columns of inverse view)
    glm::mat3 camBasis(
        glm::vec3(view_[0][0], view_[1][0], view_[2][0]),
        glm::vec3(view_[0][1], view_[1][1], view_[2][1]),
        -glm::vec3(view_[0][2], view_[1][2], view_[2][2]));
    glUniformMatrix3fv(glGetUniformLocation(sky_->id(), "uCameraBasis"), 1, GL_FALSE, &camBasis[0][0]);
    env_->bind(GL_TEXTURE0);
    glBindVertexArray(screenVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::drawScene(const AssimpModel &model)
{
    // light matrices
    glm::vec3 lightPos = -lightDir_ * 64.0f;
    glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0, 1, 0));
    glm::mat4 lightProj = glm::ortho(-32.0f, 32.0f, -32.0f, 32.0f, 1.0f, 128.0f);
    glm::mat4 modelM(1.0f);
    glm::mat4 lightMVP = lightProj * lightView * modelM;

    // shadow pass
    glViewport(0, 0, 4096, 4096);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO_);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.f, 4.f);
    shadow_->use();
    shadow_->setMatrix4("uLightMVP", &lightMVP[0][0]);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    model.draw(*shadow_);
    glCullFace(GL_BACK);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // scene pass
    // Ensure we restore the viewport to the default framebuffer size
    if (screenW_ > 0 && screenH_ > 0)
        glViewport(0, 0, screenW_, screenH_);
    glClearColor(0.1f, 0.16f, 0.24f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawSky();

    glm::mat4 mvp = proj_ * view_ * modelM;
    pbr_->use();
    pbr_->setMatrix4("uMVP", &mvp[0][0]);
    pbr_->setMatrix4("uModel", &modelM[0][0]);
    glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(modelM)));
    glUniformMatrix3fv(glGetUniformLocation(pbr_->id(), "uNormalMatrix"), 1, GL_FALSE, &normalMat[0][0]);
    pbr_->setMatrix4("uLightMVP", &lightMVP[0][0]);
    pbr_->set3f("uLightDir", lightDir_.x, lightDir_.y, lightDir_.z);
    pbr_->set3f("uLightColor", 5.f, 5.f, 5.f);
    pbr_->set3f("uAmbientColor", 0.05f, 0.05f, 0.05f);
    pbr_->set1i("uBaseColorTex", 0);
    pbr_->set1i("uORMTex", 1);
    pbr_->set1i("uNormalTex", 2);
    pbr_->set1i("uRoughnessTex", 5);
    pbr_->set1i("uMetalnessTex", 6);
    pbr_->set4f("uBaseColorFactor", 1.f, 1.f, 1.f, 1.f);
    pbr_->set1i("uShadowMap", 3);
    pbr_->set1i("uEnvEquirect", 4);
    pbr_->set1i("uHasORM", 0);
    pbr_->set1i("uHasNormal", 0);
    pbr_->set1i("uHasRoughness", 0);
    pbr_->set1i("uHasMetalness", 0);
    pbr_->set3f("uCameraPos", camPos_.x, camPos_.y, camPos_.z);
    pbr_->set1f("uEnvSpecStrength", 2.0f);
    pbr_->set1f("uEnvDiffStrength", 1.0f);
    pbr_->set1f("uOverrideRoughness", 0.25f);
    pbr_->set1f("uOverrideMetallic", -1.0f);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    if (env_ && env_->id())
    {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, env_->id());
    }

    model.draw(*pbr_);
}

void Renderer::drawInstances(const AssimpModel &model, const std::vector<LevelInstance> &instances)
{
    // light matrices
    glm::vec3 lightPos = -lightDir_ * 64.0f;
    glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0, 1, 0));
    glm::mat4 lightProj = glm::ortho(-32.0f, 32.0f, -32.0f, 32.0f, 1.0f, 128.0f);

    // shadow pass per instance
    glViewport(0, 0, 4096, 4096);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO_);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.f, 4.f);
    shadow_->use();
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    for (const auto &inst : instances)
    {
        glm::mat4 M(1.0f);
        M = glm::translate(M, inst.position);
        M = glm::scale(M, inst.scale);
        glm::mat4 LMVP = lightProj * lightView * M;
        shadow_->setMatrix4("uLightMVP", &LMVP[0][0]);
        model.draw(*shadow_);
    }
    glCullFace(GL_BACK);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // scene pass per instance
    if (screenW_ > 0 && screenH_ > 0)
        glViewport(0, 0, screenW_, screenH_);
    glClearColor(0.1f, 0.16f, 0.24f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const unsigned idx[] = {
        // Z- face (front, outward normal -Z): keep CCW
        0, 1, 2, 0, 2, 3,
        // Z+ face (back, outward normal +Z): reverse winding
        4, 6, 5, 4, 7, 6,
        // X- face (left, outward normal -X): reverse winding
        8, 10, 9, 8, 11, 10,
        // X+ face (right, outward normal +X): keep CCW
        12, 13, 14, 12, 14, 15,
        // Y- face (bottom, outward normal -Y): reverse winding
        16, 18, 17, 16, 19, 18,
        // Y+ face (top, outward normal +Y): keep CCW
        20, 21, 22, 20, 22, 23};
    pbr_->set3f("uCameraPos", camPos_.x, camPos_.y, camPos_.z);
    pbr_->set1f("uEnvSpecStrength", 2.0f);
    pbr_->set1f("uEnvDiffStrength", 1.0f);
    pbr_->set1f("uOverrideRoughness", 0.25f);
    pbr_->set1f("uOverrideMetallic", -1.0f);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    if (env_ && env_->id())
    {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, env_->id());
    }

    for (const auto &inst : instances)
    {
        glm::mat4 M(1.0f);
        M = glm::translate(M, inst.position);
        M = glm::scale(M, inst.scale);
        glm::mat4 MVP = proj_ * view_ * M;
        glm::mat3 N = glm::mat3(glm::transpose(glm::inverse(M)));
        pbr_->setMatrix4("uModel", &M[0][0]);
        pbr_->setMatrix4("uMVP", &MVP[0][0]);
        glUniformMatrix3fv(glGetUniformLocation(pbr_->id(), "uNormalMatrix"), 1, GL_FALSE, &N[0][0]);
        model.draw(*pbr_);
    }
}

bool Renderer::ensureVoxelResources()
{
    if (!voxelVAO_)
    {
        // unit cube geometry (positions, normals, uvs)
        const float v[] = {
            // pos              // norm        // uv
            -0.5f,
            -0.5f,
            -0.5f,
            0,
            0,
            -1,
            0,
            0,
            0.5f,
            -0.5f,
            -0.5f,
            0,
            0,
            -1,
            1,
            0,
            0.5f,
            0.5f,
            -0.5f,
            0,
            0,
            -1,
            1,
            1,
            -0.5f,
            0.5f,
            -0.5f,
            0,
            0,
            -1,
            0,
            1,

            -0.5f,
            -0.5f,
            0.5f,
            0,
            0,
            1,
            0,
            0,
            0.5f,
            -0.5f,
            0.5f,
            0,
            0,
            1,
            1,
            0,
            0.5f,
            0.5f,
            0.5f,
            0,
            0,
            1,
            1,
            1,
            -0.5f,
            0.5f,
            0.5f,
            0,
            0,
            1,
            0,
            1,

            -0.5f,
            -0.5f,
            -0.5f,
            -1,
            0,
            0,
            0,
            0,
            -0.5f,
            0.5f,
            -0.5f,
            -1,
            0,
            0,
            1,
            0,
            -0.5f,
            0.5f,
            0.5f,
            -1,
            0,
            0,
            1,
            1,
            -0.5f,
            -0.5f,
            0.5f,
            -1,
            0,
            0,
            0,
            1,

            0.5f,
            -0.5f,
            -0.5f,
            1,
            0,
            0,
            0,
            0,
            0.5f,
            0.5f,
            -0.5f,
            1,
            0,
            0,
            1,
            0,
            0.5f,
            0.5f,
            0.5f,
            1,
            0,
            0,
            1,
            1,
            0.5f,
            -0.5f,
            0.5f,
            1,
            0,
            0,
            0,
            1,

            -0.5f,
            -0.5f,
            -0.5f,
            0,
            -1,
            0,
            0,
            0,
            0.5f,
            -0.5f,
            -0.5f,
            0,
            -1,
            0,
            1,
            0,
            0.5f,
            -0.5f,
            0.5f,
            0,
            -1,
            0,
            1,
            1,
            -0.5f,
            -0.5f,
            0.5f,
            0,
            -1,
            0,
            0,
            1,

            -0.5f,
            0.5f,
            -0.5f,
            0,
            1,
            0,
            0,
            0,
            0.5f,
            0.5f,
            -0.5f,
            0,
            1,
            0,
            1,
            0,
            0.5f,
            0.5f,
            0.5f,
            0,
            1,
            0,
            1,
            1,
            -0.5f,
            0.5f,
            0.5f,
            0,
            1,
            0,
            0,
            1,
        };
        const unsigned idx[] = {
            // -Z face (outward -Z)
            0, 2, 1, 0, 3, 2,
            // +Z face (outward +Z)
            4, 5, 6, 4, 6, 7,
            // -X face (outward -X)
            8, 10, 9, 8, 11, 10,
            // +X face (outward +X)
            12, 13, 14, 12, 14, 15,
            // -Y face (outward -Y)
            16, 17, 18, 16, 18, 19,
            // +Y face (outward +Y)
            20, 22, 21, 20, 23, 22};
        glGenVertexArrays(1, &voxelVAO_);
        glBindVertexArray(voxelVAO_);
        glGenBuffers(1, &voxelVBO_);
        glBindBuffer(GL_ARRAY_BUFFER, voxelVBO_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
        glGenBuffers(1, &voxelEBO_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, voxelEBO_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
        const GLsizei stride = (3 + 3 + 2) * sizeof(float);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void *)(6 * sizeof(float)));
        glBindVertexArray(0);
    }
    if (!gridTex_)
    {
        int w = 0, h = 0, c = 0;
        unsigned char *data = stbi_load("assets/grid.png", &w, &h, &c, 4);
        if (data)
        {
            glGenTextures(1, &gridTex_);
            glBindTexture(GL_TEXTURE_2D, gridTex_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            stbi_image_free(data);
        }
        else
        {
            fprintf(stderr, "Failed to load assets/grid.png (w=%d h=%d c=%d)\n", w, h, c);
        }
    }
    return voxelVAO_ != 0;
}

void Renderer::drawVoxels(const VoxelWorld &world, float roughness, float uvTilesPerMeter)
{
    if (!ensureVoxelResources())
        return;

    // light matrices
    glm::vec3 lightPos = -lightDir_ * 64.0f;
    glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0, 1, 0));
    glm::mat4 lightProj = glm::ortho(-32.0f, 32.0f, -32.0f, 32.0f, 1.0f, 128.0f);

    // shadow pass
    glViewport(0, 0, 4096, 4096);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO_);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.f, 4.f);
    shadow_->use();
    if (!dbgDisableCull_) { glEnable(GL_CULL_FACE); glCullFace(GL_FRONT); }
    glBindVertexArray(voxelVAO_);
    if (!world.voxels().empty())
    {
        const auto &v = world.voxels().front();
        glm::vec3 center = v.center;
        glm::vec3 size = v.size;
        glm::mat4 M(1.0f);
        M = glm::translate(M, center);
        M = glm::scale(M, size);
        glm::mat4 LMVP = lightProj * lightView * M;
        shadow_->setMatrix4("uLightMVP", &LMVP[0][0]);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
    if (!dbgDisableCull_) glCullFace(GL_BACK);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // scene pass
    if (screenW_ > 0 && screenH_ > 0)
        glViewport(0, 0, screenW_, screenH_);
    glClearColor(0.1f, 0.16f, 0.24f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawSky();

    pbr_->use();
    pbr_->set1i("uShadowMap", 3);
    pbr_->set1i("uEnvEquirect", 4);
    pbr_->set3f("uLightDir", lightDir_.x, lightDir_.y, lightDir_.z);
    pbr_->set3f("uLightColor", 5.f, 5.f, 5.f);
    pbr_->set3f("uAmbientColor", 0.05f, 0.05f, 0.05f);
    pbr_->set3f("uCameraPos", camPos_.x, camPos_.y, camPos_.z);
    pbr_->set1f("uEnvSpecStrength", 2.0f);
    pbr_->set1f("uEnvDiffStrength", 1.0f);
    pbr_->set1f("uOverrideRoughness", roughness);
    pbr_->set1f("uOverrideMetallic", -1.0f);
    pbr_->set1i("uHasORM", 0);
    pbr_->set1i("uHasNormal", 0);
    pbr_->set1i("uHasRoughness", 0);
    pbr_->set1i("uHasMetalness", 0);
    pbr_->set4f("uBaseColorFactor", 1.f, 1.f, 1.f, 1.f);
    pbr_->set1i("uUseBoxUVMapping", 1);
    pbr_->set1f("uBoxUVScale", uvTilesPerMeter);

    // Bind grid texture as base color
    pbr_->set1i("uBaseColorTex", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gridTex_);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    if (env_ && env_->id())
    {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, env_->id());
    }

    if (dbgWireframe_) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    if (dbgDisableCull_) glDisable(GL_CULL_FACE);
    glBindVertexArray(voxelVAO_);
    if (!world.voxels().empty())
    {
        const auto &v = world.voxels().front();
        glm::vec3 center = v.center;
        glm::vec3 size = v.size;
        glm::mat4 M(1.0f);
        M = glm::translate(M, center);
        M = glm::scale(M, size);
        glm::mat4 MVP = proj_ * view_ * M;
        glm::mat4 LMVP = lightProj * lightView * M;
        glm::mat3 N = glm::mat3(glm::transpose(glm::inverse(M)));
        pbr_->setMatrix4("uModel", &M[0][0]);
        pbr_->setMatrix4("uMVP", &MVP[0][0]);
        pbr_->setMatrix4("uLightMVP", &LMVP[0][0]);
        glUniformMatrix3fv(glGetUniformLocation(pbr_->id(), "uNormalMatrix"), 1, GL_FALSE, &N[0][0]);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
    if (dbgWireframe_) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if (dbgDisableCull_) glEnable(GL_CULL_FACE);
    // restore mapping flag if subsequent draws occur
    pbr_->set1i("uUseBoxUVMapping", 0);
}
