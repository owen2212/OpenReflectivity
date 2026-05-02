#include "screenshot.hpp"

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <vector>

#include <glad/glad.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

bool save_screenshot_png_to(int fb_width, int fb_height, const std::string &path) {
    if (fb_width <= 0 || fb_height <= 0) return false;

    std::vector<unsigned char> pixels(static_cast<size_t>(fb_width) *
                                      static_cast<size_t>(fb_height) * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, fb_width, fb_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    // GL rows run bottom-up, PNG wants top-down
    stbi_flip_vertically_on_write(1);
    const int ok = stbi_write_png(path.c_str(), fb_width, fb_height, 4,
                                  pixels.data(), fb_width * 4);
    if (!ok) {
        std::fprintf(stderr, "Failed to write screenshot to %s\n", path.c_str());
        return false;
    }
    return true;
}

bool save_screenshot_png(int fb_width, int fb_height,
                         const std::string &out_dir, std::string *out_path) {
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    if (ec) {
        std::fprintf(stderr, "Failed to create %s: %s\n",
                     out_dir.c_str(), ec.message().c_str());
        return false;
    }

    std::time_t now = std::time(nullptr);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &now);
#else
    localtime_r(&now, &tm_buf);
#endif
    char name[64];
    std::strftime(name, sizeof(name), "openreflectivity_%Y%m%d_%H%M%S.png", &tm_buf);

    const std::string path = out_dir + "/" + name;
    if (!save_screenshot_png_to(fb_width, fb_height, path)) return false;
    std::printf("Saved screenshot: %s\n", path.c_str());
    std::fflush(stdout);
    if (out_path) *out_path = path;
    return true;
}
