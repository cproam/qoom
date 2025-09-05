#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "controller.h" // for AABB

class Level;

struct Voxel {
    glm::vec3 center{0};
    glm::vec3 size{1};
};

class VoxelWorld {
public:
    void buildFromLevel(const Level& level);
    const std::vector<Voxel>& voxels() const { return voxels_; }
    const std::vector<AABB>& colliders() const { return colliders_; }
    void setCollisionScale(float s) { collisionScale_ = s; }
private:
    std::vector<Voxel> voxels_;
    std::vector<AABB> colliders_;
    float collisionScale_ = 1.0f;
};
