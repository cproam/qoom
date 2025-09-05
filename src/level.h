#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include "controller.h" // for AABB

struct LevelInstance {
    glm::vec3 position{0};
    glm::vec3 scale{1};
};

class Level {
public:
    bool loadFromIni(const std::string& path);
    const std::vector<AABB>& colliders() const { return colliders_; }
    const std::vector<LevelInstance>& instances() const { return instances_; }
private:
    std::vector<AABB> colliders_;
    std::vector<LevelInstance> instances_;
};
