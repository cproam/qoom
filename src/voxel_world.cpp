#include "voxel_world.h"
#include "level.h"

void VoxelWorld::buildFromLevel(const Level& level){
    voxels_.clear();
    colliders_.clear();
    for (const auto& inst : level.instances()){
        Voxel v; v.center = inst.position; v.size = inst.scale; voxels_.push_back(v);
        glm::vec3 he = v.size * 0.5f * collisionScale_;
        glm::vec3 c = v.center; // keep center in world units
        colliders_.push_back({c - he, c + he});
    }
}
