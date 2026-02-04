#ifndef BUFFER_HPP
#define BUFFER_HPP

#include <cstdint>
#include <cstddef>

class Buffer {
    public:
        enum class Target : uint32_t {
            Array = 0x8892,        // GL_ARRAY_BUFFER
            ElementArray = 0x8893, // GL_ELEMENT_ARRAY_BUFFER
            Uniform = 0x8A11       // GL_UNIFORM_BUFFER
        };

        enum class Usage : uint32_t {
            StaticDraw = 0x88E4,  // GL_STATIC_DRAW
            DynamicDraw = 0x88E8, // GL_DYNAMIC_DRAW
            StreamDraw = 0x88E0   // GL_STREAM_DRAW
        };

        Buffer() = default;
        explicit Buffer(Target target);
        ~Buffer();

        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;
        Buffer(Buffer&&) noexcept;
        Buffer& operator=(Buffer&&) noexcept;

        void create(Target target);
        void destroy();

        void bind() const;
        void unbind() const;

        void set_data(const void* data, size_t size, Usage usage);
        void update_data(const void* data, size_t size, size_t offset = 0);

        uint32_t id() const { return id_; }
        explicit operator bool() const { return id_ != 0; }

        Target target() const { return target_; }

    private:
        uint32_t id_ = 0;
        Target target_ = Target::Array;
};

#endif
