#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cmath>
#include "shader.h"
#include "assimp_model.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include "environment.h"
#include "renderer.h"
#include "controller.h"
#include "level.h"
#include "voxel_world.h"
#include <vector>
#include <string>

struct AppState
{
    Controller *controller = nullptr;
    float fovDeg = 90.0f; // adjustable FOV (degrees)
    bool captureMouse = true;
    std::vector<AABB> world; // level collision
    // Debug
    bool dbgWireframe = false;
    bool dbgDisableCull = false;
};

static void toggle_capture(GLFWwindow *win, AppState *s, bool enable)
{
    s->captureMouse = enable;
    glfwSetInputMode(win, GLFW_CURSOR, enable ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;
    auto *s = reinterpret_cast<AppState *>(glfwGetWindowUserPointer(window));
    if (!s)
        return;
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS)
    {
        toggle_capture(window, s, !s->captureMouse);
    }
    if (key == GLFW_KEY_F2 && action == GLFW_PRESS)
    {
        // Toggle wireframe and culling disable alternately
        if (!s->dbgWireframe && !s->dbgDisableCull) {
            s->dbgWireframe = true; s->dbgDisableCull = false;
        } else if (s->dbgWireframe && !s->dbgDisableCull) {
            s->dbgWireframe = false; s->dbgDisableCull = true;
        } else {
            s->dbgWireframe = false; s->dbgDisableCull = false;
        }
    }
}

static void cursor_pos_callback(GLFWwindow *window, double xpos, double ypos)
{
    auto *s = reinterpret_cast<AppState *>(glfwGetWindowUserPointer(window));
    if (!s || !s->captureMouse)
        return;
    if (s->controller)
        s->controller->handleMouse(window, xpos, ypos);
}

static void glfw_error_callback(int error, const char *description)
{
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main()
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
    {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    // Request OpenGL 3.3 core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow *window = glfwCreateWindow(800, 600, "Qoom", nullptr, nullptr);
    if (!window)
    {
        std::fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    if (!gladLoadGL(glfwGetProcAddress))
    {
        std::fprintf(stderr, "Failed to initialize GLAD\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    int major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    std::printf("OpenGL %d.%d\n", major, minor);

    // App/input setup
    AppState state{};
    glfwSetWindowUserPointer(window, &state);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    toggle_capture(window, &state, true);

    Renderer renderer;
    if (!renderer.init())
    {
        std::fprintf(stderr, "Renderer init failed\n");
        return 1;
    }

    // No visual model; boxes will not be rendered

    // Core GL state handled by Renderer

    // Shadow resources handled by Renderer

    double lastTime = glfwGetTime();

    // Environment (clean EXR-only)
    EnvironmentMap env;
    env.loadEXR("assets/studio.exr");
    renderer.setEnvironment(&env);

    // Controller
    QuakeController qc;
    qc.fovDeg = 90.0f;
    qc.setPosition(glm::vec3(0.f, 3.f, 0.f));
    state.controller = &qc;

    // Load level
    Level level;
    if (!level.loadFromIni("levels/level.ini"))
    {
        std::fprintf(stderr, "Failed to load levels/level.ini, using empty level.\n");
    }
    VoxelWorld vox;
    vox.setCollisionScale(1.0f);
    vox.buildFromLevel(level);
    state.world = vox.colliders();
    while (!glfwWindowShouldClose(window))
    {
        double now = glfwGetTime();
        float dt = float(now - lastTime);
        lastTime = now;
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        // Inform renderer about viewport size for the scene pass
        renderer.setViewportSize(width, height);
        // Build matrices
        float aspect = (height > 0) ? (float)width / (float)height : 1.0f;
        glm::mat4 proj = glm::perspective(glm::radians(state.fovDeg), aspect, 0.1f, 200.0f);
        glm::mat4 view(1.0f);
        // Controller update and movement
        if (state.controller)
            state.controller->update(window, dt, state.world);

        // Adjust FOV with Z (decrease) / X (increase)
        const float fovRate = 60.0f; // deg per second
        if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
            state.fovDeg = glm::clamp(state.fovDeg - fovRate * dt, 20.0f, 120.0f);
        if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS)
            state.fovDeg = glm::clamp(state.fovDeg + fovRate * dt, 20.0f, 120.0f);

        if (state.controller)
        {
            view = state.controller->view();
        }
        renderer.setCamera(proj, view, state.controller ? state.controller->position() : glm::vec3(0));
    renderer.setLightDir(glm::normalize(glm::vec3(-0.3f, -1.0f, -0.2f)));
    renderer.setDebugOptions(state.dbgWireframe, state.dbgDisableCull);

    // Visualize voxels using grid.png with box-projected UVs; keep scale tied to collision
    const float uvTilesPerMeter = 1.0f; // tweak if needed
    renderer.drawVoxels(vox, 0.75f, uvTilesPerMeter);
    // Optional: overlay collision boxes for visual vs collision scale check when culling disabled debug is active
    if (state.dbgDisableCull) {
        renderer.drawColliders(state.world);
    }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
