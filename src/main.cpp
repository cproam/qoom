#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cmath>
#include "shader.h"
#include "assimp_model.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

struct FPSCamera {
    glm::vec3 pos{0.0f, 1.0f, 3.0f};
    float yaw = -90.0f;   // degrees, -Z forward
    float pitch = 0.0f;   // degrees
    float speed = 4.0f;   // units/sec
    float sensitivity = 0.12f; // deg/pixel
    bool  captureMouse = true;

    glm::vec3 front() const {
        float cy = cosf(glm::radians(yaw));
        float sy = sinf(glm::radians(yaw));
        float cp = cosf(glm::radians(pitch));
        float sp = sinf(glm::radians(pitch));
        return glm::normalize(glm::vec3(cy*cp, sp, sy*cp));
    }
    glm::mat4 view() const {
        glm::vec3 f = front();
        return glm::lookAt(pos, pos + f, glm::vec3(0,1,0));
    }
};

struct AppState {
    FPSCamera cam{};
    bool firstMouse = true;
    double lastX = 0.0, lastY = 0.0;
};

static void toggle_capture(GLFWwindow* win, AppState* s, bool enable) {
    s->cam.captureMouse = enable;
    s->firstMouse = true;
    glfwSetInputMode(win, GLFW_CURSOR, enable ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode; (void)mods;
    auto* s = reinterpret_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!s) return;
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        toggle_capture(window, s, !s->cam.captureMouse);
    }
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    auto* s = reinterpret_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!s || !s->cam.captureMouse) return;
    if (s->firstMouse) { s->lastX = xpos; s->lastY = ypos; s->firstMouse = false; }
    float dx = float(xpos - s->lastX);
    float dy = float(s->lastY - ypos); // inverted Y
    s->lastX = xpos; s->lastY = ypos;
    s->cam.yaw   += dx * s->cam.sensitivity;
    s->cam.pitch += dy * s->cam.sensitivity;
    if (s->cam.pitch > 89.0f) s->cam.pitch = 89.0f;
    if (s->cam.pitch < -89.0f) s->cam.pitch = -89.0f;
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

    // Load shader
    ShaderProgram pbr;
    std::string log;
    if (!pbr.loadFromFiles("shaders/pbr.vert", "shaders/pbr.frag", &log)) {
        std::fprintf(stderr, "%s\n", log.c_str());
    }

    // Load model
    AssimpModel model;
    if (!model.load("assets/test.glb")) {
        std::fprintf(stderr, "Failed to load assets/test.glb\n");
    }

    // Default white texture bound to unit 0 for base color sampling
    GLuint whiteTex = 0;
    glGenTextures(1, &whiteTex);
    glBindTexture(GL_TEXTURE_2D, whiteTex);
    unsigned char whitePixel[4] = {255,255,255,255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FRAMEBUFFER_SRGB); // correct gamma for sRGB textures

    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = float(now - lastTime);
        lastTime = now;
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.1f, 0.16f, 0.24f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Movement (WASD + Space/Ctrl, Shift boost)
        float spd = state.cam.speed * (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 2.5f : 1.0f);
        glm::vec3 f = state.cam.front();
        glm::vec3 r = glm::normalize(glm::cross(f, glm::vec3(0,1,0)));
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) state.cam.pos += f * spd * dt;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) state.cam.pos -= f * spd * dt;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) state.cam.pos -= r * spd * dt;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) state.cam.pos += r * spd * dt;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) state.cam.pos.y += spd * dt;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) state.cam.pos.y -= spd * dt;

        // Basic MVP from camera
        float aspect = (height > 0) ? (float)width / (float)height : 1.0f;
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 200.0f);
        glm::mat4 view = state.cam.view();
    glm::mat4 modelM = glm::scale(glm::mat4(1.0f), glm::vec3(100.0f));
        glm::mat4 mvp = proj * view * modelM;

        pbr.use();
        pbr.setMatrix4("uMVP", &mvp[0][0]);
    pbr.set3f("uLightDir", -0.3f, -1.0f, -0.2f);
    pbr.set3f("uLightColor", 5.0f, 5.0f, 5.0f); // brighter light
    pbr.set1f("uMetallic", 0.2f);
    pbr.set1f("uRoughness", 0.5f);
    pbr.set1i("uBaseColorTex", 0);
    pbr.set1i("uORMTex", 1);
    pbr.set1i("uNormalTex", 2);
    pbr.set1i("uHasORM", 0);
    pbr.set1i("uHasNormal", 0);
    pbr.set1i("uDispersionEnabled", 0);
    pbr.set1f("uAbbeNumber", 50.0f);
    pbr.set1f("uIOR", 1.5f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, whiteTex);

    model.draw(pbr);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
