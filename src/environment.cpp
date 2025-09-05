#include "environment.h"
#include <tinyexr.h>
#include <vector>

EnvironmentMap::~EnvironmentMap(){
    if (tex_) glDeleteTextures(1, &tex_);
}

bool EnvironmentMap::loadEXR(const std::string& path){
    if (tex_) { glDeleteTextures(1, &tex_); tex_ = 0; }
    float* out = nullptr; int w = 0, h = 0; const char* err = nullptr;
    int ret = LoadEXR(&out, &w, &h, path.c_str(), &err);
    if (ret != TINYEXR_SUCCESS) {
        if (err) FreeEXRErrorMessage(err);
        return false;
    }
    glGenTextures(1, &tex_);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, out);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    free(out);
    return true;
}
