#include <glad/glad.h>

#include "texture.hpp"

Texture::Texture(Target target) {
    create(target);
}

Texture::~Texture() {
    destroy();
}

Texture::Texture(Texture&& other) noexcept {
    id_ = other.id_;
    target_ = other.target_;
    other.id_ = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        destroy();
        id_ = other.id_;
        target_ = other.target_;
        other.id_ = 0;
    }
    return *this;
}

void Texture::create(Target target) {
    if (id_) {
        destroy();
    }
    target_ = target;
    glGenTextures(1, reinterpret_cast<GLuint*>(&id_));
}

void Texture::destroy() {
    if (id_) {
        glDeleteTextures(1, reinterpret_cast<const GLuint*>(&id_));
        id_ = 0;
    }
}

void Texture::bind() const {
    glBindTexture(static_cast<GLenum>(target_), static_cast<GLuint>(id_));
}

void Texture::bind_unit(uint32_t unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    bind();
}

void Texture::set_buffer(uint32_t internal_format, uint32_t buffer_id) {
    bind();
    glTexBuffer(static_cast<GLenum>(target_), static_cast<GLenum>(internal_format),
                static_cast<GLuint>(buffer_id));
}

void Texture::set_image_1d(int internal_format, int width, uint32_t format,
                           uint32_t type, const void *data) {
    bind();
    glTexImage1D(static_cast<GLenum>(target_), 0, internal_format, width, 0,
                 static_cast<GLenum>(format), static_cast<GLenum>(type), data);
}

void Texture::set_parameter(uint32_t pname, int value) {
    bind();
    glTexParameteri(static_cast<GLenum>(target_), static_cast<GLenum>(pname), value);
}
