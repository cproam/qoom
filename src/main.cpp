#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cmath>
#include "shader.h"
#include "assimp_model.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <tinyexr.h>
// fallback HDR loader
#include <stb_image.h>

// Toggle sky background rendering (reflections will still use the environment)
static const bool kRenderSky = false;

struct FPSCamera
{
    glm::vec3 pos{0.0f, 1.0f, 3.0f};
    float yaw = -90.0f;        // degrees, -Z forward
    float pitch = 0.0f;        // degrees
    float speed = 4.0f;        // units/sec
    float sensitivity = 0.12f; // deg/pixel
    bool captureMouse = true;

    glm::vec3 front() const
    {
        float cy = cosf(glm::radians(yaw));
        float sy = sinf(glm::radians(yaw));
        float cp = cosf(glm::radians(pitch));
        float sp = sinf(glm::radians(pitch));
        return glm::normalize(glm::vec3(cy * cp, sp, sy * cp));
    }
    glm::mat4 view() const
    {
        glm::vec3 f = front();
        return glm::lookAt(pos, pos + f, glm::vec3(0, 1, 0));
    }
};

struct AppState
{
    FPSCamera cam{};
    bool firstMouse = true;
    double lastX = 0.0, lastY = 0.0;
};

static void toggle_capture(GLFWwindow *win, AppState *s, bool enable)
{
    s->cam.captureMouse = enable;
    s->firstMouse = true;
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
        toggle_capture(window, s, !s->cam.captureMouse);
    }
}

static void cursor_pos_callback(GLFWwindow *window, double xpos, double ypos)
{
    auto *s = reinterpret_cast<AppState *>(glfwGetWindowUserPointer(window));
    if (!s || !s->cam.captureMouse)
        return;
    if (s->firstMouse)
    {
        s->lastX = xpos;
        s->lastY = ypos;
        s->firstMouse = false;
    }
    float dx = float(xpos - s->lastX);
    float dy = float(s->lastY - ypos); // inverted Y
    s->lastX = xpos;
    s->lastY = ypos;
    s->cam.yaw += dx * s->cam.sensitivity;
    s->cam.pitch += dy * s->cam.sensitivity;
    if (s->cam.pitch > 89.0f)
        s->cam.pitch = 89.0f;
    if (s->cam.pitch < -89.0f)
        s->cam.pitch = -89.0f;
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
    ShaderProgram sky;
    std::string skylog;
    if (!sky.loadFromFiles("shaders/env_sky.vert", "shaders/env_sky.frag", &skylog))
    {
        std::fprintf(stderr, "%s\n", skylog.c_str());
    }
    std::string log;
    if (!pbr.loadFromFiles("shaders/pbr.vert", "shaders/pbr.frag", &log))
    {
        std::fprintf(stderr, "%s\n", log.c_str());
    }

    // Shadow map: depth-only shader
    ShaderProgram shadow;
    std::string slog;
    if (!shadow.loadFromFiles("shaders/shadow.vert", "shaders/shadow.frag", &slog))
    {
        std::fprintf(stderr, "%s\n", slog.c_str());
    }

    // Load model
    AssimpModel model;
    if (!model.load("assets/test.glb"))
    {
        std::fprintf(stderr, "Failed to load assets/test.glb\n");
    }

    // Default white texture bound to unit 0 for base color sampling
    GLuint whiteTex = 0;
    glGenTextures(1, &whiteTex);
    glBindTexture(GL_TEXTURE_2D, whiteTex);
    unsigned char whitePixel[4] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FRAMEBUFFER_SRGB); // correct gamma for sRGB textures
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Core profile requires a VAO to be bound even if no VBOs are used
    GLuint screenVAO = 0;
    glGenVertexArrays(1, &screenVAO);

    // Create shadow map resources
    const int SHADOW_SIZE = 4096;
    GLuint shadowFBO = 0, shadowTex = 0;
    glGenFramebuffers(1, &shadowFBO);
    glGenTextures(1, &shadowTex);
    glBindTexture(GL_TEXTURE_2D, shadowTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_SIZE, SHADOW_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    // Enable depth compare for sampler2DShadow
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::fprintf(stderr, "Shadow framebuffer incomplete\n");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    double lastTime = glfwGetTime();

    // Load environment EXR (preferred) or fallback to HDR via stb_image
    GLuint envTex = 0;
    {
        const char *exrPath = "assets/studio.exr";
        float *out;
        int w, h;
        const char *err = nullptr;
        int ret = LoadEXR(&out, &w, &h, exrPath, &err);
        if (ret == TINYEXR_SUCCESS)
        {
            glGenTextures(1, &envTex);
            glBindTexture(GL_TEXTURE_2D, envTex);
            // TinyEXR LoadEXR returns RGBA float image
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, out);
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            free(out);
        }
        else
        {
            if (err)
            {
                std::fprintf(stderr, "EXR load error: %s\n", err);
                FreeEXRErrorMessage(err);
            }
            // Fallback: try LDR image (PNG/JPG) only
            int ww = 0, hh = 0, comp = 0;
            unsigned char *ldr = stbi_load("assets/studio.jpg", &ww, &hh, &comp, 3);
            if (!ldr)
                ldr = stbi_load("assets/studio.png", &ww, &hh, &comp, 3);
            if (ldr)
            {
                glGenTextures(1, &envTex);
                glBindTexture(GL_TEXTURE_2D, envTex);
                // Use sRGB internal format so sampling converts to linear
                glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, ww, hh, 0, GL_RGB, GL_UNSIGNED_BYTE, ldr);
                glGenerateMipmap(GL_TEXTURE_2D);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                stbi_image_free(ldr);
                std::fprintf(stderr, "Loaded LDR environment (studio.jpg/png). Lighting range may be limited.\n");
            }
            else
            {
                std::fprintf(stderr, "No environment found. Place assets/studio.exr (ZIP/PIZ) or studio.jpg/png.\n");
            }
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    while (!glfwWindowShouldClose(window))
    {
        double now = glfwGetTime();
        float dt = float(now - lastTime);
        lastTime = now;
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        // Build matrices
        float aspect = (height > 0) ? (float)width / (float)height : 1.0f;
        glm::mat4 proj = glm::perspective(glm::radians(85.0f), aspect, 0.1f, 200.0f);
        glm::mat4 view = state.cam.view();
        glm::mat4 modelM = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
        glm::mat4 mvp = proj * view * modelM;

        // Light view-projection (directional)
        glm::vec3 lightDir = glm::normalize(glm::vec3(-0.3f, -1.0f, -0.2f));
        glm::vec3 lightPos = -lightDir * 64.0f; // place far along direction
        glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0, 1, 0));
        glm::mat4 lightProj = glm::ortho(-32.0f, 32.0f, -32.0f, 32.0f, 1.0f, 128.0f);
        glm::mat4 lightVP = lightProj * lightView;
        glm::mat4 lightMVP = lightVP * modelM;

        // Shadow pass
        glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.0f, 4.0f);
        shadow.use();
        shadow.setMatrix4("uLightMVP", &lightMVP[0][0]);
        // Render back faces into the shadow map to reduce self-shadowing
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        model.draw(shadow);
        glCullFace(GL_BACK);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Scene pass
        glViewport(0, 0, width, height);
        glClearColor(0.05f, 0.05f, 0.125f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Sky background (disabled by default, reflections still use envTex)
        if (envTex && kRenderSky)
        {
            glDisable(GL_DEPTH_TEST);
            sky.use();
            sky.set1i("uEnvEquirect", 0);
            // Build camera basis (right, up, forward)
            glm::vec3 fwd = state.cam.front();
            glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
            glm::vec3 up = glm::normalize(glm::cross(right, fwd));
            glm::mat3 camBasis(right, up, fwd);
            glUniformMatrix3fv(glGetUniformLocation(sky.id(), "uCameraBasis"), 1, GL_FALSE, &camBasis[0][0]);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, envTex);
            glBindVertexArray(screenVAO);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindVertexArray(0);
            glEnable(GL_DEPTH_TEST);
        }

        // Movement (WASD + Space/Ctrl, Shift boost)
        float spd = state.cam.speed * (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 2.5f : 1.0f);
        glm::vec3 f = state.cam.front();
        glm::vec3 r = glm::normalize(glm::cross(f, glm::vec3(0, 1, 0)));
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            state.cam.pos += f * spd * dt;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            state.cam.pos -= f * spd * dt;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            state.cam.pos -= r * spd * dt;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            state.cam.pos += r * spd * dt;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            state.cam.pos.y += spd * dt;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
            state.cam.pos.y -= spd * dt;

        pbr.use();
        pbr.setMatrix4("uMVP", &mvp[0][0]);
        pbr.setMatrix4("uModel", &modelM[0][0]);
        glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(modelM)));
        glUniformMatrix3fv(glGetUniformLocation(pbr.id(), "uNormalMatrix"), 1, GL_FALSE, &normalMat[0][0]);
        pbr.setMatrix4("uLightMVP", &lightMVP[0][0]);
        pbr.set3f("uLightDir", -0.3f, -1.0f, -0.2f);
        pbr.set3f("uLightColor", 5.0f, 5.0f, 5.0f);      // brighter directional
        pbr.set3f("uAmbientColor", 0.05f, 0.05f, 0.05f); // ambient light
        pbr.set1i("uBaseColorTex", 0);
        pbr.set1i("uORMTex", 1);
        pbr.set1i("uNormalTex", 2);
        pbr.set1i("uRoughnessTex", 5);
        pbr.set1i("uMetalnessTex", 6);
        pbr.set4f("uBaseColorFactor", 1.0f, 1.0f, 1.0f, 1.0f);
        pbr.set1i("uShadowMap", 3);
        pbr.set1i("uEnvEquirect", 4);
        pbr.set1i("uHasORM", 0);
        pbr.set1i("uHasNormal", 0);
        pbr.set1i("uHasRoughness", 0);
        pbr.set1i("uHasMetalness", 0);
        pbr.set3f("uCameraPos", state.cam.pos.x, state.cam.pos.y, state.cam.pos.z);
        // IBL strengths: boost a bit to make reflections clearly visible
        pbr.set1f("uEnvSpecStrength", 2.0f);
        pbr.set1f("uEnvDiffStrength", 1.0f);
    // Temporary global material overrides for untextured assets
    pbr.set1f("uOverrideRoughness", 0.25f); // shinier by default
    pbr.set1f("uOverrideMetallic", -1.0f);  // keep non-metal unless desired
        // Don't bind a default base color here; material draw() will bind per-material or fallback.
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, shadowTex);
        if (envTex)
        {
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, envTex);
        }

        model.draw(pbr);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
