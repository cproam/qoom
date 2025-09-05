#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

class ShaderProgram;
class AssimpModel;
class EnvironmentMap;
struct LevelInstance;
struct AABB;
class VoxelWorld;
struct AABB;

class Renderer
{
public:
    Renderer();
    ~Renderer();
    bool init();
    void setEnvironment(const EnvironmentMap *env);
    void setCamera(const glm::mat4 &proj, const glm::mat4 &view, const glm::vec3 &camPos);
    void setLightDir(const glm::vec3 &dir);
    // Set the default framebuffer viewport size (pixels)
    void setViewportSize(int w, int h) { screenW_ = w; screenH_ = h; }
    void drawScene(const AssimpModel &model);
    // Render helper: draw a model multiple times with instance transforms
    void drawInstances(const AssimpModel& model, const std::vector<LevelInstance>& instances);
    // Visualize collision boxes (voxels) using a grid texture with specified roughness
    void drawVoxels(const VoxelWorld& world, float roughness = 0.75f, float uvTilesPerMeter = 1.0f);
    void enableSky(bool enable) { skyEnabled_ = enable; }
    // Debug rendering options
    void setDebugOptions(bool wireframe, bool disableCulling) { dbgWireframe_ = wireframe; dbgDisableCull_ = disableCulling; }
    // Debug: draw collider AABBs as wireframe
    void drawColliders(const std::vector<AABB>& colliders);

private:
    bool initShadow();
    void drawSky();
    bool ensureVoxelResources();

    const EnvironmentMap *env_ = nullptr;
    GLuint shadowFBO_ = 0, shadowTex_ = 0;
    GLuint screenVAO_ = 0;
    // Voxel resources
    GLuint voxelVAO_ = 0, voxelVBO_ = 0, voxelEBO_ = 0;
    GLuint gridTex_ = 0;

    ShaderProgram *sky_ = nullptr;
    ShaderProgram *pbr_ = nullptr;
    ShaderProgram *shadow_ = nullptr;
    ShaderProgram *debug_ = nullptr; // simple color shader

    glm::mat4 proj_{1.0f}, view_{1.0f};
    glm::vec3 camPos_{0.0f};
    glm::vec3 lightDir_{-0.3f, -1.0f, -0.2f};
    bool skyEnabled_ = false;

    // Default framebuffer dimensions (set by app each frame)
    int screenW_ = 0;
    int screenH_ = 0;

    // Debug flags
    bool dbgWireframe_ = false;
    bool dbgDisableCull_ = false;
};
