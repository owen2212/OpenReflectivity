#ifndef VERTEX_ARRAY_HPP
#define VERTEX_ARRAY_HPP

#include <cstdint>

class VertexArray {
    public:
        VertexArray() = default;
        explicit VertexArray(bool create_now);
        ~VertexArray();

        VertexArray(const VertexArray&) = delete;
        VertexArray& operator=(const VertexArray&) = delete;
        VertexArray(VertexArray&&) noexcept;
        VertexArray& operator=(VertexArray&&) noexcept;

        void create();
        void destroy();

        void bind() const;
        void unbind() const;

        uint32_t id() const { return id_; }
        explicit operator bool() const { return id_ != 0; }

    private:
        uint32_t id_ = 0;
};

#endif
