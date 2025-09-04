#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

struct AMeshPrimitive {
    GLuint vao=0, vbo=0, ebo=0;
    GLsizei indexCount=0;
    GLenum indexType=GL_UNSIGNED_INT;
    int materialIndex = -1;
};

struct AMaterial {
    GLuint baseColorTex = 0; // sRGB
    GLuint ormTex = 0;       // linear, (R=occlusion, G=roughness, B=metallic)
    GLuint normalTex = 0;    // normal map (tangent space)
    glm::vec4 baseColorFactor{1,1,1,1};
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    bool hasBaseColor = false;
    bool hasORM = false;
    bool hasNormal = false;
    bool dispersion = false;
    float ior = 1.5f;      // index of refraction
    float abbe = 50.0f;    // dispersion parameter
};

class AssimpModel {
public:
    bool load(const std::string& path);
    void draw(const class ShaderProgram& shader) const;
private:
    std::vector<AMeshPrimitive> meshes_;
    std::vector<AMaterial> materials_;
    std::string baseDir_;
    void clear();
    GLuint loadTextureFromAssimp(const struct aiTexture* tex, bool srgb) const;
    GLuint loadTextureFromFile(const std::string& path, bool srgb) const;
};
