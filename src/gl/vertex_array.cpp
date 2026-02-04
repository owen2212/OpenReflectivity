#include <glad/glad.h>

#include "vertex_array.hpp"

VertexArray::VertexArray(bool create_now) {
    if (create_now) {
        create();
    }
}

VertexArray::~VertexArray() {
    destroy();
}

VertexArray::VertexArray(VertexArray&& other) noexcept {
    id_ = other.id_;
    other.id_ = 0;
}

VertexArray& VertexArray::operator=(VertexArray&& other) noexcept {
    if (this != &other) {
        destroy();
        id_ = other.id_;
        other.id_ = 0;
    }
    return *this;
}

void VertexArray::create() {
    if (id_) {
        destroy();
    }
    glGenVertexArrays(1, reinterpret_cast<GLuint*>(&id_));
}

void VertexArray::destroy() {
    if (id_) {
        glDeleteVertexArrays(1, reinterpret_cast<const GLuint*>(&id_));
        id_ = 0;
    }
}

void VertexArray::bind() const {
    glBindVertexArray(static_cast<GLuint>(id_));
}

void VertexArray::unbind() const {
    glBindVertexArray(0);
}
