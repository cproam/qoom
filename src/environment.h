#pragma once
#include <glad/gl.h>
#include <string>

class EnvironmentMap {
public:
    EnvironmentMap() = default;
    ~EnvironmentMap();

    // Load EXR equirectangular environment. Returns false on failure.
    bool loadEXR(const std::string& path);
    GLuint id() const { return tex_; }
    void bind(GLenum unit) const {
        glActiveTexture(unit);
        glBindTexture(GL_TEXTURE_2D, tex_);
    }
private:
    GLuint tex_ = 0;
};
