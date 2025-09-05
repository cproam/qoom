#include "controller.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

static inline float clampf(float v, float lo, float hi){ return v < lo ? lo : (v > hi ? hi : v); }

QuakeController::QuakeController() {}

glm::vec3 QuakeController::forward() const {
    float cy = cosf(glm::radians(yaw_));
    float sy = sinf(glm::radians(yaw_));
    float cp = cosf(glm::radians(pitch_));
    float sp = sinf(glm::radians(pitch_));
    return glm::normalize(glm::vec3(cy*cp, sp, sy*cp));
}
glm::vec3 QuakeController::right() const {
    return glm::normalize(glm::cross(forward(), glm::vec3(0,1,0)));
}

void QuakeController::handleMouse(GLFWwindow* window, double xpos, double ypos){
    (void)window;
    if (firstMouse_) { lastX_ = xpos; lastY_ = ypos; firstMouse_ = false; }
    float dx = float(xpos - lastX_);
    float dy = float(lastY_ - ypos);
    lastX_ = xpos; lastY_ = ypos;
    yaw_ += dx * mouseSensitivity;
    pitch_ += dy * mouseSensitivity;
    pitch_ = clampf(pitch_, -maxPitch, maxPitch);
}

void QuakeController::applyFriction(float dt){
    if (!grounded_) return;
    float speed = glm::length(glm::vec2(velocity_.x, velocity_.z));
    if (speed < 1e-4f) return;
    float drop = speed * friction_ * dt;
    float newspeed = std::max(speed - drop, 0.0f);
    if (newspeed != speed){
        float scale = newspeed / speed;
        velocity_.x *= scale;
        velocity_.z *= scale;
    }
}

void QuakeController::accelerate(const glm::vec3& wishdir, float wishspeed, float accel, float dt){
    float currentspeed = glm::dot(glm::vec3(velocity_.x,0,velocity_.z), glm::vec3(wishdir.x,0,wishdir.z));
    float addspeed = wishspeed - currentspeed;
    if (addspeed <= 0) return;
    float accelspeed = accel * dt * wishspeed;
    if (accelspeed > addspeed) accelspeed = addspeed;
    velocity_.x += accelspeed * wishdir.x;
    velocity_.z += accelspeed * wishdir.z;
}

bool QuakeController::aabbOverlap(const AABB& a, const AABB& b) const{
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
           (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
           (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

void QuakeController::resolveCollisions(glm::vec3& pos, AABB& aabb, const std::vector<AABB>& world){
    // Simple axis-separated resolution (swept per axis)
    // Update AABB to new pos
    aabb.min = pos - halfExtents;
    aabb.max = pos + halfExtents;
    grounded_ = false;

    for (const auto& w : world){
        if (!aabbOverlap(aabb, w)) continue;
        // Compute overlap on each axis
        float ox1 = w.max.x - aabb.min.x; // push +X
        float ox2 = aabb.max.x - w.min.x; // push -X
        float oy1 = w.max.y - aabb.min.y; // push +Y
        float oy2 = aabb.max.y - w.min.y; // push -Y
        float oz1 = w.max.z - aabb.min.z; // push +Z
        float oz2 = aabb.max.z - w.min.z; // push -Z

        // pick smallest magnitude push
        float px = (ox1 < ox2) ? ox1 : -ox2;
        float py = (oy1 < oy2) ? oy1 : -oy2;
        float pz = (oz1 < oz2) ? oz1 : -oz2;

        float ax = fabsf(px), ay = fabsf(py), az = fabsf(pz);
        if (ax <= ay && ax <= az){
            pos.x += px; velocity_.x = 0.f;
        } else if (ay <= ax && ay <= az){
            pos.y += py; velocity_.y = 0.f; if (py > 0) grounded_ = true;
        } else {
            pos.z += pz; velocity_.z = 0.f;
        }
        aabb.min = pos - halfExtents;
        aabb.max = pos + halfExtents;
    }
}

glm::mat4 QuakeController::view() const{
    glm::vec3 f = forward();
    return glm::lookAt(position_, position_ + f, glm::vec3(0,1,0));
}

void QuakeController::update(GLFWwindow* window, float dt, const std::vector<AABB>& world){
    // Inputs
    bool up = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    bool down = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    bool left = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
    bool rightK = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
    bool jump = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    bool boost = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;

    glm::vec3 f = forward();
    glm::vec3 r = right();
    f.y = 0; r.y = 0; // wishdir on XZ plane
    f = glm::normalize(f); r = glm::normalize(r);
    glm::vec3 wishdir(0.f);
    if (up) wishdir += f;
    if (down) wishdir -= f;
    if (left) wishdir -= r;
    if (rightK) wishdir += r;
    if (glm::dot(wishdir, wishdir) > 0) wishdir = glm::normalize(wishdir);

    float wishspeed = moveSpeed_ * (boost ? 1.7f : 1.0f);

    // Apply friction if on ground
    applyFriction(dt);

    // Accelerate (ground or air)
    float accel = grounded_ ? accelGround_ : accelAir_;
    accelerate(wishdir, wishspeed, accel, dt);

    // Gravity and jump
    velocity_.y -= gravity_ * dt;
    if (grounded_ && jump) {
        velocity_.y = jumpSpeed_;
        grounded_ = false;
    }

    // Integrate and collide
    glm::vec3 newPos = position_ + velocity_ * dt;
    AABB me{newPos - halfExtents, newPos + halfExtents};
    resolveCollisions(newPos, me, world);
    position_ = newPos;
}
