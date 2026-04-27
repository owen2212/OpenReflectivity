#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include <cstdint>

class Texture {
    public:
        enum class Target : uint32_t {
            Texture1D = 0x0DE0,      // GL_TEXTURE_1D
            TextureBuffer = 0x8C2A,  // GL_TEXTURE_BUFFER
        };

        Texture() = default;
        explicit Texture(Target target);
        ~Texture();

        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;
        Texture(Texture&& other) noexcept;
        Texture& operator=(Texture&& other) noexcept;

        void create(Target target);
        void destroy();

        void bind() const;
        void bind_unit(uint32_t unit) const;
        void set_buffer(uint32_t internal_format, uint32_t buffer_id);
        void set_image_1d(int internal_format, int width, uint32_t format,
                          uint32_t type, const void *data);
        void set_parameter(uint32_t pname, int value);

        uint32_t id() const { return id_; }
        Target target() const { return target_; }
        explicit operator bool() const { return id_ != 0; }

    private:
        uint32_t id_ = 0;
        Target target_ = Target::Texture1D;
};

#endif
