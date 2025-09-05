#pragma once
#include <string>
#include <glad/gl.h>

class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    bool loadFromFiles(const std::string& vsPath, const std::string& fsPath, std::string* log = nullptr);
    void use() const { glUseProgram(program_); }
    GLuint id() const { return program_; }

    // convenience setters
    void set1i(const char* name, int v) const;
    void set1f(const char* name, float v) const;
    void set3f(const char* name, float x, float y, float z) const;
    void set4f(const char* name, float x, float y, float z, float w) const;
    void setMatrix4(const char* name, const float* m) const;

private:
    GLuint program_ = 0;
    static GLuint compile(GLenum type, const std::string& src, std::string* log);
    static bool readFile(const std::string& path, std::string& out);
};
