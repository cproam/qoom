#pragma once
#include <glm/glm.hpp>
#include <vector>
struct GLFWwindow;

struct AABB { glm::vec3 min, max; };

class Controller {
public:
    virtual ~Controller() = default;
    virtual void handleMouse(GLFWwindow* window, double xpos, double ypos) = 0;
    virtual void update(GLFWwindow* window, float dt, const std::vector<AABB>& world) = 0;
    virtual glm::mat4 view() const = 0;
    virtual glm::vec3 position() const = 0;
};

class QuakeController : public Controller {
public:
    QuakeController();
    void handleMouse(GLFWwindow* window, double xpos, double ypos) override;
    void update(GLFWwindow* window, float dt, const std::vector<AABB>& world) override;
    glm::mat4 view() const override;
    glm::vec3 position() const override { return position_; }
    void setPosition(const glm::vec3& p) { position_ = p; }

    // Config
    float fovDeg = 90.f;
    float mouseSensitivity = 0.12f; // deg/pixel
    float maxPitch = 89.0f;
    // Player shape (AABB half extents)
    glm::vec3 halfExtents{0.3f, 0.9f, 0.3f}; // ~0.6m x 1.8m x 0.6m

private:
    // Movement helpers
    void accelerate(const glm::vec3& wishdir, float wishspeed, float accel, float dt);
    void applyFriction(float dt);
    bool aabbOverlap(const AABB& a, const AABB& b) const;
    void resolveCollisions(glm::vec3& pos, AABB& aabb, const std::vector<AABB>& world);

    glm::vec3 forward() const;
    glm::vec3 right() const;

    // State
    glm::vec3 position_{0.f, 1.0f, 3.0f};
    glm::vec3 velocity_{0.f};
    float yaw_ = -90.f;  // degrees
    float pitch_ = 0.f;  // degrees
    bool firstMouse_ = true; double lastX_ = 0.0, lastY_ = 0.0;
    bool grounded_ = false;

    // Tuning (approx Quake-like)
    float moveSpeed_ = 6.0f;       // target ground speed m/s
    float accelGround_ = 10.0f;    // ground acceleration m/s^2
    float accelAir_ = 1.5f;        // air acceleration m/s^2
    float friction_ = 6.0f;        // ground friction
    float gravity_ = 9.81f;        // m/s^2
    float jumpSpeed_ = 5.0f;       // m/s (vertical impulse)
};
