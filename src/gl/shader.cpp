#include <string>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <stdexcept>

#include <glad/glad.h>

#include "shader.hpp"

// Static utility function to go from file -> string
static std::string readTextFile(const std::string &path) {
    std::ifstream f(path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

Shader::Shader(Shader&& other) noexcept {
    program_ = other.program_;
    uniform_cache_ = std::move(other.uniform_cache_);
    other.program_ = 0;
    other.uniform_cache_.clear();
}

Shader::~Shader() {
    if (program_) {
        glDeleteProgram((GLuint)program_);
        program_ = 0;
    }
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (program_) glDeleteProgram((GLuint)program_);
        program_ = other.program_;
        uniform_cache_ = std::move(other.uniform_cache_);
        other.program_ = 0;
        other.uniform_cache_.clear();
    }
    return *this;
}

static bool compile_shader(GLuint &shader_out, GLuint shader_type, std::string_view src, std::string &log_out){
    GLuint shader = glCreateShader(shader_type);
    const char *cstr_src = src.data();
    GLint len = src.size();
    glShaderSource(shader, 1, &cstr_src, &len);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if(!ok){
        GLint log_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
        log_out.resize((size_t) log_len);
        glGetShaderInfoLog(shader, log_len, nullptr, log_out.data());
        glDeleteShader(shader);
        return false;
    }

    shader_out = shader;
    return true;
}

bool Shader::load_files(const std::string &vertex_path, const std::string &fragment_path){
    std::string vertex_src = readTextFile(vertex_path);
    std::string fragment_src = readTextFile(fragment_path);
    return load_sources(vertex_src, fragment_src);
}

bool Shader::load_sources(std::string_view vertex_src, std::string_view fragment_src){
    std::string log;
    GLuint vertex, fragment;

    if(!(compile_shader(vertex, GL_VERTEX_SHADER, vertex_src, log))){
        fprintf(stderr, "Error compiling vertex shader:\n%s\n", log.data());
        return false;
    }

    if(!(compile_shader(vertex, GL_FRAGMENT_SHADER, fragment_src, log))){
        fprintf(stderr, "Error compiling fragment shader:\n%s\n",log);
        glDeleteShader(vertex);
        return false;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if(!ok){
        GLint log_len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
        log.resize((size_t) log_len);
        glGetProgramInfoLog(program, log_len, nullptr, log.data());
        fprintf(stderr, "Error linking shader program:\n%s\n", log);
        glDeleteProgram(program);
        return false;
    }
    
    program_ = program;
    uniform_cache_.clear();
    return true;
}

void Shader::use() const{
    glUseProgram((GLuint)program_);
}

int Shader::uniform_location(std::string_view name) const {
    if (!program_) return -1;
    std::string name_str(name);
    auto it = uniform_cache_.find(name_str);
    if (it != uniform_cache_.end()) return it->second;
    int loc = glGetUniformLocation((GLuint)program_, name_str.c_str());
    uniform_cache_.emplace(std::move(name_str), loc);
    return loc;
}

void Shader::set_int(std::string_view name, int v) const {
    const int loc = uniform_location(name);
    if (loc < 0) return;
    glUniform1i((GLint)loc, (GLint)v);
}

void Shader::set_float(std::string_view name, float v) const {
    const int loc = uniform_location(name);
    if (loc < 0) return;
    glUniform1f((GLint)loc, v);
}

void Shader::set_mat4(std::string_view name, const float* mat4) const {
    const int loc = uniform_location(name);
    if (loc < 0) return;
    glUniformMatrix4fv((GLint)loc, 1, GL_FALSE, mat4);
}
