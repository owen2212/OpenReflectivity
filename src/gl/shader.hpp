#ifndef SHADER_HPP
#define SHADER_HPP

#include <string>
#include <string_view>
#include <cstdint>
#include <unordered_map>

class Shader {
    public:
        Shader() = default;
        ~Shader();
        
        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;
        Shader(Shader&&) noexcept;
        Shader& operator=(Shader&&) noexcept;
        
        bool load_files(const std::string &vertex_path, const std::string &fragment_path);
        bool load_sources(std::string_view vertex_src, std::string_view fragment_src);

        void use() const;

        void set_int(std::string_view name, int v) const;
        void set_float(std::string_view name, float v) const;
        void set_vec2(std::string_view name, float x, float y) const;
        void set_vec4(std::string_view name, float x, float y, float z, float w) const;

        uint32_t id() const { return program_; }
        explicit operator bool() const { return program_ != 0; }

    private:
        uint32_t program_ = 0;
        mutable std::unordered_map<std::string, int> uniform_cache_;
        int uniform_location(std::string_view name) const;

};

#endif
