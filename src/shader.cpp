#include "shader.h"
#include <vector>
#include <fstream>
#include <sstream>

ShaderProgram::~ShaderProgram() {
    if (program_) glDeleteProgram(program_);
}

bool ShaderProgram::readFile(const std::string& path, std::string& out) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;
    std::ostringstream oss;
    oss << ifs.rdbuf();
    out = oss.str();
    return true;
}

GLuint ShaderProgram::compile(GLenum type, const std::string& src, std::string* log) {
    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0; glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> buf(len);
        glGetShaderInfoLog(s, len, nullptr, buf.data());
        if (log) *log += std::string(buf.data(), buf.size());
        glDeleteShader(s);
        return 0;
    }
    return s;
}

bool ShaderProgram::loadFromFiles(const std::string& vsPath, const std::string& fsPath, std::string* log) {
    std::string vs, fs;
    if (!readFile(vsPath, vs) || !readFile(fsPath, fs)) {
        if (log) *log += "Failed to read shader files\n";
        return false;
    }
    GLuint v = compile(GL_VERTEX_SHADER, vs, log);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs, log);
    if (!v || !f) return false;
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0; glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> buf(len);
        glGetProgramInfoLog(p, len, nullptr, buf.data());
        if (log) *log += std::string(buf.data(), buf.size());
        glDeleteProgram(p);
        return false;
    }
    program_ = p;
    return true;
}

void ShaderProgram::set1i(const char* name, int v) const {
    glUniform1i(glGetUniformLocation(program_, name), v);
}
void ShaderProgram::set1f(const char* name, float v) const {
    glUniform1f(glGetUniformLocation(program_, name), v);
}
void ShaderProgram::set3f(const char* name, float x, float y, float z) const {
    glUniform3f(glGetUniformLocation(program_, name), x, y, z);
}
void ShaderProgram::set4f(const char* name, float x, float y, float z, float w) const {
    glUniform4f(glGetUniformLocation(program_, name), x, y, z, w);
}
void ShaderProgram::setMatrix4(const char* name, const float* m) const {
    glUniformMatrix4fv(glGetUniformLocation(program_, name), 1, GL_FALSE, m);
}
