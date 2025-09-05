#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

struct AMeshPrimitive
{
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLsizei indexCount = 0;
    GLenum indexType = GL_UNSIGNED_INT;
    int materialIndex = -1;
};

struct AMaterial
{
    GLuint baseColorTex = 0; // sRGB
    GLuint ormTex = 0;       // linear, (R=occlusion, G=roughness, B=metallic)
    GLuint normalTex = 0;    // normal map (tangent space)
    GLuint roughnessTex = 0; // linear, if provided separately
    GLuint metalnessTex = 0; // linear, if provided separately
    glm::vec4 baseColorFactor{1, 1, 1, 1};
    float metallicFactor = 0.0f;  // glTF dielectrics default to non-metal
    float roughnessFactor = 0.5f;  // moderate roughness as a practical default
    bool hasBaseColor = false;
    bool hasORM = false;
    bool hasNormal = false;
    bool hasRoughness = false;
    bool hasMetalness = false;
};

class AssimpModel
{
public:
    bool load(const std::string &path);
    void draw(const class ShaderProgram &shader) const;

private:
    std::vector<AMeshPrimitive> meshes_;
    std::vector<AMaterial> materials_;
    std::string baseDir_;
    // Fallbacks
    GLuint defaultWhiteTex_ = 0; // sRGB white for albedo when no texture
    void clear();
    GLuint loadTextureFromAssimp(const struct aiTexture *tex, bool srgb) const;
    GLuint loadTextureFromFile(const std::string &path, bool srgb) const;
};
