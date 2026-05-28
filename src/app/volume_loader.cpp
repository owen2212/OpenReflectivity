#include "volume_loader.hpp"

#include <cctype>
#include <exception>
#include <filesystem>
#include <utility>

VolumeLoader::~VolumeLoader() {
    if (worker_.joinable()) worker_.join();
}

bool VolumeLoader::request(const std::string &path, const std::string &site_id) {
    if (busy_.load()) return false;
    if (worker_.joinable()) worker_.join();

    busy_.store(true);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        result_.reset();
    }
    worker_ = std::thread([this, path, site_id]() {
        LoadedVolume vol;
        vol.path = path;
        try {
            rsl::RadarData radar_data(path, site_id);
            vol.site = radar_data.site_info();
            for (const ProductDescriptor &desc : product_table()) {
                try {
                    vol.products[product_index(desc.type)] =
                        radar_data.get_product(desc.type);
                } catch (const std::exception &) {
                    // missing moments stay empty, the UI disables them
                }
            }
            bool any = false;
            for (const rsl::Product &p : vol.products) any = any || !p.scans.empty();
            if (!any) vol.error = "No decodable moments in " + path;
        } catch (const std::exception &ex) {
            vol.error = ex.what();
        }
        std::lock_guard<std::mutex> lock(mutex_);
        result_ = std::move(vol);
        busy_.store(false);
    });
    return true;
}

std::optional<LoadedVolume> VolumeLoader::poll() {
    std::optional<LoadedVolume> out;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (result_) {
            out = std::move(result_);
            result_.reset();
        }
    }
    if (out && worker_.joinable()) worker_.join();
    return out;
}

std::string infer_site_id(const std::string &path, const std::string &fallback) {
    const std::string name = std::filesystem::path(path).filename().string();
    if (name.size() >= 4) {
        std::string id;
        for (size_t i = 0; i < 4; ++i) {
            const unsigned char c = static_cast<unsigned char>(name[i]);
            if (!std::isalpha(c)) return fallback;
            id.push_back(static_cast<char>(std::toupper(c)));
        }
        return id;
    }
    return fallback;
}
