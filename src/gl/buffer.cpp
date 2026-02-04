#include <glad/glad.h>

#include "buffer.hpp"

Buffer::Buffer(Target target) {
    create(target);
}

Buffer::~Buffer() {
    destroy();
}

Buffer::Buffer(Buffer&& other) noexcept {
    id_ = other.id_;
    target_ = other.target_;
    other.id_ = 0;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        destroy();
        id_ = other.id_;
        target_ = other.target_;
        other.id_ = 0;
    }
    return *this;
}

void Buffer::create(Target target) {
    if (id_) {
        destroy();
    }
    target_ = target;
    glGenBuffers(1, reinterpret_cast<GLuint*>(&id_));
}

void Buffer::destroy() {
    if (id_) {
        glDeleteBuffers(1, reinterpret_cast<const GLuint*>(&id_));
        id_ = 0;
    }
}

void Buffer::bind() const {
    glBindBuffer(static_cast<GLenum>(target_), static_cast<GLuint>(id_));
}

void Buffer::unbind() const {
    glBindBuffer(static_cast<GLenum>(target_), 0);
}

void Buffer::set_data(const void* data, size_t size, Usage usage) {
    bind();
    glBufferData(static_cast<GLenum>(target_), static_cast<GLsizeiptr>(size), data,
                 static_cast<GLenum>(usage));
}

void Buffer::update_data(const void* data, size_t size, size_t offset) {
    bind();
    glBufferSubData(static_cast<GLenum>(target_), static_cast<GLintptr>(offset),
                    static_cast<GLsizeiptr>(size), data);
}
